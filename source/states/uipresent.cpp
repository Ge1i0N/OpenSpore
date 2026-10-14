#include "states/uipresent.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "engine/appstates.h"  // the state's own script, which names the screen
#include "engine/log.h"
#include "engine/naming.h"     // hash_lower: a layout NAME becomes a resource INSTANCE
#include "engine/spui.h"
#include "engine/text.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/statemgr.h"

namespace os::states {
namespace {

// The last `why` we logged, so the choice is announced once per change and not 60 times a second.
// Kept here rather than in the caller because every state that draws a layout shares this path.
std::string g_why_logged;

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
    why = "state '" + st + "' names no layout";
    return nullptr;
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

    // Design resolution: the widest right/bottom seen among usable rects. Measured from the layout
    // itself rather than assumed, so the scale is honest at any window size.
    float maxr = 0.f, maxb = 0.f;
    for (const auto& win : lay.windows) {
        if (!win.has_area) continue;
        maxr = std::max(maxr, win.area.right);
        maxb = std::max(maxb, win.area.bottom);
    }
    const float sx = maxr > 1.f ? (float)w / maxr : 1.f;
    const float sy = maxb > 1.f ? (float)h / maxb : 1.f;

    char head[160];
    std::snprintf(head, sizeof(head), "spui layout: %s  scaled x%.3f / x%.3f",
                  lay.name().c_str(), sx, sy);
    ov.text_fmt(48.f, 22.f, 0xFF8FB8D8, "%s", head);
    ov.text_fmt(48.f, 38.f, 0xFF6E8FA8, "chosen because: %s", why.c_str());

    size_t drawn = 0;
    for (const auto& win : lay.windows) {
        if (!win.has_area) continue;
        const float x = win.area.left * sx, y = win.area.top * sy;
        const float bw = win.area.w() * sx, bh = win.area.h() * sy;
        if (x + bw < 0.f || y + bh < 0.f || x > (float)w || y > (float)h) continue;
        // The window's own colour, used AS STORED including its alpha. Forcing alpha to 0xFF here
        // (which an earlier revision did) turned the layout into opaque slabs: the shipped values
        // carry meaningful transparency (BackgroundColor 0x007F7F7F is a fully transparent gray,
        // ShadeColor 0xFFFFFFFF is opaque white). Transparency is part of the data.
        const u32 cbg = win.bg_color;
        const f32 x2 = x + bw, y2 = y + bh;
        if ((cbg >> 24) != 0) ov.rect(x, y, bw, bh, cbg);
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
        if (++drawn >= 260) break;
    }
    char foot[180];
    std::snprintf(foot, sizeof(foot),
                  "%zu windows drawn from %zu records; captions are the game's own strings",
                  drawn, lay.anchors);
    ov.text_fmt(48.f, (float)h - 62.f, 0xFF90A8B8, "%s", foot);
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
