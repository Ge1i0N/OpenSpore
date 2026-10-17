#pragma once
// Cell-stage runtime: turns the decoded CellGame.package data into a live, playable world.
//
// This is the first step that UNITES the two halves of the cell work:
//   * `cell_game.{h,cpp}` decodes the shipped data (archetypes, populations, structures, ...);
//   * this module instantiates that data into entities and simulates a simple food chain.
//
// What is evidence-backed (level 1):
//   * the spawn list is the `populate` resource for the level: each entry names a `cell`
//     archetype id and a spawn count (record +0x18 / +0x2C, see analysis/cell-game.md);
//   * an archetype's body model is the `structure` whose id equals the archetype's template
//     id at record +0x000 (verified across the shipped records).
//
// What is a SEED, not the original (stated, not hidden): per-entity positions, sizes,
// velocities, the food/creature split, growth rates, HP, predator aggression, the camera and
// the arena are OUR placeholders - the original's numbers are not recovered (the archetype
// stat slots are positional and unnamed). The point is a deterministic, data-driven loop that
// is genuinely playable and machine-checkable, not the shipped tide-pool balance.
//
// Read-only with respect to shipped data; the world state is ours. D3D-free so an offline
// tool (tools/cell_world_verify.cpp) exercises it without a device.
#include <string>
#include <vector>

#include "common.h"
#include "engine/cell_game.h"

namespace os::cell {

// What an entity is for the food chain. Food is eaten; a Creature is eaten (prey) when it is
// smaller than the player and bites (predator) when it is larger.
enum class Role : u8 { Food = 0, Creature = 1 };

// One spawned cell: which archetype, its body-structure index, and its simulation state.
struct Entity {
    u32 archetype = 0;    // cell archetype id
    int structure = -1;   // index into GameData::structures, or -1
    Role role = Role::Creature;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float size = 1.0f;    // body radius (world units) - seed, not a decoded stat
    bool alive = true;    // cleared when eaten
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

    // Spawn the entities for a `populate` level (e.g. "level_water") and place the player.
    // Returns false and sets `error` when the level is unknown. A no-op error when not loaded.
    bool set_level(const std::string& level, std::string& error);

    // Player steering: accelerate toward a world-space target (a mouse position, normally).
    void set_target(float wx, float wy);
    void clear_target();

    // Dev/test affordance: move the player to a world point (zeroing its velocity). Used by
    // tools/cell_world_verify.cpp to drive collisions deterministically; no-op with no player.
    void teleport_player(float wx, float wy);

    // Integrate one step: steering, predator/prey AI, collisions, eating/growth, HP, respawn,
    // camera. Dropped (counter only) while no world is loaded.
    void tick(float dt);
    void clear();

    bool loaded() const { return m_loaded; }
    const GameData& data() const { return m_data; }
    const std::string& level() const { return m_level; }
    const std::string& source() const { return m_source; }
    const std::vector<Entity>& entities() const { return m_entities; }
    int player_index() const { return m_player; }
    const Entity* player() const;

    // Player state (placeholders; see the header note).
    bool player_alive() const { return m_player_alive; }
    float player_hp() const { return m_hp; }
    float player_max_hp() const { return m_max_hp; }
    int eaten() const { return m_eaten; }
    bool evolved() const { return m_evolved; }

    // Camera: world -> screen is (world - camera) * pixels_per_unit + screen_center.
    float camera_x() const { return m_cam_x; }
    float camera_y() const { return m_cam_y; }
    float pixels_per_unit() const;

    u64 ticks() const { return m_ticks; }
    float time() const { return m_time; }
    float arena() const { return m_arena; }

    // One line: level, cell count, player, HP. Honest about "no world data".
    std::string status() const;
    // The structure index that an archetype's +0x000 template id names, or -1.
    int structure_of(const Archetype& a) const;

private:
    void spawn_from_level(const Population& pop);
    void respawn_player();

    GameData m_data;
    bool m_loaded = false;
    std::string m_source;
    std::string m_level;
    std::vector<Entity> m_entities;
    int m_player = -1;
    u64 m_ticks = 0;
    float m_time = 0.0f;
    float m_arena = 60.0f;

    // Player + camera state.
    bool m_player_alive = true;
    float m_hp = 100.0f, m_max_hp = 100.0f;
    int m_eaten = 0;
    bool m_evolved = false;
    float m_respawn = 0.0f;
    bool m_has_target = false;
    float m_target_x = 0.0f, m_target_y = 0.0f;
    float m_cam_x = 0.0f, m_cam_y = 0.0f;
};

}  // namespace os::cell
