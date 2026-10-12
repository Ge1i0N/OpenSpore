#include "states/statemgr.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>
#include <string>
#include <utility>

#include "engine/console.h"
#include "engine/diag.h"
#include "engine/fs.h"
#include "engine/log.h"
#include "engine/pkg.h"
#include "render/device.h"
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
};

Loader g_loader;
Manager* g_mgr = nullptr;

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
            if (pk.index_parsed()) {
                ++g_loader.pkgs_fixed;
                g_loader.pkg_lines.emplace_back(
                    name, "kind=" + std::to_string(pk.kind()) + " count=" + std::to_string(pk.header().count) +
                          " recs, idx=0x" +
                          [&] {
                              char b[16];
                              _snprintf_s(b, sizeof(b), _TRUNCATE, "%x", pk.header().index_offset);
                              return std::string(b);
                          }());
            } else {
                ++g_loader.pkgs_varwidth;
                g_loader.pkg_lines.emplace_back(name, "KIND-UNKNOWN (kind=" + std::to_string(pk.kind()) + ")");
            }
            // LOG_ONCE: 109 packages share this call site, and the per-site ledger would
            // hide 100 of them ("x9, further repeats suppressed").
            LOG_ONCE_INFO("loader", "%s", pk.describe().c_str());
        }
    }
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
    // Real packages, read from EA's shipped data through the reversed header.
    ov.text_fmt(pad, y, kAccent, "packages: %d parsed, %d kind-unknown, %d invalid", g_loader.pkgs_fixed,
                g_loader.pkgs_varwidth, g_loader.pkgs_invalid);
    y += ov.font_height();
    for (size_t i = 0; i < g_loader.pkg_lines.size() && y < (float)h - ov.font_height(); ++i) {
        ov.text_fmt(pad, y, kDim, "  %s", clip(g_loader.pkg_lines[i].first + "  " + g_loader.pkg_lines[i].second).c_str());
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
