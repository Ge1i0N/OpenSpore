#include "states/statemgr.h"

#include <string>
#include <vector>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"
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

    // A title-screen frame over the art. The menu items are placeholders for the real `spui`
    // layout we have not parsed yet (analysis/ui.md) - clearly labelled as such.
    float pad = 60.f;
    ov.text_fmt(pad, pad, 0xFFFFE8A0, "OpenSpore");
    ov.text_fmt(pad, pad + 26.f, 0xFFB0C4D0, "title screen (state machine) - real Spore UI atlas bg");
    float y = (float)h * 0.45f;
    const char* items[] = {"New Game", "Load Game", "Settings", "Exit"};
    for (const char* it : items) {
        ov.text_fmt(pad, y, 0xFFE8F0F4, "  %s", it);
        y += 22.f;
    }
    ov.text_fmt(pad, (float)h - pad, 0xFF909AA0, "state: Title   (auto -> Loader in %.1fs)", 4.f - g_t);
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
