#pragma once
// Cell-stage runtime: turns the decoded CellGame.package data into a live world.
//
// This is the first step that UNITES the two halves of the cell work:
//   * `cell_game.{h,cpp}` decodes the shipped data (archetypes, populations, structures, ...);
//   * this module instantiates that data into entities and advances them each tick.
//
// What is evidence-backed (level 1):
//   * the spawn list is the `populate` resource for the level: each entry names a `cell`
//     archetype id and a spawn count (record +0x18 / +0x2C, see analysis/cell-game.md);
//   * an archetype's body model is the `structure` whose id equals the archetype's
//     template id at record +0x000 (verified across the shipped records).
//
// What is a SEED, not the original (stated, not hidden): per-entity positions, velocities
// and the arena bounds are placeholders - the original's placement algorithm is not
// recovered. `tick()` therefore only integrates a constant drift; it is a deterministic
// sandbox that proves the data flows through the engine, not the real tide-pool simulation.
//
// Read-only with respect to shipped data; the world state is ours. D3D-free so an offline
// tool (tools/cell_world_verify.cpp) exercises it without a device.
#include <string>
#include <vector>

#include "common.h"
#include "engine/cell_game.h"

namespace os::cell {

// One spawned cell: which archetype, its body-structure index, and a placeholder pose.
struct Entity {
    u32 archetype = 0;    // cell archetype id
    int structure = -1;   // index into GameData::structures, or -1
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float size = 1.0f;    // body scale (from the structure header, else 1)
};

class World {
public:
    // Decode a CellGame.package from an explicit path. Read-only.
    bool load_path(const std::string& path, std::string& error);
    // Decode from memory (tests; no filesystem dependency).
    bool load_memory(const u8* data, size_t size, const std::string& label, std::string& error);
    // Resolve Data/CellGame.package through os::fs and decode it. Missing data is an error,
    // not a crash: the caller reports "no world data" and keeps running.
    bool load_default(std::string& error);

    // Spawn the entities for a `populate` level (e.g. "level_water"). Returns false and sets
    // `error` when the level is unknown to the package. A no-op with an error when not loaded.
    bool set_level(const std::string& level, std::string& error);

    // Integrate one step. Dropped (counter only) while no world is loaded.
    void tick(float dt);
    void clear();

    bool loaded() const { return m_loaded; }
    const GameData& data() const { return m_data; }
    const std::string& level() const { return m_level; }
    const std::string& source() const { return m_source; }
    const std::vector<Entity>& entities() const { return m_entities; }
    int player_index() const { return m_player; }
    const Entity* player() const;
    u64 ticks() const { return m_ticks; }
    float time() const { return m_time; }
    float arena() const { return m_arena; }

    // One line: level, cell count, player. Honest about "no world data".
    std::string status() const;
    // The structure index that an archetype's +0x000 template id names, or -1.
    int structure_of(const Archetype& a) const;

private:
    GameData m_data;
    bool m_loaded = false;
    std::string m_source;
    std::string m_level;
    std::vector<Entity> m_entities;
    int m_player = -1;
    u64 m_ticks = 0;
    float m_time = 0.0f;
    float m_arena = 60.0f;
};

}  // namespace os::cell
