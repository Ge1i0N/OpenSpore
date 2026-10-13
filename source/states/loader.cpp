#include "states/statemgr.h"

#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "engine/console.h"
#include "engine/diag.h"
#include "engine/fs.h"
#include "engine/gmdl.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"
#include "engine/raster.h"
#include "render/device.h"
#include "render/imgfile.h"
#include "render/mesh.h"
#include "render/overlay.h"

namespace os::states {
namespace {

// The Loader state: our stand-in for the original's loading screen. Its name is chosen to
// match the reversed vocabulary rather than invented: the GA build carries
// 'AppModePluginLoader' (0x01413814), 'UIWorld_Loading' (0x01418274),
// '*Loading...*' (0x013ffae2) and 'EventLoadScreenBufferPercentage' (0x01446c18),
// so "Loader" is the family name the engine itself uses for this mode.
struct Loader {
    f32 progress = 0.f;
    f32 elapsed = 0.f;
    bool done = false;
    u32 frames = 0;
    std::string status = "initialising";
    // Real packages, discovered through the reversed search paths (not hardcoded).
    std::vector<std::pair<std::string, std::string>> pkg_lines;  // (name, describe())
    u32 pkgs_fixed = 0, pkgs_varwidth = 0, pkgs_invalid = 0;
    // Global inventory across every package: type -> count, and how many ids resolved.
    std::vector<std::pair<std::string, u32>> top_types;
    u64 total_records = 0, resolved_instances = 0, total_bytes = 0;
    // One real decoded game asset, uploaded as a texture and drawn (proof the pipeline ends in
    // pixels on our own renderer).
    IDirect3DTexture9* tex = nullptr;
    int tex_w = 0, tex_h = 0;
    std::string tex_name;
    std::string tex_format;  // "raster DXT1" etc., or "png"
};

Loader g_loader;
Manager* g_mgr = nullptr;

// The decoded meshes live OUTSIDE Loader on purpose: Loader is reset by copy-assignment
// (`g_loader = Loader{}`), which would copy raw D3D pointers and double-free them.
struct Slot {
    render::Mesh mesh;
    IDirect3DTexture9* tex = nullptr;
    std::string name;
    u32 tris = 0;
    u32 verts = 0;
};
Slot g_slots[4];
int g_slot_count = 0;
constexpr u32 kGmdlType = 0x00E6BCE5;
constexpr u32 kRasterType = 0x2F4E681C;

// Load one real gmdl model and its paired texture (same instance id) into a slot. Returns false if
// the record or the mesh cannot be used.
bool load_slot(Slot& s, const std::string& pkgpath, u32 rec) {
    pkg::Package pk;
    if (!pk.open(pkgpath)) return false;
    std::vector<u8> bytes;
    if (!pk.read_resource_decoded(rec, bytes)) return false;
    gmdl::Mesh m = gmdl::parse(bytes.data(), bytes.size());
    if (!m.ok || m.indices.size() < 3) return false;
    if (m.vertex_count < 256 || m.vertex_count > 120000) return false;
    if (!s.mesh.create(render::device().d3d(), m.positions.data(), m.uvs.data(), m.vertex_count,
                       m.indices.data(), (u32)m.indices.size()))
        return false;
    const auto& r = pk.records()[rec];
    const u32 inst = r.instance();
    s.name = naming::resource_path(r.type(), r.group(), r.instance());
    s.tris = s.mesh.triangle_count();
    s.verts = m.vertex_count;

    // paired texture: a `raster` with the same instance id
    if (s.tex) { s.tex->Release(); s.tex = nullptr; }
    for (u32 n = 0; n < pk.records().size(); ++n) {
        if (pk.records()[n].type() != kRasterType) continue;
        if (pk.records()[n].instance() != inst) continue;
        std::string rname = naming::resource_path(pk.records()[n].type(), pk.records()[n].group(),
                                                  pk.records()[n].instance());
        std::string low = rname;
        for (auto& c : low) c = (char)std::tolower((unsigned char)c);
        if (low.find("normal") != std::string::npos || low.find("specular") != std::string::npos
            || low.find("_mask") != std::string::npos)
            continue;
        std::vector<u8> tb;
        if (!pk.read_resource_decoded(n, tb)) continue;
        raster::Texture rt = raster::decode(tb.data(), tb.size());
        if (!rt.ok || rt.width < 32 || rt.height < 32) continue;
        s.tex = render::device().create_texture_bgra(rt.bgra.data(), rt.width, rt.height);
        if (s.tex) break;
    }
    LOG_INFO("loader", "model slot: %s (%u verts, %u tris)%s", s.name.c_str(), s.verts, s.tris,
             s.tex ? " + texture" : " (no texture)");
    return true;
}

constexpr u32 kBg = 0xFF101418;      // dark slate
constexpr u32 kBar = 0xFF2A6E8F;
constexpr u32 kBarFill = 0xFF63C7DA;
constexpr u32 kText = 0xFFE8F0F4;
constexpr u32 kDim = 0xFF9AA8B0;
constexpr u32 kAccent = 0xFFFFC850;

void step_status() {
    // Walk a fixed list so the screen visibly progresses without any EA asset present.
    static const char* kSteps[] = {"boot",
                                   "diagnostics",
                                   "command line",
                                   "search paths",
                                   "renderer",
                                   "font atlas",
                                   "packages (missing - loose only)",
                                   "ready"};
    size_t n = sizeof(kSteps) / sizeof(kSteps[0]);
    size_t i = (size_t)(g_loader.progress * (n - 1));
    if (i >= n) i = n - 1;
    g_loader.status = kSteps[i];
}

void loader_enter() {
    g_loader = Loader{};
    g_loader.status = "boot";
    LOG_INFO("loader", "enter Loader state");
    step_status();
    // Scan the real search paths for packages and open each one through the proven header
    // validator. This is the first place the reimplementation touches EA's shipped data.
    //
    // Dedupe by canonical path: the dev search list contains BOTH `Data=<exe>\Data` and
    // `cwd=<exe>`, so a recursive scan visits every file twice. Measured before the fix:
    // 62 "opened" lines for 31 distinct files (tools/log_dupes.py). After dedupe the totals
    // are 31 fixed-stride + 26 var-width = 57, which is exactly what the offline analysis
    // (tools/pkg_records.py over the same tree) reports - two independent paths to the
    // same number, which is the confirmation we want rather than a log that merely looks right.
    std::set<std::string> seen;
    std::map<u32, u32> type_counts;
    for (auto& sp : fs::search_paths()) {
        auto found = pkg::find_packages(sp.dir.string());
        if (found.empty()) continue;
        for (auto& p : found) {
            std::error_code ec;
            std::string key = std::filesystem::weakly_canonical(p, ec).string();
            if (ec) key = p;
            if (!seen.insert(key).second) continue;  // already opened via another root
            pkg::Package pk;
            pk.open(p);
            std::string name = std::filesystem::path(p).filename().string();
            if (!pk.ok()) {
                ++g_loader.pkgs_invalid;
                g_loader.pkg_lines.emplace_back(name, "INVALID: " + pk.why());
                continue;
            }
            if (pk.index_parsed()) ++g_loader.pkgs_fixed;
            else ++g_loader.pkgs_varwidth;
            for (const auto& r : pk.records()) {
                type_counts[r.type()]++;
                ++g_loader.total_records;
                g_loader.total_bytes += r.mem_size();
                if (naming::instance_name(r.instance())) ++g_loader.resolved_instances;
            }
            g_loader.pkg_lines.emplace_back(
                name, "kind=" + std::to_string(pk.kind()) + " count=" + std::to_string(pk.header().count) + " recs");
            // LOG_ONCE: 109 packages share this call site, and the per-site ledger would
            // hide 100 of them ("x9, further repeats suppressed").
            LOG_ONCE_INFO("loader", "%s", pk.describe().c_str());
        }
    }
    // Global inventory: which resource types exist in the whole game, named.
    std::vector<std::pair<u32, u32>> tv(type_counts.begin(), type_counts.end());
    std::sort(tv.begin(), tv.end(), [](auto& a, auto& b) { return a.second > b.second; });
    for (size_t i = 0; i < tv.size() && i < 14; ++i) {
        const char* tn = naming::type_name(tv[i].first);
        if (!tn) tn = pkg::type_id_name(tv[i].first);
        char line[48];
        _snprintf_s(line, sizeof(line), _TRUNCATE, "%-16s", tn ? tn : "(unnamed)");
        g_loader.top_types.emplace_back(line, tv[i].second);
    }
    LOG_ONCE_INFO("loader", "inventory: %llu resources, %llu decoded bytes, %llu instances named (%s)",
                  (unsigned long long)g_loader.total_records, (unsigned long long)g_loader.total_bytes,
                  (unsigned long long)g_loader.resolved_instances, naming::stats().c_str());

    // Collect real models we can draw (gmdl that parses) and load the first FOUR into a 2x2 grid,
    // each with its paired texture - a small gallery of real Spore content on our own renderer.
    g_slot_count = 0;
    for (auto& s : g_slots) { s.mesh.shutdown(); if (s.tex) { s.tex->Release(); s.tex = nullptr; } s.name.clear(); }
    const bool have_dev = render::device().d3d() != nullptr;
    if (have_dev) {
        std::vector<std::tuple<std::string, u32, u32>> cands;  // (pkg, rec, instance)
        for (auto& sp : fs::search_paths()) {
            if (cands.size() >= 48) break;
            for (auto& p : pkg::find_packages(sp.dir.string())) {
                if (cands.size() >= 48) break;
                pkg::Package pk;
                if (!pk.open(p)) continue;
                for (u32 n = 0; n < pk.records().size(); ++n) {
                    if (cands.size() >= 48) break;
                    if (pk.records()[n].type() != kGmdlType) continue;
                    std::vector<u8> bytes;
                    if (!pk.read_resource_decoded(n, bytes)) continue;
                    gmdl::Mesh m = gmdl::parse(bytes.data(), bytes.size());
                    if (!m.ok || m.indices.size() < 3) continue;
                    if (m.vertex_count < 256 || m.vertex_count > 120000) continue;
                    cands.emplace_back(p, n, pk.records()[n].instance());
                }
            }
        }
        // Load four with DISTINCT instance ids, so the grid shows four different models.
        std::set<u32> used;
        for (auto& c : cands) {
            if (g_slot_count >= 4) break;
            if (used.count(std::get<2>(c))) continue;
            if (load_slot(g_slots[g_slot_count], std::get<0>(c), std::get<1>(c))) {
                used.insert(std::get<2>(c));
                ++g_slot_count;
            }
        }
    }
    if (g_slot_count == 0) LOG_WARN("loader", "no drawable gmdl mesh found");

    // Put one real game image on screen. This is the pipeline's end: DBPF index -> RefPack ->
    // type/instance name -> decode -> D3D9 texture. Two decoders, native first:
    //   1. `raster` (0x2f4e681c) - the engine's OWN texture format (DXT1/DXT5/raw). Preferred.
    //   2. `png`    - a decoded image resource, via GDI+.
    // Skipped in headless mode (no device to upload to).
    for (auto& sp : fs::search_paths()) {
        if (g_loader.tex || !render::device().d3d()) break;
        for (auto& p : pkg::find_packages(sp.dir.string())) {
            if (g_loader.tex) break;
            pkg::Package pk;
            if (!pk.open(p)) continue;
            for (u32 n = 0; n < pk.records().size(); ++n) {
                if (pk.records()[n].type() != kRasterType) continue;
                std::vector<u8> bytes;
                if (!pk.read_resource_decoded(n, bytes)) continue;
                raster::Texture rt = raster::decode(bytes.data(), bytes.size());
                if (!rt.ok) continue;
                if (rt.width < 128 || rt.height < 128 || rt.width > 2048 || rt.height > 2048) continue;
                // Normal maps in this game are stored UNCOMPRESSED (raw BGRA), so skip raw here and
                // prefer a DXT-compressed (colour/diffuse) texture for the demo.
                if (rt.format == "raw") continue;
                // Prefer a visually DENSE texture: many rasters are sparse decal atlases on a
                // transparent background (they decode correctly but look like nothing). Sample the
                // alpha and skip textures that are mostly transparent, so the demo shows real art.
                size_t opaque = 0, sampled = 0;
                for (size_t i = 3; i < rt.bgra.size(); i += 4 * 37) {
                    if (rt.bgra[i] > 16) ++opaque;
                    ++sampled;
                }
                if (sampled && opaque * 100 / sampled < 35) continue;
                // Also skip non-albedo maps (normal/specular/mask) - they decode fine but a normal
                // map just looks like flat purple. Prefer a diffuse texture for the demo.
                std::string rname = naming::resource_path(pk.records()[n].type(), pk.records()[n].group(),
                                                          pk.records()[n].instance());
                std::string low = rname;
                for (auto& c : low) c = (char)std::tolower((unsigned char)c);
                if (low.find("normal") != std::string::npos || low.find("specular") != std::string::npos
                    || low.find("_mask") != std::string::npos)
                    continue;
                IDirect3DTexture9* tex = render::device().create_texture_bgra(rt.bgra.data(), rt.width, rt.height);
                if (!tex) continue;
                g_loader.tex = tex;
                g_loader.tex_w = rt.width;
                g_loader.tex_h = rt.height;
                g_loader.tex_name = rname;
                g_loader.tex_format = "raster " + rt.format;
                LOG_ONCE_INFO("loader", "showing real raster %s (%dx%d %s, %zu decoded bytes) from %s",
                              g_loader.tex_name.c_str(), rt.width, rt.height, rt.format.c_str(), bytes.size(),
                              std::filesystem::path(p).filename().string().c_str());
                break;
            }
        }
    }
    // Fall back to a png if no raster was usable.
    for (auto& sp : fs::search_paths()) {
        if (g_loader.tex || !render::device().d3d()) break;
        for (auto& p : pkg::find_packages(sp.dir.string())) {
            if (g_loader.tex) break;
            pkg::Package pk;
            if (!pk.open(p)) continue;
            for (u32 n = 0; n < pk.records().size(); ++n) {
                const char* tn = naming::type_name(pk.records()[n].type());
                if (!tn || std::string(tn) != "png") continue;
                std::vector<u8> bytes, bgra;
                if (!pk.read_resource_decoded(n, bytes)) continue;
                int w = 0, h = 0;
                if (!image::decode_from_memory(bytes.data(), bytes.size(), bgra, w, h)) continue;
                if (w < 96 || h < 96 || w > 2048 || h > 2048) continue;  // icons are too small to show
                IDirect3DTexture9* tex = render::device().create_texture_bgra(bgra.data(), w, h);
                if (!tex) continue;
                g_loader.tex = tex;
                g_loader.tex_w = w;
                g_loader.tex_h = h;
                g_loader.tex_format = "png";
                g_loader.tex_name = naming::resource_path(pk.records()[n].type(), pk.records()[n].group(),
                                                          pk.records()[n].instance());
                LOG_ONCE_INFO("loader", "showing real asset %s (%dx%d, %zu decoded bytes) from %s",
                              g_loader.tex_name.c_str(), w, h, bytes.size(),
                              std::filesystem::path(p).filename().string().c_str());
                break;
            }
        }
    }
    if (!g_loader.tex) LOG_WARN("loader", "no drawable png found to display");

    LOG_INFO("loader", "packages: %d index-parsed, %d kind-unknown, %d invalid", g_loader.pkgs_fixed,
             g_loader.pkgs_varwidth, g_loader.pkgs_invalid);
}

void loader_exit() { LOG_INFO("loader", "exit Loader state after %u frames, %.2fs", g_loader.frames, g_loader.elapsed); }

void loader_tick(f32 dt) {
    g_loader.elapsed += dt;
    if (g_loader.done) return;
    // Ramp to 1.0 over ~2.5s purely to demonstrate the loop is alive and vsync'd.
    g_loader.progress += dt / 2.5f;
    if (g_loader.progress >= 1.f) {
        g_loader.progress = 1.f;
        g_loader.done = true;
    }
    step_status();
}

void loader_render() {
    using namespace os::render;
    Device& dev = device();
    Overlay& ov = overlay();
    if (!dev.d3d()) return;
    // Correct order: open the scene, THEN clear/draw, then present. The first version
    // cleared outside a scene, which D3D9 silently drops.
    if (!dev.begin_frame()) return;
    int w = dev.width(), h = dev.height();
    dev.clear(kBg);
    // The decoded gmdl meshes: draw up to four real Spore models in a 2x2 grid on the right side,
    // each with its own texture, before the 2D overlay (so the text sits on top). draw() saves and
    // restores the viewport itself.
    int cell = 0, gx = 0, gy = 0;
    if (g_slot_count > 0) {
        int region = (w < h ? w : h) * 70 / 100;
        cell = region / 2;
        if (cell < 120) cell = 120;
        gx = w - region - 12;
        gy = 44;
        if (gx < 8) gx = 8;
        for (int i = 0; i < g_slot_count; ++i) {
            int cx = gx + (i % 2) * cell;
            int cy = gy + (i / 2) * cell;
            g_slots[i].mesh.draw(dev.d3d(), cx, cy, cell, cell, g_loader.elapsed * 0.6f, g_slots[i].tex);
        }
    }
    ov.begin(w, h);

    // Title + identity block: this is the "log the state, not the symptom" rule applied
    // visually - the screen shows the real device/args state, not a decorative mock-up.
    float pad = 24.f;
    ov.text_fmt(pad, pad, kAccent, "OpenSpore - Evo reversed");
    float y = pad + (float)ov.font_height() + 8.f;
    // Clip every readout line to the screen width: the search-path dump alone is longer
    // than 1280 px, and an unclipped line ran off the window in the first captured frame.
    const float max_chars = (float)(w - (int)pad * 2) / 8.0f;  // 8 px = the Consolas advance here
    auto clip = [&](std::string s) {
        size_t n = (size_t)max_chars;
        if (n < 4) n = 4;
        if (s.size() > n) s = s.substr(0, n - 3) + "...";
        return s;
    };
    // Clip a line drawn at x to the space that actually remains to its right (the mesh/asset
    // captions sit near the right edge and used to run off the window).
    auto clip_at = [&](float x, std::string s) {
        size_t n = (size_t)(((float)w - x - pad) / 8.0f);
        if (n < 4) n = 4;
        if (s.size() > n) s = s.substr(0, n - 3) + "...";
        return s;
    };
    auto fmt_mode = [&](os::render::Device& d) {
        char b[192];
        _snprintf_s(b, sizeof(b), _TRUNCATE, "mode: %s %dx%d vsync=%d  desktop=%dx%d",
                    d.windowed() ? "windowed" : "fullscreen", d.width(), d.height(), (int)d.vsync(),
                    (int)d.desktop_mode().Width, (int)d.desktop_mode().Height);
        return std::string(b);
    };
    ov.text(pad, y, kDim, clip(fmt_mode(dev)));
    y += ov.font_height();
    ov.text_fmt(pad, y, kDim, "state: %s", g_mgr ? g_mgr->current_name().c_str() : "?");
    y += ov.font_height();
    ov.text(pad, y, kDim, clip(fs::dump_search_list()));
    y += ov.font_height() * 1.5f;

    // Progress bar
    float bw = (float)w - pad * 2.f;
    float bh = 18.f;
    float bx = pad, by = y;
    ov.rect(bx, by, bw, bh, kBar);
    ov.rect(bx, by, bw * g_loader.progress, bh, kBarFill);
    y = by + bh + 8.f;
    ov.text_fmt(pad, y, kText, "%3d%%  %s", (int)(g_loader.progress * 100.f), g_loader.status.c_str());
    y += ov.font_height();
    ov.text_fmt(pad, y, kDim, "frames: %u   t=%.2fs   log lines: %llu", g_loader.frames, g_loader.elapsed,
                (unsigned long long)log::lines_written());
    y += ov.font_height();
    ov.text_fmt(pad, y, kDim, "console: type 'help', then 'state -list'");
    y += ov.font_height() * 1.5f;
    // Captions under each grid cell: the real model name and size, on OUR renderer. Clip to the
    // cell width (a long name would otherwise spill into the neighbouring cell).
    const size_t cell_chars = (size_t)((cell - 12) / 8);
    auto clip_cell = [&](std::string s) {
        size_t n = cell_chars < 8 ? 8 : cell_chars;
        if (s.size() > n) s = s.substr(0, n - 3) + "...";
        return s;
    };
    for (int i = 0; i < g_slot_count; ++i) {
        float cx = (float)(gx + (i % 2) * cell);
        float cy = (float)(gy + (i / 2) * cell + cell) + 2.f;
        ov.text(cx, cy, kAccent,
                clip_cell(g_slots[i].name + " (" + std::to_string(g_slots[i].verts) + "v)"));
    }
    y += ov.font_height() * 1.5f;
    // Global inventory: what the game actually contains, by name.
    ov.text_fmt(pad, y, kAccent, "inventory: %llu resources, %.2f GB decoded, %llu named",
                (unsigned long long)g_loader.total_records, (double)g_loader.total_bytes / 1e9,
                (unsigned long long)g_loader.resolved_instances);
    y += ov.font_height();
    for (size_t i = 0; i < g_loader.top_types.size() && y < (float)h - ov.font_height(); ++i) {
        ov.text_fmt(pad, y, kDim, "  %s %7u", g_loader.top_types[i].first.c_str(), g_loader.top_types[i].second);
        y += ov.font_height();
    }

    ov.end();
    dev.end_frame();
    ++g_loader.frames;
}

void loader_reload(const char* /*path*/) {
    g_loader.progress = 0.f;
    g_loader.done = false;
    LOG_INFO("loader", "reload requested - restarting progress ramp");
}

}  // namespace

void install_loader(Manager& mgr) {
    g_mgr = &mgr;
    State st;
    st.name = "Loader";
    st.on_enter = loader_enter;
    st.on_exit = loader_exit;
    st.on_tick = loader_tick;
    st.on_render = loader_render;
    st.on_reload = loader_reload;
    if (!mgr.reg(std::move(st))) {
        LOG_ERROR("loader", "could not register the Loader state");
        return;
    }
    LOG_INFO("loader", "Loader state registered");
}

}  // namespace os::states
