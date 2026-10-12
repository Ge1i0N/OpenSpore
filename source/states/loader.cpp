#include "states/statemgr.h"

#include <cstdio>
#include <cstring>

#include "engine/console.h"
#include "engine/diag.h"
#include "engine/fs.h"
#include "engine/log.h"
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
    ov.text_fmt(pad, y, kDim, "mode: %s %dx%d vsync=%d  desktop=%dx%d", dev.windowed() ? "windowed" : "fullscreen", w,
                h, (int)dev.vsync(), dev.desktop_mode().Width, dev.desktop_mode().Height);
    y += ov.font_height();
    ov.text_fmt(pad, y, kDim, "state: %s", g_mgr ? g_mgr->current_name().c_str() : "?");
    y += ov.font_height();
    ov.text_fmt(pad, y, kDim, "%s", fs::dump_search_list().c_str());
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
