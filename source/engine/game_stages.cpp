#include "engine/game_stages.h"

#include <cstdio>

#include "engine/cell_world.h"
#include "engine/log.h"

namespace os::stages {
namespace {

// THE TABLE. Five playable stages; each row joins two independent artifacts that agree:
// the binary's registration call (tag/registrar/object size) and the shipped
// SporeAppStates script (mode/state/next). See analysis/modes.md and analysis/states.md.
const Stage kStages[] = {
    {"Game_Cell", 0x01654C00u, 0x00e616c0u, 0x0cu, "CellGame", "CreatureGame", 0},
    {"Game_Creature", 0x01654C01u, 0x00d3bd20u, 0xe8u, "CreatureGame", "TribeGame", 1},
    {"Game_Tribe", 0x01654C02u, 0x00cdb4e0u, 0x3a8u, "TribeGame", "CivilisationGame", 2},
    {"Game_Civ", 0x01654C04u, 0x00cfa480u, 0x150u, "CivilisationGame", "SpaceGame", 3},
    {"Game_Space", 0x01654C05u, 0x00fddb30u, 0x198u, "SpaceGame", "", 4},
};

bool same(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return false;
    }
    return *a == *b;
}

// Skeleton state: active between cell_enter() and cell_exit(), counting ticks.
bool g_active = false;
u64 g_ticks = 0;
// The cell world: data decoded from CellGame.package + the cells spawned for a level. Empty
// when the data is not on the search paths (D3D-free; see engine/cell_world.h).
cell::World g_world;
bool g_world_tried = false;
}  // namespace

const Stage* table() { return kStages; }
size_t count() { return sizeof(kStages) / sizeof(kStages[0]); }

const Stage* find_by_mode(const char* mode) {
    if (!mode || !*mode) return nullptr;
    for (const Stage& st : kStages) {
        if (same(st.mode, mode)) return &st;
    }
    return nullptr;
}

const Stage* find_by_state(const char* state) {
    if (!state || !*state) return nullptr;
    for (const Stage& st : kStages) {
        if (same(st.state, state)) return &st;
    }
    return nullptr;
}

std::string describe(const Stage& st) {
    char b[240];
    std::snprintf(b, sizeof(b),
                  "stage %-13s tag=0x%08X reg=0x%08X obj=%3u B state='%s' next='%s' order=%d",
                  st.mode, st.tag, st.registrar, st.object_size,
                  st.state, st.next[0] ? st.next : "-", st.order);
    return b;
}

bool has_driver(const char* mode) {
    // One skeleton today. The comparison is by MODE NAME (the script's namespace), not by
    // tag: tags are binary addresses, names are what states ask for.
    return same(mode, "Game_Cell");
}

void cell_enter() {
    g_active = true;
    g_ticks = 0;
    // Load the cell data once, then spawn the default level. A miss is reported, not fatal:
    // the stage still ticks so the offline verify (no data) exercises the same contract.
    if (!g_world.loaded()) {
        std::string err;
        if (g_world.load_default(err)) {
            std::string lerr;
            if (g_world.set_level("level_water", lerr)) {
                LOG_INFO("stages", "cell world: %s", g_world.status().c_str());
            } else {
                LOG_WARN("stages", "cell world loaded but level spawn failed: %s", lerr.c_str());
            }
            // Load saved progress if available.
            std::string save_err;
            if (g_world.load_save("cell_progress.spo", save_err)) {
                LOG_INFO("stages", "cell progress restored: %s", g_world.status().c_str());
            } else if (!save_err.empty()) {
                LOG_INFO("stages", "no cell progress: %s", save_err.c_str());
            }
        } else if (!g_world_tried) {
            LOG_WARN("stages", "cell world unavailable: %s", err.c_str());
        }
        g_world_tried = true;
    }
}

void cell_exit() {
    g_active = false;
    g_ticks = 0;
    g_world.clear();
    g_world_tried = false;
}

void cell_tick(f32 dt) {
    if (!g_active) return;
    ++g_ticks;
    g_world.tick(dt);
}

u64 cell_ticks() { return g_ticks; }

std::string cell_status() {
    if (!g_active) return "idle (no simulation)";
    char b[224];
    std::snprintf(b, sizeof(b), "active: %llu ticks; %s",
                  (unsigned long long)g_ticks, g_world.status().c_str());
    return b;
}

std::string cell_world_status() { return g_world.status(); }

bool cell_world_loaded() { return g_world.loaded(); }

cell::World& cell_world() { return g_world; }

}  // namespace os::stages
