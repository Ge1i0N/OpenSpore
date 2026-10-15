#include "states/uipresent.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <map>
#include <set>
#include <string>

#include "engine/appstates.h"  // the state's own script, which names the screen
#include "engine/appmodes.h"
#include "engine/gge_intro.h"
#include "engine/input.h"      // the pointer, latched once per frame
#include "engine/log.h"
#include "engine/naming.h"     // hash_lower: a layout NAME becomes a resource INSTANCE
#include "engine/fs.h"
#include "engine/pkg.h"
#include "engine/spui.h"
#include "engine/spui_drawable.h"
#include "engine/text.h"
#include "render/device.h"
#include "render/imgfile.h"
#include "render/overlay.h"
#include "states/statemgr.h"

namespace os::states {
namespace {

// The last `why` we logged, so the choice is announced once per change and not 60 times a second.
// Kept here rather than in the caller because every state that draws a layout shares this path.
std::string g_why_logged;

struct TextureKey {
    u32 instance, type, group;
    bool operator<(const TextureKey& o) const {
        if (type != o.type) return type < o.type;
        if (group != o.group) return group < o.group;
        return instance < o.instance;
    }
};

struct CachedTexture {
    IDirect3DTexture9* texture = nullptr;
    int width = 0, height = 0;
    bool attempted = false;
};

std::map<TextureKey, CachedTexture> g_textures;

struct ResourceLocation {
    std::string path;
    u32 record = 0;
};

std::map<TextureKey, ResourceLocation> g_resources;
bool g_resources_indexed = false;

void index_external_resources() {
    if (g_resources_indexed) return;
    g_resources_indexed = true;
    std::set<std::string> seen;
    u32 packages = 0, records = 0, pngs = 0;
    for (const auto& sp : os::fs::search_paths()) {
        for (const auto& path : os::pkg::find_packages(sp.dir.string())) {
            if (!seen.insert(path).second) continue;
            os::pkg::Package package;
            if (!package.open(path)) continue;
            ++packages;
            for (u32 n = 0; n < package.records().size(); ++n) {
                const auto& record = package.records()[n];
                ++records;
                if (record.type() != 0x2f7d0004u) continue;
                ++pngs;
                g_resources.emplace(TextureKey{record.instance(), record.type(), record.group()},
                                     ResourceLocation{path, n});
            }
        }
    }
    LOG_INFO("ui", "drawable resource index: %u packages, %u records, %u PNG entries, %zu keys",
             packages, records, pngs, g_resources.size());
}

CachedTexture& load_external(const os::spui::Layout& lay, u16 ref) {
    static CachedTexture missing;
    if (ref == 0xffff || ref >= lay.external_resources.size()) return missing;
    const auto& e = lay.external_resources[ref];
    const TextureKey key{e.instance, e.type, e.group};
    CachedTexture& cached = g_textures[key];
    if (cached.attempted) return cached;
    cached.attempted = true;
    index_external_resources();
    const auto location = g_resources.find(key);
    if (location != g_resources.end()) {
        os::pkg::Package package;
        if (package.open(location->second.path)) {
            std::vector<u8> bytes, bgra;
            int width = 0, height = 0;
            if (package.read_resource_decoded(location->second.record, bytes) &&
                image::decode_from_memory(bytes.data(), bytes.size(), bgra, width, height)) {
                cached.texture = render::device().create_texture_bgra(bgra.data(), width, height);
                cached.width = width;
                cached.height = height;
                LOG_INFO("ui", "drawable PNG: %s rec[%u] instance=0x%08X %dx%d",
                         std::filesystem::path(location->second.path).filename().string().c_str(),
                         location->second.record, e.instance, width, height);
                return cached;
            }
            LOG_WARN("ui", "drawable PNG decode/read failed: %s rec[%u] instance=0x%08X",
                     std::filesystem::path(location->second.path).filename().string().c_str(),
                     location->second.record, e.instance);
        }
    }
    LOG_WARN("ui", "drawable external unresolved: instance=0x%08X type=0x%08X group=0x%08X",
             e.instance, e.type, e.group);
    return cached;
}

}  // namespace

const os::spui::Layout* requested_by_state(std::string& why) {
    auto& rd = os::spui::instance();
    const std::string st = manager().current_name();
    if (st.empty()) {
        why = "no state is current";
        return nullptr;
    }
    const auto* gs = os::appstates::instance().find(st);
    if (!gs) {
        // One of OUR states (Title, Loader), not a script-defined game state, so it has no opinion.
        why = "state '" + st + "' is ours, not the game's script";
        return nullptr;
    }
    for (const auto& u : os::appstates::ui_commands(*gs)) {
        if (u.unload) continue;
        char b[200];
        if (const auto* L = rd.find_by_name(u.name)) {
            std::snprintf(b, sizeof(b), "state '%s' asked for '%s'", st.c_str(), u.name.c_str());
            why = b;
            return L;
        }
        // Asked for and not present. Logged once per distinct sentence by present_ui_screen, so a
        // per-frame call cannot flood the console log.
        std::snprintf(b, sizeof(b),
                      "state '%s' asked for '%s' (instance 0x%08X) - not readable",
                      st.c_str(), u.name.c_str(), naming::hash_lower(u.name.c_str()));
        why = b;
    }
    if (const auto* binding = os::appmodes::primary_ui(gs->mode.c_str())) {
        const auto* layout = rd.find(binding->instance);
        why = "mode '" + gs->mode + "' -> " + binding->layout + "; " + binding->limitation;
        return layout;
    }
    why = "state '" + st + "' names no layout";
    return nullptr;
}

// What a click HITS. Reports the widget's identity as the data gives it: control id, command id, and
// the caption resolved through the locale tables.
//
    // The ControlID-to-state bridge below is our reconstructed launcher behavior. General CommandID
    // dispatch and WinProc behavior remain unimplemented, independently of the now-complete framing.
void dispatch_click(const os::spui::Layout& lay, int vw, int vh) {
    const os::spui::Window* w = hit_test(lay, (float)input::x(), (float)input::y(), (float)vw,
                                         (float)vh);
    if (!w) {
        LOG_INFO("ui", "click at (%d,%d) hit NO widget of %s", input::x(), input::y(),
                 lay.name().c_str());
        return;
    }
    std::string cap;
    if (!w->caption.empty()) {
        cap = w->caption;
    } else if (w->caption_key_seen) {
        cap = os::text::instance().get(w->caption_table, w->caption_key);
    }
    cap = os::text::to_display_ascii(cap);
    char idpart[96];
    std::snprintf(idpart, sizeof(idpart), "control=0x%08X command=0x%08X", w->control_id,
                  w->command_id);

    // THE BRIDGE, AND WHY IT IS EVIDENCE RATHER THAN AN INVENTION. `spui -xref` measured that a
    // widget's CONTROL ID is sometimes exactly a STATE ID from the game's own script - and the three
    // independent confirmations are what make it real, not the numeric coincidence:
    //   * the layout holding them is the one the script NAMES (`uilayout -load E3LaunchScreen`);
    //   * control 0x02CCDA9C == GalaxyGameEntry's script id, and its caption reads "GGE";
    //   * 0x022D9E01/02/03/05/10 and 0xF12178D3/0x679D892F carry the captions "Creature Editor",
    //     "Building Editor", "Vehicle Editor", "Flora Editor", "Terrain Editor", "UFO Editor",
    //     "Anim Viewer" - the state names, in the game's own words.
    // Expected by chance across 4,187 distinct control ids and 60 states: <0.001 hits. Observed: 9
    // (discounting 0x00000001, which is the block-scoped low-id trap). So a widget in the launch
    // screen CARRIES the id of the state it enters, and a click can drive the state machine WITHOUT
    // reversing the command vocabulary - which is the gate we could not otherwise open.
    const auto* target = (lay.instance == 0xe932bd44u && manager().current_name() == "DevLauncher" &&
                         w->control_id >= 0x1000 && w->command_id == 0x00e1132f)
                             ? appstates::instance().find_by_id(w->control_id) : nullptr;
    if (target && !target->name.empty()) {
        LOG_INFO("ui", "click at (%d,%d) -> \"%s\" %s  => ENTER STATE '%s' (control id == state id)",
                 input::x(), input::y(), cap.empty() ? "-" : cap.c_str(), idpart,
                 target->name.c_str());
        manager().request_goto(target->name);
        return;
    }
    if (cap.empty()) {
        LOG_INFO("ui", "click at (%d,%d) -> %s  (no caption; %s)", input::x(), input::y(),
                 w->area.describe().c_str(), idpart);
    } else {
        LOG_INFO("ui", "click at (%d,%d) -> \"%s\"  %s  (%s)", input::x(), input::y(), cap.c_str(),
                 w->area.describe().c_str(), idpart);
    }
}

int present_spui_layout(const os::spui::Layout& lay, const std::string& why,
                        IDirect3DTexture9* bg, int bg_w, int bg_h) {
    using namespace os::render;
    Device& dev = device();
    Overlay& ov = overlay();
    if (!dev.d3d()) return -1;
    if (!dev.begin_frame()) return -1;
    const int w = dev.width(), h = dev.height();
    dev.clear(0xFF0A0C10);
    ov.begin(w, h);
    if (bg) ov.image(0.f, 0.f, (float)w, (float)h, bg, bg_w, bg_h, false);

    if (why != g_why_logged) {
        LOG_INFO("ui", "screen: %s -> %s", why.c_str(), lay.name().c_str());
        g_why_logged = why;
    }

    // Design resolution from the layout itself, through the SAME accessor the hit-test uses, so a
    // click can never land beside the box the user was aiming at.
    float dw = 0.f, dh = 0.f;
    lay.design_resolution(dw, dh);
    const float sx = dw > 1.f ? (float)w / dw : 1.f;
    const float sy = dh > 1.f ? (float)h / dh : 1.f;

    char head[160];
    std::snprintf(head, sizeof(head), "spui layout: %s  scaled x%.3f / x%.3f",
                  lay.name().c_str(), sx, sy);
    ov.text_fmt(48.f, 22.f, 0xFF8FB8D8, "%s", head);
    ov.text_fmt(48.f, 38.f, 0xFF6E8FA8, "chosen because: %s", why.c_str());

    size_t drawn = 0;
    // Parent before its serialized Children, with ancestor visibility applied by paint_order().
    const std::vector<size_t> order = os::spui::paint_order(lay);
    const auto* hovered = os::spui::hit_test(lay, (float)input::x(), (float)input::y(), (float)w, (float)h);
    for (size_t oi : order) {
        const auto& win = lay.windows[oi];
        if (!win.has_area) continue;
        const float x = win.placed_area.left * sx, y = win.placed_area.top * sy;
        const float bw = win.placed_area.w() * sx, bh = win.placed_area.h() * sy;
        if (x + bw < 0.f || y + bh < 0.f || x > (float)w || y > (float)h) continue;
        const auto clip = os::spui::inherited_clip(lay, oi, {0, 0, (float)w/sx, (float)h/sy});
        if (!clip.valid() || !ov.set_clip(clip.left*sx, clip.top*sy, clip.right*sx, clip.bottom*sy)) continue;
        // The window's own colour, used AS STORED including its alpha. Forcing alpha to 0xFF here
        // (which an earlier revision did) turned the layout into opaque slabs: the shipped values
        // carry meaningful transparency (BackgroundColor 0x007F7F7F is a fully transparent gray,
        // ShadeColor 0xFFFFFFFF is opaque white). Transparency is part of the data.
        const u32 cbg = win.bg_color;
        const f32 x2 = x + bw, y2 = y + bh;
        if ((cbg >> 24) != 0) ov.rect(x, y, bw, bh, cbg);
        // Host input -> drawable state is our adapter; the frame selection and geometry are GA's.
        const u32 state = !win.enabled ? 1u : hovered == &win ? (input::left_down() ? 3u : 2u) : 0u;
        os::spui::ImageQuad quad;
        if (os::spui::drawable_quad(lay, win.drawable_object, win.placed_area, state, quad)) {
            CachedTexture& image = load_external(lay, quad.external);
            if (image.texture) ov.image_uv(quad.area.left * sx, quad.area.top * sy,
                quad.area.w() * sx, quad.area.h() * sy, image.texture,
                quad.uv.left, quad.uv.top, quad.uv.right, quad.uv.bottom);
        }
        // Always outline, so a transparent window is still visible as the box the designer placed.
        const u32 edge = (win.shade_color >> 24) != 0 ? (win.shade_color & 0xFFFFFFu) | 0x90000000u
                                                      : 0x70A8C8E0u;
        ov.rect(x, y, bw, 1.f, edge);
        ov.rect(x, y2 - 1.f, bw, 1.f, edge);
        ov.rect(x, y, 1.f, bh, edge);
        ov.rect(x2 - 1.f, y, 1.f, bh, edge);
        if (!win.caption.empty() || win.caption_is_ref) {
            // The text is the GAME'S, resolved two ways: inline UTF-16 straight out of the layout,
            // or the (table, key) pair the layout's Caption record carries, resolved through the
            // locale tables we parse in engine/text.cpp.
            std::string cap;
            if (!win.caption.empty()) {
                cap = win.caption;
            } else if (win.caption_key_seen) {
                cap = os::text::instance().get(win.caption_table, win.caption_key);
            }
            if (cap.empty()) {
                // Nothing resolved: show WHAT it points at, so the screen documents the gap
                // instead of inventing a label (the `Load Game` lesson from the Title menu).
                char rb[48];
                std::snprintf(rb, sizeof(rb), "[tbl 0x%08X]", win.caption_table);
                cap = rb;
            }
            cap = os::text::to_display_ascii(cap);
            if (cap.size() > 44) cap = cap.substr(0, 44) + "...";
            if (bw >= 24.f && bh >= 10.f) {
                ov.text_fmt(x + 3.f, y + bh * 0.5f - 7.f, 0xFFF4F8FC, "%s", cap.c_str());
            }
        }
        // A RUNAWAY GUARD, not a policy. It used to be 260, which BINDS: the largest layout here has
        // 570 windows, so the renderer drew a SUBSET while the hit-test knew all of them, and a click
        // could select a widget that was never on screen. Any cap the renderer applies must be the
        // same cap the hit-test respects. 4096 is above every layout in the corpus (max 570 measured
        // over 524 distinct layouts), so it only fires on a corrupt stream.
        if (++drawn >= 4096) break;
    }
    ov.reset_clip(); // diagnostics below are outside the per-window scissor
    if (const auto* gs = os::appstates::instance().find(manager().current_name())) {
        if (os::appmodes::primary_ui(gs->mode.c_str())) {
            // Honest boundary, drawn after game widgets so their runtime-uninitialized positions
            // cannot cover it. No fake starfield or forced visibility for controller-owned panels.
            ov.rect(32.f, (float)h - 116.f, (float)w - 64.f, 40.f, 0xe0101820u);
            ov.text(48.f, (float)h - 106.f, 0xffffce80u,
                    "GGE PRIMARY UI ONLY - galaxy scene, zoom panels and controller events are not implemented");
            ov.text(48.f, (float)h - 89.f, 0xffffce80u,
                    os::gge::wait_description(os::gge::intro_subset().waiting()));
        }
    }
    char foot[180];
    std::snprintf(foot, sizeof(foot),
                  "%zu windows drawn from %zu records; captions are the game's own strings",
                  drawn, lay.anchors);
    ov.text_fmt(48.f, (float)h - 62.f, 0xFF90A8B8, "%s", foot);

    // THE POINTER. Checked once per frame, after the frame's geometry is known, so a click resolves
    // against the same scale that just drew the screen.
    if (input::clicked()) dispatch_click(lay, w, h);

    ov.end();
    dev.end_frame();
    return (int)drawn;
}

bool present_ui_screen(bool fall_back, IDirect3DTexture9* bg, int bg_w, int bg_h) {
    std::string why;
    const auto* lay = requested_by_state(why);
    if (!lay && fall_back) {
        const auto* big = os::spui::instance().best_screen();
        if (big) {
            lay = big;
            why = why + "; showing the largest readable layout instead - OUR choice, not the game's";
        }
    }
    if (!lay) {
        // Say it ONCE. A state that asked for a screen we cannot read must not be papered over with
        // somebody else's layout, and it must not be silent either.
        if (why != g_why_logged) {
            LOG_WARN("ui", "no screen to present: %s", why.c_str());
            g_why_logged = why;
        }
        return false;
    }
    return present_spui_layout(*lay, why, bg, bg_w, bg_h) >= 0;
}

}  // namespace os::states
