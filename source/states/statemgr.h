#pragma once
// AppStateManager - the game's launch-state machine, reimplemented from disassembly.
//
// This is the mechanism the user remembers as "типа режимы запуска, управлял ими в девбилде".
// It IS a real, controllable subsystem in the retail GA build, and it is reachable in the
// release exe too (not a debug-only secret). Every claim below is level-1 evidence from
// refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe:
//
//  * State identity is a 32-bit FNV-1a hash of the name:
//      007e40f2: push 0x811c9dc5 / push eax(name) / call 0x932e50
//      007e4103: mov [ecx+0x28], eax          ; ID stored on the state
//      007e4132: cmp ecx,[eax+edx*4]          ; duplicate-ID probe
//      007e413b: push 0x1413600               ; "A state with this ID already exists"
//  * The manager registers itself under the name 'AppStateManager' (0x0141319c), next to
//    the ArgScript verbs that drive it: 'state' 0x01413194, 'gotoState' 0x0141316c,
//    'transition' 0x01413188, 'entry' 0x01413180, 'exit' 0x01413178, 'endChain' 0x014130f8,
//    and the behaviour-tree tag '.eBT' 0x014131bd. So states are DECLARED IN SCRIPT DATA
//    (.eBT behaviour trees) and instantiated by this manager - the set of state names is
//    data, not code. We have no .big assets yet, hence our own Loader state below.
//  * Nesting is illegal, and transitions are a chain, not a stack:
//      0x014135dc "can't nest states or transitions"   (refs 0x7e35ce, 0x7e3fde)
//      0x014135b4 'unknown to state'    0x014135c8 'unknown from state'
//      0x01413624 'onExit'              (the per-state callback slot)
//  * Startup selects the state via launch keys (both UTF-16, both looked up through the
//    same helper 0x92b570 as devDirs/shipDirs):
//      0x007e2235: push 'preload'(0x14135a4) / call 0x92b570 -> [esi+0x38] lookup, [esi+0x28] goto
//      0x007e22a0: push 'state'  (0x1413588) / call 0x92b570 -> 0x93c4e0 decode, then goto
//  * The developer console command is literally `state` (help 0x014136dc
//    'State manager control. With no arguments lists current state.', arg spec 0x0141371c
//    '[<state:cstring>]', options pushed as a table at 0x007e4fb1..0x007e4faf):
//      0x1413748 'List all states'      -list^  (0x140e430)
//      0x1413760 'Move to next state'   -next^  (0x1413758)
//      0x141377c 'Move to previous state' -prev^ (0x1413774)
//      0x14137a0 'Force script file to reload' -reload^ (0x1413794)
//      0x1413730 'Switch to given state.'
//    Listing format: 0x14137ec '  %-40.40s [0x%08x]'   <- name + its FNV ID, exactly our model
//    Current:        0x14137d4 "Current state: '%s'"
//    Miss:           0x1413804 'no such state'
//
// Our implementation keeps the CONTRACT (FNV ids, no nesting, onEnter/onExit, the `state`
// console verb with its four options, the -state/-preload launch keys) and drops the parts
// that need EA's script data.

#include <functional>
#include <string>
#include <vector>

#include "common.h"

namespace os::states {

struct State {
    std::string name;
    u32 id = 0;  // FNV-1a of name, as the original stores at [state+0x28]
    std::function<void()> on_enter;
    std::function<void()> on_exit;
    std::function<void(f32 dt)> on_tick;
    std::function<void()> on_render;
    std::function<void(const char* path)> on_reload;  // -reload^ analog
};

class Manager {
public:
    // Register a state. Returns false and logs "A state with this ID already exists"
    // semantics on a duplicate id (the original asserts rather than silently overwriting).
    bool reg(State st);

    // Switch state. Refuses to recurse: "can't nest states or transitions".
    bool goto_name(const std::string& name);

    // The `state` console verb's four options.
    bool next();
    bool prev();
    void reload();  // re-fires on_reload on the current state

    std::string list() const;        // '  %-40.40s [0x%08x]' per state
    std::string current_name() const { return m_current ? m_current->name : std::string(); }
    u32 current_id() const { return m_current ? m_current->id : 0; }
    State* find(const std::string& name);
    const std::vector<State*>& states() const { return m_all; }

    void tick(f32 dt);
    void render();

    bool entering() const { return m_in_transition; }

private:
    std::vector<State*> m_owned;      // registration order == the -next/-prev order
    std::vector<State*> m_all;        // same pointers, kept for iteration stability
    State* m_current = nullptr;
    bool m_in_transition = false;     // the anti-nesting guard
};

Manager& manager();

// Register the `state` console command against a manager (dev builds get it too; the
// original ships it in release, so we do not hide it behind OS_ENABLE_DEV_COMMANDS).
void install_console_command(Manager& mgr);

// The boot state we own (see states/loader.cpp). Named 'Loader' after the original's
// AppModePluginLoader / UIWorld_Loading family.
void install_loader(Manager& mgr);

// A first UI state (see states/title.cpp): the title screen, which auto-transitions to Loader.
void install_title(Manager& mgr);

}  // namespace os::states
