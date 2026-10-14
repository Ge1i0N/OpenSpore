#pragma once
// The game's OWN state table, loaded from the shipped `SporeAppStates` trigger resource.
//
// This is the answer to "the state names are data we do not have" - which was wrong for three
// sessions. `SporeAppStates` is a `trigger` resource (type 0x024A0E52) with INSTANCE id 0xD6396913
// in group 0; it was missing from every earlier search because those looked at the GROUP field of a
// record that has three id fields. Full reversal: analysis/states.md.
//
// The resource is plain ArgScript text, RefPack-compressed, 8,499 decoded bytes in the EP1 copy:
//
//   state Main -id 0
//      description "Spore Launch Screen"
//      gotoState GalaxyGameEntry
//   end
//
// 60 states, 17 modes. The entry chain is `Main` -> `GalaxyGameEntry`, and `-demo` enters `Demo`
// instead, exactly as the file's own header says:
//
//   # Note: if you run with the -demo flag, you'll start up
//   # with the Demo state. Otherwise, you can run with
//   # -state:<stateName> to get a specific state.
//
// IDS COME FROM THE FILE. 19 states carry an explicit `-id`, and measured: 0 of those 19 equal
// fnv1_lower(name), because the script's author assigned them by hand ("assign manual IDs just so I
// can fill in the UI layout stuff more easily"). The other 41 have no `-id` and are hashed by the
// engine, which is the AppStateManager rule we already reversed (0x007e40f2). So `State::id` must
// never be recomputed for an explicit id - see states::State.
//
// CROSS-SYSTEM CONFIRMATION (measured, session 13): four state names hash to a real en-us `locale`
// TABLE instance, i.e. those stages own their text:
//   GalaxyGameEntry 0x2B7A0311   SpaceGame 0x2DB6DAD3
//   CreatureGame    0xAD56080C   TribeGame 0xF71FA311
// Control: 193 of 125,272 registry names hash to any of the 200 table instances, so the expected
// accidental rate over 60 states is ~0.09 hits, not 4. This single measurement corroborates at once
// the FNV-1 multiply-first direction, the tolower folding, the instance-id naming rule, the recovered
// state names, and the locale table indexing - which is why it is recorded here rather than buried in
// a script.
//
// WHAT IS INFERRED, NOT PROVEN: the execution ORDER (state-level `mode`/`message` fire on entry,
// `gotoState` fires after the entry block). It is read off the script's shape - every state uses one
// of the two forms consistently - and matches the `entry`/`exit`/`onExit` callback names we found in
// .rdata. The interpreter's own order is still to be confirmed against the disassembly of the
// executors (analysis/config-scripts.md) before anything gameplay-critical depends on it.
//
// WHAT THIS DOES NOT GIVE US: the modes. `mode GalaxyGameEntry` names a subsystem configuration that
// lives in the engine's mode table, not in this script; loading it is the next step after `spui`.
#include <string>
#include <vector>

#include "common.h"

namespace os::states {
class Manager;
}

namespace os::appstates {

struct Action {
    std::string verb;   // mode | cheat | effect | message | prop | gotoState
    std::string raw;    // the rest of the line, verbatim
    std::string a1;     // the first token after the verb, when there is one
};

struct GameState {
    std::string name;
    u32 id = 0;
    bool has_explicit_id = false;
    std::string description;
    std::string mode;             // state-level mode (fires on entry)
    std::vector<Action> entry;    // the `entry` block, in file order
    std::vector<Action> exit;     // the `exit` block, in file order
    std::vector<std::string> goto_state;  // state-level transitions, in file order
    std::vector<Action> messages;         // state-level `message` actions
};

class Table {
public:
    // Finds the `trigger` resource with instance 0xD6396913 across the package search paths and
    // parses it. Never throws; a missing resource yields an empty table and a log line saying so
    // (and, per this session's lesson, the log names the paths that were searched).
    bool load();

    bool loaded() const { return m_loaded; }
    const std::vector<GameState>& states() const { return m_states; }
    const GameState* find(const std::string& name) const;
    const GameState* find_by_id(u32 id) const;
    size_t mode_count() const;
    std::string entry_state() const;   // the state with -id 0, else the first named "Main"
    std::string stats() const;
    const std::string& source() const { return m_source; }

private:
    bool m_loaded = false;
    std::vector<GameState> m_states;
    std::string m_source;
};

Table& instance();

// The UI verbs a state's script carries. The recovered script contains exactly ONE UI command in
// the whole file - state DevLauncher's `cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"`
// - and its twin `-unload` on exit. This is the seam between "we render a layout" and "the state
// machine drives the UI": the script names the screen, and spui::Reader::find_by_name() resolves
// that name to a resource instance (E3LaunchScreen -> 0xE932BD44, measured).
//
// The two trailing ids are KEPT, not discarded. They are not resource ids (they appear in no
// type/group/instance field of any of the 150,945 records in the corpus) and no name in the 131,854
// registry names hashes to them; the first appears twice inside the spui byte corpus, the second
// nowhere. So they are internal to the layout or runtime handles - UNRESOLVED, and dropping them
// would make that look like a decision rather than an open question.
struct UiCommand {
    std::string name;      // the layout name, verbatim from the script
    u32 id1 = 0;           // the first trailing id, unexplained
    u32 id2 = 0;           // the second trailing id, unexplained
    bool unload = false;   // `-unload` (teardown) rather than `-load`
};
std::vector<UiCommand> ui_commands(const GameState& gs);

// Registers every game state into our AppStateManager analog, so `-state:TribalAccessoriesEditor`
// resolves to the real state with the real id. States whose name collides with one we already own
// (Loader, Title) are skipped with a log line rather than silently shadowing ours.
void install(os::states::Manager& mgr);

}  // namespace os::appstates
