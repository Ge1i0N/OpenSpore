#include "states/debugmenu.h"

#ifdef OS_ENABLE_DEV_COMMANDS
#include "engine/console.h"
#include "engine/log.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/statemgr.h"

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

namespace os::states::debugmenu {
namespace {
constexpr const char* kName = "OpenSporeDebugCheatConsole";
Manager* g_manager = nullptr;
std::string g_previous;
bool g_open = false;
std::string g_input;
std::vector<std::string> g_history;
size_t g_history_pos = 0;
std::deque<std::string> g_output;

void output(const std::string& text) {
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (line.size() > 112) line.resize(112);
        g_output.push_back(std::move(line));
        if (g_output.size() > 80) g_output.pop_front();
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

void submit() {
    if (g_input.empty()) return;
    const std::string line = g_input;
    output("> " + line);
    if (g_history.empty() || g_history.back() != line) g_history.push_back(line);
    if (g_history.size() > 32) g_history.erase(g_history.begin());
    g_history_pos = g_history.size();
    std::string result;
    const bool ok = os::console::exec_line(line, result);
    if (!result.empty()) output(result);
    else if (!ok) output("command failed");
    g_input.clear();
}

void history_up() {
    if (g_history.empty()) return;
    if (g_history_pos > 0) --g_history_pos;
    g_input = g_history[g_history_pos];
}

void history_down() {
    if (g_history_pos + 1 < g_history.size()) {
        ++g_history_pos;
        g_input = g_history[g_history_pos];
    } else {
        g_history_pos = g_history.size();
        g_input.clear();
    }
}

void close_console() {
    if (!g_manager || !g_open) return;
    const std::string previous = g_previous;
    g_open = false;
    g_previous.clear();
    g_input.clear();
    if (!previous.empty()) g_manager->request_goto(previous);
}

void enter() {
    g_open = true;
    g_input.clear();
    g_history_pos = g_history.size();
    g_output.clear();
    output("OpenSpore DEBUG CHEAT CONSOLE");
    output("Type help for commands. Enter executes; Up/Down recalls history.");
    output("Ctrl+Shift+C toggles this console; Esc returns to the previous state.");
    LOG_INFO("debug-console", "opened previous=%s", g_previous.c_str());
}

void exit() {
    g_open = false;
    g_input.clear();
    g_previous.clear();
    LOG_INFO("debug-console", "closed");
}

void tick(f32) {
    // Text input is delivered by the window procedure, just like the original in-window console's
    // message-facing boundary. The state owns only the presentation and command submission.
}

void render() {
    auto& device = os::render::device();
    if (!device.d3d() || !device.begin_frame()) return;
    device.clear(0xff080d16);
    auto& overlay = os::render::overlay();
    overlay.begin(device.width(), device.height());
    const float left = 58.f, top = 42.f, width = (float)device.width() - 116.f;
    overlay.rect(left - 18.f, top - 18.f, width + 36.f, (float)device.height() - 84.f, 0xee101c2c);
    overlay.text(left, top, 0xffffe3a0, "OpenSpore DEBUG CHEAT CONSOLE");
    overlay.text(left, top + 22.f, 0xff9fb4c4,
                 "Ctrl+Shift+C toggles | Esc returns | type a command and press Enter");
    const int visible = std::max(8, (device.height() - 190) / 18);
    int skip = (int)g_output.size() - visible;
    if (skip < 0) skip = 0;
    float y = top + 52.f;
    for (int i = skip; i < (int)g_output.size(); ++i) {
        overlay.text(left, y, 0xffd4e2ec, g_output[(size_t)i]);
        y += 18.f;
    }
    overlay.rect(left - 4.f, y + 4.f, width - 8.f, 26.f, 0xff152638);
    overlay.text(left + 8.f, y + 9.f, 0xffffffff, "> " + g_input + "_");
    overlay.text_fmt(left, y + 42.f, 0xff8197ae, "Current state: %s [0x%08X]",
                     g_previous.empty() ? "<none>" : g_previous.c_str(), g_manager->current_id());
    overlay.text(left, y + 60.f, 0xff8197ae,
                 "Host console: unsupported game services remain explicit and do not alter saves.");
    overlay.end();
    device.end_frame();
}

} // namespace

void toggle() {
    if (!g_manager) return;
    if (g_open) {
        close_console();
        return;
    }
    const std::string current = g_manager->current_name();
    if (current.empty() || current == kName) return;
    g_previous = current;
    g_manager->request_goto(kName);
}

bool open() { return g_open; }

bool handle_keydown(unsigned key, long repeat_flags) {
    if (!g_open || (repeat_flags & (1L << 30))) return false;
    switch (key) {
        case VK_ESCAPE: close_console(); return true;
        case VK_RETURN: submit(); return true;
        case VK_BACK: if (!g_input.empty()) g_input.pop_back(); return true;
        case VK_UP: history_up(); return true;
        case VK_DOWN: history_down(); return true;
        default: return false;
    }
}

bool handle_char(wchar_t ch) {
    if (!g_open || ch < 32 || ch > 126) return false;
    if (g_input.size() < 240) g_input.push_back((char)ch);
    return true;
}

void install(Manager& manager) {
    g_manager = &manager;
    State state;
    state.name = kName;
    state.on_enter = enter;
    state.on_exit = exit;
    state.on_tick = tick;
    state.on_render = render;
    if (manager.reg(std::move(state))) LOG_INFO("states", "registered dev-only %s", kName);

    const auto toggle_command = [](const os::console::CmdContext&) -> std::string {
        const bool was_open = open();
        toggle();
        return was_open ? "Debug Cheat Console closing" : "Debug Cheat Console opening";
    };
    // Keep _debugmenu as a compatibility alias for the already-shipped local launcher scripts.
    os::console::reg({"_debugmenu", "Open or close the OpenSpore Debug Cheat Console.", "", toggle_command});
    os::console::reg({"_debugconsole", "Open or close the OpenSpore Debug Cheat Console.", "", toggle_command});
    os::console::reg({"levels", "Game level developer controls (host boundary).", "-unlock",
                      [](const os::console::CmdContext& c) -> std::string {
                          if (c.args.size() > 1 && c.args[1] == "-unlock")
                              return "levels -unlock: not implemented; no levels or save data changed";
                          return "levels: use -unlock (native level service is not connected)";
                      }});
}

} // namespace os::states::debugmenu
#else
namespace os::states::debugmenu {
void toggle() {}
bool open() { return false; }
bool handle_keydown(unsigned, long) { return false; }
bool handle_char(wchar_t) { return false; }
void install(Manager&) {}
} // namespace os::states::debugmenu
#endif
