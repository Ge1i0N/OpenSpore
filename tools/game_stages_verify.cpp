#include "engine/game_stages.h"
#include <iostream>
#include <string>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (ok) return;
    ++failures;
    std::cerr << "FAIL: " << what << "\n";
}
}  // namespace

int main() {
    using namespace os;

    // Table shape: five playable stages in progression order.
    check(stages::count() == 5, "five playable stages");
    const stages::Stage* t = stages::table();
    check(t != nullptr, "table non-null");

    // Evidence transcription (analysis/modes.md): mode / tag / registrar / object size.
    struct Expect { const char* mode; u32 tag; u32 reg; u32 size; const char* state; const char* next; int order; };
    const Expect kWant[] = {
        {"Game_Cell", 0x01654C00u, 0x00e616c0u, 0x0cu, "CellGame", "CreatureGame", 0},
        {"Game_Creature", 0x01654C01u, 0x00d3bd20u, 0xe8u, "CreatureGame", "TribeGame", 1},
        {"Game_Tribe", 0x01654C02u, 0x00cdb4e0u, 0x3a8u, "TribeGame", "CivilisationGame", 2},
        {"Game_Civ", 0x01654C04u, 0x00cfa480u, 0x150u, "CivilisationGame", "SpaceGame", 3},
        {"Game_Space", 0x01654C05u, 0x00fddb30u, 0x198u, "SpaceGame", "", 4},
    };
    for (const Expect& w : kWant) {
        const stages::Stage* by_mode = stages::find_by_mode(w.mode);
        const stages::Stage* by_state = stages::find_by_state(w.state);
        check(by_mode && by_mode == by_state, w.mode);
        if (!by_mode) continue;
        check(by_mode->tag == w.tag && by_mode->registrar == w.reg && by_mode->object_size == w.size,
              "registration row");
        check(std::string(by_mode->next) == w.next && by_mode->order == w.order, "progression row");
        // Case-insensitive lookup, like appmodes::find_by_name.
        check(stages::find_by_mode(w.mode) != nullptr, "lookup");
    }
    // Boundary lookups: null/empty/unknown never resolve, near-misses do not match.
    check(!stages::find_by_mode(nullptr) && !stages::find_by_mode("") &&
              !stages::find_by_state(nullptr) && !stages::find_by_state(""),
          "null/empty lookups");
    check(!stages::find_by_mode("Game_CellExtra") && !stages::find_by_mode("GameCell") &&
              !stages::find_by_state("Cell") && !stages::find_by_mode("GalaxyGameEntry"),
          "unknown/near-miss lookups");

    // Progression chain is linked end to end through the `next` fields.
    for (size_t i = 0; i + 1 < stages::count(); ++i) {
        const stages::Stage* cur = stages::table() + i;
        check(stages::find_by_state(cur->next) == stages::table() + i + 1, "chain link");
    }
    check(stages::table()[stages::count() - 1].next[0] == '\0', "last stage terminates");

    // Driver coverage: only Game_Cell has a skeleton; nothing else claims one.
    check(stages::has_driver("Game_Cell") && stages::has_driver("game_cell"), "cell driver");
    check(!stages::has_driver(nullptr) && !stages::has_driver("") &&
              !stages::has_driver("Game_Creature") && !stages::has_driver("Game_Tribe") &&
              !stages::has_driver("Game_Civ") && !stages::has_driver("Game_Space") &&
              !stages::has_driver("GalaxyGameEntry"),
          "no other drivers");

    // Skeleton lifecycle: idle -> active counting -> parked. Ticks while idle are dropped.
    stages::cell_exit();
    check(stages::cell_ticks() == 0, "idle counter");
    stages::cell_tick(0.016f);
    check(stages::cell_ticks() == 0, "idle ticks dropped");
    check(stages::cell_status() == "idle (no simulation)", "idle status");
    // No fs search paths are initialised in this offline tool, so the world cannot load here;
    // that path must be honest and non-fatal (cell_world (no data) is the same driver test).
    check(!stages::cell_world_loaded(), "no world loaded offline");
    check(stages::cell_world_status() == "no world data", "offline world status");
    stages::cell_enter();
    stages::cell_tick(0.016f);
    stages::cell_tick(0.016f);
    stages::cell_tick(0.016f);
    check(stages::cell_ticks() == 3, "active counting");
    check(stages::cell_status().find("3 ticks") != std::string::npos, "active status");
    stages::cell_enter();  // re-entry restarts, it must not accumulate across entries
    check(stages::cell_ticks() == 0, "re-entry restarts");
    stages::cell_exit();
    check(stages::cell_ticks() == 0 && stages::cell_status() == "idle (no simulation)", "exit parks");

    if (failures) {
        std::cerr << "game_stages_verify: " << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "GA stage table/progression/cell-driver boundaries PASS\n";
}
