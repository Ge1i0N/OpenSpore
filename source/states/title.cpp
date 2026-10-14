#include "states/statemgr.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"
#include "engine/spui.h"
#include "engine/text.h"
#include "render/device.h"
#include "render/imgfile.h"
#include "render/overlay.h"

// A first real UI state: the game's title/launch screen, composed on OUR renderer from the game's
// own UI atlas. It exists to exercise the state machine (entry/exit/tick/render) and to show real
// UI art - the first step toward the menu. It auto-transitions to the Loader after a few seconds,
// which is the same "Title -> Loader" shape the original uses (AppModePluginLoader -> UIWorld).
namespace os::states {
namespace {

IDirect3DTexture9* g_bg = nullptr;
int g_bg_w = 0, g_bg_h = 0;
std::string g_bg_name;
f32 g_t = 0.f;
bool g_left = false;
size_t g_sel = 0;  // the highlighted row; input is not wired to widgets yet (see title_render)

void title_enter() {
    g_t = 0.f;
    g_left = false;
    LOG_INFO("title", "enter Title state");
    // Load a real UI backdrop png (same rule the Loader uses), so the title screen shows game art.
    if (g_bg) { g_bg->Release(); g_bg = nullptr; }
    static const char* kWant[] = {"bckgd", "launchscreen", "newgameflow", "globalui"};
    for (auto& sp : fs::search_paths()) {
        if (g_bg) break;
        for (auto& p : pkg::find_packages(sp.dir.string())) {
            if (g_bg) break;
            pkg::Package pk;
            if (!pk.open(p)) continue;
            for (u32 n = 0; n < pk.records().size(); ++n) {
                const char* tn = naming::type_name(pk.records()[n].type());
                if (!tn || std::string(tn) != "png") continue;
                std::string rname = naming::resource_path(pk.records()[n].type(), pk.records()[n].group(),
                                                          pk.records()[n].instance());
                std::string low = rname;
                for (auto& c : low) c = (char)std::tolower((unsigned char)c);
                bool want = false;
                for (const char* w : kWant) if (low.find(w) != std::string::npos) want = true;
                if (!want) continue;
                std::vector<u8> bytes, bgra;
                if (!pk.read_resource_decoded(n, bytes)) continue;
                if (bytes.size() < 200000) continue;
                int w = 0, h = 0;
                if (!image::decode_from_memory(bytes.data(), bytes.size(), bgra, w, h)) continue;
                if (w < 256 || h < 128) continue;
                g_bg = render::device().create_texture_bgra(bgra.data(), w, h);
                if (!g_bg) continue;
                g_bg_w = w;
                g_bg_h = h;
                g_bg_name = rname;
                LOG_INFO("title", "backdrop %s (%dx%d)", rname.c_str(), w, h);
                break;
            }
        }
    }
}

void title_exit() {
    LOG_INFO("title", "exit Title state after %.1fs", g_t);
    if (g_bg) { g_bg->Release(); g_bg = nullptr; }
}

void title_tick(f32 dt) {
    g_t += dt;
    // Auto-advance to the Loader after 4s - the state machine doing a real transition.
    if (!g_left && g_t >= 4.f) {
        g_left = true;
        LOG_INFO("title", "transition Title -> Loader");
        manager().goto_name("Loader");
    }
}

void title_render() {
    using namespace os::render;
    Device& dev = device();
    Overlay& ov = overlay();
    if (!dev.d3d()) return;
    if (!dev.begin_frame()) return;
    int w = dev.width(), h = dev.height();
    dev.clear(0xFF0A0C10);
    ov.begin(w, h);
    if (g_bg) ov.image(0.f, 0.f, (float)w, (float)h, g_bg, g_bg_w, g_bg_h, false);

    // THE REAL LAYOUT. The game's own UI designer schema (shipped as 14 `xml` resources) lets us read
    // the `spui` layouts, so this draws the game's windows at the game's own coordinates with the
    // game's own captions - not a menu we invented. The layout is scaled from its design resolution
    // to the viewport so it is visible at any window size.
    const auto* lay = os::spui::instance().best_screen();
    if (lay) {
        // Design resolution: the widest right/bottom seen among usable rects, rounded up to a
        // conventional UI size. Measured rather than assumed, so the scale is honest.
        float maxr = 0.f, maxb = 0.f;
        for (const auto& win : lay->windows) {
            if (!win.has_area) continue;
            maxr = std::max(maxr, win.area.right);
            maxb = std::max(maxb, win.area.bottom);
        }
        const float sx = maxr > 1.f ? (float)w / maxr : 1.f;
        const float sy = maxb > 1.f ? (float)h / maxb : 1.f;
        char head[160];
        std::snprintf(head, sizeof(head), "real spui layout: %s  scaled x%.3f / x%.3f",
                      lay->name().c_str(), sx, sy);
        ov.text_fmt(48.f, 22.f, 0xFF8FB8D8, "%s", head);
        size_t drawn = 0;
        for (const auto& win : lay->windows) {
            if (!win.has_area) continue;
            const float x = win.area.left * sx, y = win.area.top * sy;
            const float bw = win.area.w() * sx, bh = win.area.h() * sy;
            if (x + bw < 0.f || y + bh < 0.f || x > (float)w || y > (float)h) continue;
            // The window's own colour, used AS STORED including its alpha. Forcing alpha to 0xFF here
            // (which an earlier revision did) turned the layout into opaque slabs: the shipped values
            // carry meaningful transparency (BackgroundColor 0x007F7F7F is a fully transparent gray,
            // ShadeColor 0xFFFFFFFF is opaque white). Transparency is part of the data.
            const u32 bg = win.bg_color;
            const f32 x2 = x + bw, y2 = y + bh;
            if ((bg >> 24) != 0) ov.rect(x, y, bw, bh, bg);
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
                // locale tables we parse in engine/text.cpp. The second path is the one that lights up
                // most widgets - 1,097 of 1,426 reference records carry a key that is a real key of the
                // referenced table (tools/spui_caption_ref.py).
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
                      drawn, lay->anchors);
        ov.text_fmt(48.f, (float)h - 62.f, 0xFF90A8B8, "%s", foot);
        ov.end();
        dev.end_frame();
        return;
    }

    // Fallback: no layout parsed (missing packages), so show the state-machine menu we resolve from
    // the locale tables instead of nothing.
    auto& tx = os::text::instance();
    float pad = 60.f;
    ov.text_fmt(pad, pad, 0xFFFFE8A0, "OpenSpore");
    std::string src = tx.loaded() ? tx.stats() : std::string("locale text not loaded");
    ov.text_fmt(pad, pad + 26.f, 0xFFB0C4D0, "%s", src.c_str());

    struct Item { const char* label; u32 key; };
    const Item items[] = {
        {"New Game", 0}, {"Load Game", 0}, {"Options", 0}, {"Credits", 0}, {"Exit", 0},
    };
    // Resolve each label to its globally-unique key, so we can print key + text together and prove
    // where the string came from. "Load Game" does not exist as one string in the tables (the game
    // uses "Load"), so fall back through candidates instead of inventing a label.
    static const char* kFallback[][2] = {
        {"New Game"}, {"Load", "Load Game"}, {"Options", "Settings"}, {"Credits"}, {"Exit", "Quit"},
    };
    float y = (float)h * 0.45f;
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        std::string shown;
        u32 key = 0;
        for (const char** p = kFallback[i]; *p; ++p) {
            auto hits = tx.find_label(*p);
            if (hits.empty()) continue;
            // Prefer an unambiguous key: an ambiguous one could render someone else's string.
            for (const auto& hh : hits) {
                if (tx.ambiguous(hh.key)) continue;
                std::string v = tx.get(hh.key);
                if (v.empty()) continue;
                shown = os::text::to_display_ascii(v);
                key = hh.key;
                break;
            }
            if (!shown.empty()) break;
        }
        if (shown.empty()) shown = std::string("[") + items[i].label + "] (no locale string)";
        char bullet = (i == g_sel) ? '>' : ' ';
        ov.text_fmt(pad, y, i == g_sel ? 0xFFFFFFA0 : 0xFFE8F0F4, "%c %s", bullet, shown.c_str());
        if (key) {
            char kb[32];
            _snprintf_s(kb, sizeof(kb), _TRUNCATE, "0x%08X", key);
            ov.text_fmt(pad + 300.f, y, 0xFF8894A0, "%s", kb);
        }
        y += 22.f;
    }
    ov.text_fmt(pad, (float)h - pad, 0xFF909AA0,
                "state: Title   (auto -> Loader in %.1fs)   arrows/enter are not wired: the widget "
                "geometry lives in `spui`, which is unparsed", 4.f - g_t);
    ov.end();
    dev.end_frame();
}

}  // namespace

void install_title(Manager& mgr) {
    State st;
    st.name = "Title";
    st.on_enter = title_enter;
    st.on_exit = title_exit;
    st.on_tick = title_tick;
    st.on_render = title_render;
    if (!mgr.reg(std::move(st))) {
        LOG_ERROR("title", "could not register the Title state");
        return;
    }
    LOG_INFO("title", "Title state registered");
}

}  // namespace os::states
