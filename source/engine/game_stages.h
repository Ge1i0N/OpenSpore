#pragma once
// Playable stage drivers - the first step of the gameplay work.
//
// A `mode` is a named C++ driver class registered by a per-class factory under a TAG
// (analysis/modes.md): implementing the ten rows of engine/appmodes.cpp means implementing
// ten driver classes, which IS the gameplay work. This header owns the five PLAYABLE-stage
// rows plus the first behaviour: a Game_Cell driver attached to the CellGame state that
// loads CellGame.package and spawns a level's cells (see engine/cell_world.h).
//
// Evidence (all level-1, GA oracle refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe):
//   * Registration calls: Game_Cell tag 0x01654C00 from 0x00e616c0 (obj 0x0c),
//     Game_Creature 0x01654C01 / 0x00d3bd20 (0xe8), Game_Tribe 0x01654C02 / 0x00cdb4e0
//     (0x3a8), Game_Civ 0x01654C04 / 0x00cfa480 (0x150), Game_Space 0x01654C05 /
//     0x00fddb30 (0x198). See analysis/modes.md.
//   * Stage/state pairing and the Cell -> Creature -> Tribe -> Civ -> Space progression:
//     the shipped SporeAppStates trigger (instance 0xD6396913), extracted verbatim to
//     analysis/scripts/SporeAppStates.txt. See analysis/states.md.
//   * CellGame's own block is only `mode Game_Cell` (no UI layout, no messages), so a
//     skeleton driver that ticks without simulating is the honest first step, not a stub
//     pretending to be the tide pool.
//
// WHAT THIS IS NOT: simulation, rendering, input, save/load, or any other stage driver.
// has_driver() is true for Game_Cell only; the other four rows parse but have no behaviour.
#include <string>

#include "common.h"

namespace os::stages {

// One row per playable stage: the binary registration beside the state the script pairs it
// with. `next` is the stage the shipped script forwards to (CellGame -> CreatureGame via
// CreatureGameFromCellGame, etc.); the last stage has an empty next.
struct Stage {
    const char* mode;       // the literal the registrar passes, e.g. "Game_Cell"
    u32 tag;                // MODE TAG marker address (identity is the address, not a counter)
    u32 registrar;          // factory that allocates, constructs and registers the driver
    u32 object_size;        // bytes the factory allocates (0x0c for Game_Cell: just a vtable)
    const char* state;      // state whose script says `mode <mode>`, e.g. "CellGame"
    const char* next;       // progression successor state, or "" for the last stage
    int order;              // Cell 0 .. Space 4
};

const Stage* table();
size_t count();

// The mode/state a script or caller names, or null. Case-insensitive: the script and the
// binary agree in spelling but not necessarily in case across builds (same rule as
// appmodes::find_by_name).
const Stage* find_by_mode(const char* mode);
const Stage* find_by_state(const char* state);

// One line for the log: mode, tag, registrar, object size and the driven state.
std::string describe(const Stage& st);

// True only for modes with an attached skeleton driver (today: Game_Cell alone).
bool has_driver(const char* mode);

// Cell-stage lifecycle. cell_enter() loads Data/CellGame.package (best effort) and spawns the
// default level; cell_tick() advances the world; cell_exit() parks. Deliberately D3D-free so
// the offline tools/game_stages_verify.cpp exercises the whole contract without a device.
// Without the data it degrades to a tick counter (reported, not hidden).
void cell_enter();
void cell_exit();
void cell_tick(f32 dt);
u64 cell_ticks();
std::string cell_status();

// The live cell world status (loaded package + spawned cells), or "no world data" when
// Data/CellGame.package is not on the search paths. D3D-free (see engine/cell_world.h).
std::string cell_world_status();
bool cell_world_loaded();

}  // namespace os::stages
