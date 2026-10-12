#include "app.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>  // abort(), for the deliberate fault-tolerance probe
#include <cstring>
#include <string>

#include "engine/cmdline.h"
#include "engine/console.h"
#include "engine/diag.h"
#include "engine/fs.h"
#include "engine/log.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/statemgr.h"

namespace os::app {
namespace {

// ---------------------------------------------------------------- yardstick
// Field guide §1: maintain ONE cheap numeric measure of "are we further along".
// Here it is the count of boot-chain markers that reached OK. It is printed once per run
// and is machine-comparable across sessions.
struct BootMarker {
    const char* name;
    const char* status = "pending";
};

BootMarker g_chain[] = {
    {"diag.install"}, {"log.init"}, {"cmdline.parse"}, {"fs.init"}, {"window.create"},
    {"d3d.device"}, {"font.atlas"}, {"console.init"}, {"states.register"}, {"state.goto"},
};
constexpr size_t kChainCount = sizeof(g_chain) / sizeof(g_chain[0]);
size_t g_chain_ok = 0;

void mark(size_t i, bool ok, const std::string& detail = {}) {
    g_chain[i].status = ok ? "OK" : "FAIL";
    if (ok) ++g_chain_ok;
    char buf[160];
    _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[%2zu/%2zu] %-16s %s%s%s", i + 1, kChainCount, g_chain[i].name,
                g_chain[i].status, detail.empty() ? "" : "  ", detail.c_str());
    // LOG_ONCE on purpose: every marker shares this call site, so the ledger would
    // consolidate markers 9-10 away ("x9, further repeats suppressed" in the first run).
    if (ok) {
        LOG_ONCE_INFO("boot", "%s", buf);
    } else {
        LOG_ONCE_ERROR("boot", "%s", buf);
    }
}

void print_chain() {
    std::string s = "\n--- boot chain -------------------------------------------------\n";
    for (size_t i = 0; i < kChainCount; ++i) {
        char line[128];
        _snprintf_s(line, sizeof(line), _TRUNCATE, "  %-18s %s\n", g_chain[i].name, g_chain[i].status);
        s += line;
    }
    s += "--------------------------------------------------------------\n";
    LOG_INFO("boot", "yardstick: %zu/%zu markers OK", g_chain_ok, kChainCount);
    log::raw_write(s.c_str(), s.size());
}

// ---------------------------------------------------------------- window
const wchar_t* kWndClass = L"OpenSporeWindowClass";

Args g_args;
HWND g_hwnd = nullptr;
bool g_quit = false;
int g_quit_code = 0;
bool g_minimized = false;
bool g_size_pending = false;
int g_new_w = 0, g_new_h = 0;

LRESULT CALLBACK wnd_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: return 0;
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            if (wp == SIZE_MINIMIZED) {
                g_minimized = true;
            } else if (wp == SIZE_RESTORED) {
                g_minimized = false;
            } else if (wp == SIZE_MAXIMIZED) {
                g_minimized = false;
            }
            if (wp != SIZE_MINIMIZED) {
                g_new_w = LOWORD(lp);
                g_new_h = HIWORD(lp);
                g_size_pending = true;
            }
            return 0;
        case WM_MOVE: return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(h, &ps);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_SETCURSOR:
            // The original manages its own cursor (USER32 SetCursor/ShowCursor imports).
            // During boot we keep the arrow.
            SetCursor(LoadCursor(nullptr, IDC_ARROW));
            return TRUE;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                PostQuitMessage(0);
                return 0;
            }
            break;
        case WM_ACTIVATEAPP:
            LOG_DEBUG("app", "activate=%d", (int)wp);
            return 0;
        case WM_DISPLAYCHANGE:
            LOG_INFO("app", "display changed: %dx%d", LOWORD(lp), HIWORD(lp));
            return 0;
        case WM_DPICHANGED: return 0;
        default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

bool create_window(HINSTANCE inst, int w, int h, bool windowed, const wchar_t* title) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = kWndClass;
    // Icon: shipped as resource id 101 (see openspore.rc, generated from refs/OpenSpore.ico).
    HICON ico = LoadIconW(inst, MAKEINTRESOURCEW(101));
    wc.hIcon = ico ? ico : LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = ico;
    if (!RegisterClassExW(&wc)) {
        LOG_ERROR("app", "RegisterClassExW failed: %lu", GetLastError());
        return false;
    }
    DWORD style = windowed ? (WS_OVERLAPPEDWINDOW) : (WS_POPUP | WS_VISIBLE);
    RECT rc{0, 0, w, h};
    if (windowed) AdjustWindowRectEx(&rc, style, FALSE, 0);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int x = CW_USEDEFAULT, y = CW_USEDEFAULT;
    if (windowed) {
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        x = (sw - ww) / 2;
        y = (sh - wh) / 2;
    }
    g_hwnd = CreateWindowExW(0, kWndClass, title, style, x, y, ww, wh, nullptr, nullptr, inst, nullptr);
    if (!g_hwnd) {
        LOG_ERROR("app", "CreateWindowExW failed: %lu", GetLastError());
        return false;
    }
    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);
    LOG_INFO("app", "window %p '%s' %dx%d (client %dx%d) windowed=%d icon=%s", g_hwnd,
            "OpenSpore - Loader", ww, wh, w, h, (int)windowed, ico ? "embedded" : "fallback");
    return true;
}

// ---------------------------------------------------------------- console attach
// PROMPT.md: "игра будет с консолью при запуске". Strategy: if we were launched from a
// shell we already HAVE a console (AttachConsole), otherwise allocate one. Never block on
// it: a console is an observer here, not a dependency.
// Returns a human-readable account of which path succeeded, for the boot log.
std::string attach_console() {
    if (GetConsoleWindow()) return "inherited from parent";
    if (AttachConsole(ATTACH_PARENT_PROCESS)) return "attached to parent console";
    if (AllocConsole()) {
        // Re-point the C runtime at the new console so printf/fgets work.
        FILE* dummy = nullptr;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONIN$", "r", stdin);
        return "allocated a new console";
    }
    return "NONE (AllocConsole failed, err=" + std::to_string(GetLastError()) + ")";
}

// ---------------------------------------------------------------- frame loop
u64 g_frames = 0;
u64 g_frame_aborts = 0;

// The risky region, isolated in a frame with no non-trivial C++ objects, so that a
// longjmp-based recovery from diag::run_guarded cannot corrupt the caller.
struct FrameCtx {
    f32 dt;
};

// Pending deliberate aborts, requested by the `_abortNow` verb. The command itself only
// sets this counter; frame_body() consumes it INSIDE the guarded region. Executing abort()
// in console::poll() would test nothing, because poll() runs outside run_guarded.
volatile LONG g_pending_aborts = 0;

void request_abort(int times) { InterlockedExchange(&g_pending_aborts, (LONG)times); }

void frame_body(void* p) {
    auto* fc = static_cast<FrameCtx*>(p);
    LONG n = g_pending_aborts;
    if (n > 0) {
        // One abort per frame, not nine in a single frame: that is what a real fault looks
        // like (a state that dies on entry), and it is the only way to reach diag's
        // storm limit (>5 aborts within 1s) instead of recovering forever.
        InterlockedDecrement(&g_pending_aborts);
        LOG_ERROR("fault", "deliberate abort() from INSIDE the guarded frame (%ld left)", (long)g_pending_aborts);
        void (*volatile fp)() = abort;
        fp();
    }
    states::manager().tick(fc->dt);
    states::manager().render();
}

void pump_messages() {
    MSG m;
    while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) {
        if (m.message == WM_QUIT) {
            g_quit = true;
            g_quit_code = (int)m.wParam;
            return;
        }
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
}

}  // namespace

void request_quit(int code) {
    g_quit = true;
    g_quit_code = code;
}

bool quitting() { return g_quit; }

int run(HINSTANCE inst, HINSTANCE, LPWSTR cmdline_wide, int) {
    // 0. diagnostics + log FIRST (field guide §4: instrument before you experiment).
    // Order matters and was wrong in the first run: diag::install() must precede log::init(),
    // because log::init() itself can trip a CRT assert (it did: setvbuf with a 0-byte buffer),
    // and the only thing that turns that into "log + continue" instead of a modal dialog is
    // the harness being already in place.
    std::string console_how = attach_console();
    diag::install("OpenSpore.crash.log");
    log::init("OpenSpore.log", log::Level::Trace, log::Level::Debug);
    mark(0, true, "harness up before the log existed");
    mark(1, true, "file=OpenSpore.log console=" + console_how);

    LOG_INFO("app", "=== OpenSpore boot (GA reimplementation) ===");
    // Preprocessor directives are not allowed inside a macro argument list (that is what
    // the first build's C2121 '#' error was), so the arch string is computed beforehand.
#ifdef _WIN64
    const char* arch = "x64";
#else
    const char* arch = "x86";
#endif
    LOG_INFO("app", "build: %s, console=%s, MSVC %d", arch,
             GetConsoleWindow() ? "attached" : "none", _MSC_VER);

    // 2. command line
    g_args = parse_command_line(cmdline_wide ? cmdline_wide : GetCommandLineW());
    mark(2, true, to_string(g_args));
    LOG_INFO("app", "flags: windowed=%d devDirs=%d shipDirs=%d useDevDirs=%d state='%s' preload='%s' %dx%d vsync=%d",
             (int)g_args.windowed, (int)g_args.dev_dirs, (int)g_args.ship_dirs, (int)g_args.use_dev_dirs(),
             g_args.state.c_str(), g_args.preload.c_str(), g_args.width, g_args.height, (int)!g_args.no_vsync);

    // 3. filesystem search paths (reversed devDirs contract: shipDirs wins over devDirs)
    wchar_t exe_path[MAX_PATH] = {};
    GetModuleFileNameW(inst, exe_path, MAX_PATH);
    std::filesystem::path exe_dir = std::filesystem::path(exe_path).parent_path();
    fs::init(g_args, exe_dir);
    mark(3, true, fs::dump_search_list());

    // 4. window
    if (!create_window(inst, g_args.width, g_args.height, g_args.windowed, L"OpenSpore - Loader")) {
        mark(4, false, "CreateWindow failed");
        print_chain();
        log::shutdown();
        return 3;
    }
    mark(4, true);

    // 5. renderer. Error strings mirror the original's own contract (see device.h header).
    render::Config rcfg;
    rcfg.windowed = g_args.windowed;
    rcfg.width = g_args.width;
    rcfg.height = g_args.height;
    rcfg.vsync = !g_args.no_vsync;
    std::string err;
    if (!render::device().init(g_hwnd, rcfg, err)) {
        mark(5, false, err);
        LOG_FATAL("app", "Could not start the renderer. (%s)", err.c_str());  // 0x014139e0
        print_chain();
        log::shutdown();
        return 4;
    }
    mark(5, true,
         "device " + std::to_string(render::device().width()) + "x" + std::to_string(render::device().height()));

    // 6. font atlas
    if (!render::overlay().init(render::device().d3d())) {
        mark(6, false, "atlas");
        print_chain();
        log::shutdown();
        return 5;
    }
    mark(6, true);

    // 7. console + dev commands. PROMPT.md: developer commands available in Debug builds;
    // the original also ships `state` in release, so the state verb is unconditional.
#ifdef OS_ENABLE_DEV_COMMANDS
    const bool dev = true;
#else
    const bool dev = false;
#endif
    console::init(dev);
    states::install_console_command(states::manager());
    states::install_loader(states::manager());

    // Capture + fault-tolerance probes. `capture` is unconditional (it is the only way to
    // get machine evidence of a rendered frame); `_abortNow` is dev-only by the '_' rule.
    console::reg({"capture", "Write the current front buffer to a BMP (PrintWindow cannot do D3D9).",
                  "-<path>",
                  [](const console::CmdContext& c) -> std::string {
                      std::string p = c.args.size() > 1 ? c.args[1] : "OpenSpore_capture.bmp";
                      // Same deferral as the -capture launch key: poll() runs outside the scene.
                      render::device().request_capture(p.c_str());
                      return "capture scheduled for the next frame -> " + p;
                  }});
    console::reg({"_abortNow",
                  "Deliberately abort() inside the guarded frame to prove run_guarded recovers.",
                  "-<times:n>",
                  [](const console::CmdContext& c) -> std::string {
                      int times = c.args.size() > 1 ? atoi(c.args[1].c_str()) : 1;
                      if (times < 1) times = 1;
                      // Do NOT abort here: this handler runs in console::poll(), which is
                      // outside run_guarded, so it would test the handler but not the
                      // recovery. Set the flag; frame_body() consumes it inside the guard.
                      request_abort(times);
                      LOG_WARN("fault", "_abortNow: armed %d abort(s) for the next frame", times);
                      return "armed: the next frame will abort inside run_guarded";
                  }});
    mark(7, true, std::to_string(console::count()) + " commands");
    mark(8, true, "Loader registered");

    // 8. goto the requested state (launch key -state <name>; default: Loader)
    std::string want = !g_args.state.empty() ? g_args.state : "Loader";
    bool ok = states::manager().goto_name(want);
    if (!ok && want != "Loader") {
        LOG_WARN("app", "state '%s' not found (no such state), falling back to Loader", want.c_str());
        ok = states::manager().goto_name("Loader");
    }
    mark(9, ok, "current='" + states::manager().current_name() + "'");
    print_chain();

    // 9. stdin console worker (no-op when no console is attached)
    console::start_stdin_thread();
    LOG_ONCE_INFO("frame", "about to enter the frame loop (console=%s, state='%s')",
                  GetConsoleWindow() ? "attached" : "none", states::manager().current_name().c_str());

    // ------------------------------------------------------------ frame loop
    LARGE_INTEGER qpf{}, t0{}, t_prev{};
    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&t0);
    t_prev = t0;
    double fps_acc = 0.0;
    u32 fps_frames = 0;
    double fps_display = 0.0;
    LARGE_INTEGER t_fps = t0;
    bool loop_alive_logged = false;
    bool first_iter_logged = false;

    while (!g_quit) {
        pump_messages();
        if (g_quit) break;
        console::poll();
        if (!first_iter_logged) {
            first_iter_logged = true;
            LOG_ONCE_INFO("frame", "first iteration reached (minimized=%d)", (int)g_minimized);
        }

        if (g_size_pending) {
            g_size_pending = false;
            if (!g_minimized && render::device().windowed()) {
                render::device().resize(g_new_w, g_new_h);
            }
        }

        LARGE_INTEGER t_now;
        QueryPerformanceCounter(&t_now);
        f32 dt = (f32)((double)(t_now.QuadPart - t_prev.QuadPart) / (double)qpf.QuadPart);
        t_prev = t_now;
        if (dt > 0.25f) dt = 0.25f;  // tab-out guard

        if (g_minimized) {
            Sleep(20);
            continue;
        }

        // Fault tolerance: one bad frame must not take the process down. If anything in
        // the frame calls abort()/assert-with-abort, diag's handler logs the stack and
        // longjmps back, and we skip the rest of that frame.
        FrameCtx fctx{dt};
        if (diag::run_guarded(frame_body, &fctx, "frame")) {
            ++g_frame_aborts;
            LOG_ERROR("frame", "frame aborted (total %llu) - skipping this frame",
                      (unsigned long long)g_frame_aborts);
            Sleep(16);
            continue;
        }

        ++g_frames;
        if (!g_args.capture_path.empty() && g_frames == (u64)g_args.capture_after) {
            // Defer into the scene: GetFrontBufferData is only legal between BeginScene and
            // EndScene, and this point in the loop is outside it. end_frame() performs it.
            render::device().request_capture(g_args.capture_path.c_str());
            LOG_ONCE_INFO("capture", "capture requested for frame %llu -> %s", (unsigned long long)g_frames,
                          g_args.capture_path.c_str());
        }
        if (!g_args.exec_cmds.empty() && g_frames == (u64)g_args.exec_after) {
            // Split on ';' and run each through the real command path, so an unattended
            // test exercises exactly what a human types.
            std::string s = g_args.exec_cmds;
            size_t pos = 0;
            while (pos < s.size()) {
                size_t sc = s.find(';', pos);
                std::string one = (sc == std::string::npos) ? s.substr(pos) : s.substr(pos, sc - pos);
                pos = (sc == std::string::npos) ? s.size() : sc + 1;
                // strip surrounding spaces
                size_t a0 = one.find_first_not_of(" \t");
                if (a0 == std::string::npos) continue;
                size_t a1 = one.find_last_not_of(" \t");
                one = one.substr(a0, a1 - a0 + 1);
                std::string res;
                bool cmd_ok = console::exec_line(one, res);
                LOG_ONCE_INFO("exec", "-exec '%s' -> ok=%d %s", one.c_str(), (int)cmd_ok, res.c_str());
            }
        }
        if (g_args.quit_after_frames > 0 && g_frames >= (u64)g_args.quit_after_frames) {
            LOG_ONCE_INFO("app", "auto-quit after %llu frames (-quitAfter %d)", (unsigned long long)g_frames,
                          g_args.quit_after_frames);
            break;
        }
        fps_acc += dt;
        ++fps_frames;
        if ((double)(t_now.QuadPart - t_fps.QuadPart) / (double)qpf.QuadPart >= 1.0) {
            fps_display = fps_frames / (fps_acc > 0 ? fps_acc : 1.0);
            fps_acc = 0.0;
            fps_frames = 0;
            t_fps = t_now;
            LOG_DEBUG("frame", "fps=%.1f frames=%llu aborts=%llu state='%s'", fps_display,
                      (unsigned long long)g_frames, (unsigned long long)g_frame_aborts,
                      states::manager().current_name().c_str());
        }
        if (!loop_alive_logged && g_frames >= 2) {
            loop_alive_logged = true;
            LOG_INFO("frame", "frame loop alive (2 frames rendered, state='%s')", states::manager().current_name().c_str());
        }
    }

    LOG_INFO("app", "shutting down: frames=%llu aborts=%llu asserts=%u unreachable=%u log_lines=%llu",
             (unsigned long long)g_frames, (unsigned long long)g_frame_aborts, diag::assert_count(),
             diag::unreachable_count(), (unsigned long long)log::lines_written());
    console::shutdown();
    render::overlay().shutdown();
    render::device().shutdown();
    log::dump_ledger();
    LOG_INFO("app", "clean exit code=%d", g_quit_code);
    log::shutdown();
    return g_quit_code;
}

}  // namespace os::app
