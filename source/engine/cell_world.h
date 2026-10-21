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
#include "engine/cell_anims.inc"

namespace os::cell {

// What an entity is for the food chain. Food is eaten (a plant/plankton mote); Meat is a chunk a
// cell drops when it is eaten (the carnivore diet); a Creature is eaten (prey) when it is smaller
// than the player and bites (predator) when it is larger. Prop is the level's `_bg` background
// population (cell_jawhead/puffish/megamouth): scenery that drifts but neither hunts nor is eaten.
enum class Role : u8 { Food = 0, Creature = 1, Meat = 2, Prop = 3 };

// The player's diet: which food the player has been eating. A seed model (the original tracks it
// through the mouth parts chosen in the cell editor), but it makes eating read as a choice.
enum class Diet : u8 { Herbivore = 0, Carnivore = 1, Omnivore = 2 };

// A simple particle for visual effects (eat, death, evolve).
struct Particle {
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float life = 0.0f;
    u32 color = 0xFFFFFFFF;
};

// One spawned cell: which archetype, its body-structure index, and its simulation state.
struct Entity {
    u32 archetype = 0;    // cell archetype id
    int structure = -1;   // index into GameData::structures, or -1
    Role role = Role::Creature;
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float size = 1.0f;    // body radius (world units) - from the archetype's cAIData growAmount
    float hp = 0.0f;      // current HP (0 = dead); NPCs can be damaged and die
    float max_hp = 0.0f;  // max HP
    float chase_speed = 0.0f;   // archetype cAIData chaseSpeed
    float flee_speed = 0.0f;    // archetype cAIData fleeSpeed
    float wander_speed = 0.0f;  // archetype cAIData wanderSpeed
    float turn_factor = 0.0f;   // archetype cAIData turnFactor
    float aware_food = 0.0f;      // archetype cAIData awarenessRadiusFood
    float aware_predator = 0.0f;  // archetype cAIData awarenessRadiusPredator
    float ttl = 0.0f;     // seconds left before a dropped meat chunk rots (0 = never)
    bool alive = true;    // cleared when eaten
    const char* anim = kAnimIdle;  // current animation event (from xls table)
    float anim_time = 0.0f;        // time in current animation
    int diet = -1;                 // 0=herbivore, 1=carnivore, 2=omnivore, -1=unknown
    float protect_time = 0.0f;     // remaining invulnerability seconds
    float poison_time = 0.0f;      // remaining poison damage-over-time seconds
    float electric_time = 0.0f;     // remaining electric stun seconds
    float hatch_time = 0.0f;       // seconds until this egg hatches (0 = not an egg)
    bool unlocked = false;         // whether this entity has been unlocked as a part
    float anim_duration = 0.0f;    // duration of current animation clip
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
    // Load a .spo save file to restore player position and stats.
    bool load_save(const std::string& path, std::string& error);
    // Write a .spo save file with current progress (DNA, eaten, unlocked parts).
    bool write_save(const std::string& path, std::string& error) const;
    // Select a part for the cell editor (before evolve). Returns true if the part was unlocked.
    bool select_part(const std::string& part_name);
    // Get the currently selected part.
    const std::string& selected_part() const { return m_selected_part; }
    // List all available parts (unlocked or not) with their names and costs.
    struct PartInfo {
        std::string name;
        std::string display;
        int cost;
        bool unlocked;
    };
    std::vector<PartInfo> parts_list() const;
    // Get the list of selected parts.
    const std::vector<std::string>& selected_parts() const { return m_selected_parts; }
    // Toggle a part selection (add/remove from selected_parts).
    bool toggle_part(const std::string& part_name);
    // Clear all selected parts.
    void clear_selected_parts();
    // Play a procedural sound effect (simple tone).
    void play_sound(const char* name);
    // Get the name of the last played sound (for UI display).
    const std::string& last_sound() const { return m_last_sound; }
    // Spawn particles at a world position (for eat, death, evolve effects).
    void spawn_particles(float x, float y, int count, u32 color);
    // Get active particles for rendering.
    const std::vector<Particle>& particles() const { return m_particles; }

    // Electric chain: find nearby creatures to chain to.
    std::vector<const Entity*> electric_targets(const Entity& source, float range) const;
    // Poison cloud: find creatures within poison radius.
    std::vector<const Entity*> poison_targets(const Entity& source, float range) const;

    // Spawn the entities for a `populate` level (e.g. "level_water") and place the player.
    // Returns false and sets `error` when the level is unknown. A no-op error when not loaded.
    bool set_level(const std::string& level, std::string& error);

    // Player steering: accelerate toward a world-space target (a mouse position, normally).
    void set_target(float wx, float wy);
    void clear_target();

    // Dev/test affordance: move the player to a world point (zeroing its velocity). Used by
    // tools/cell_world_verify.cpp to drive collisions deterministically; no-op with no player.
    void teleport_player(float wx, float wy);
    // Dev/test affordance: reset the player's protection window.
    void reset_protection();

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
    int dna() const { return m_dna; }
    Diet diet() const { return m_diet; }
    int eaten_plant() const { return m_eaten_plant; }
    int eaten_meat() const { return m_eaten_meat; }
    bool evolved() const { return m_evolved; }
    int evo_points() const { return m_evo_points; }

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

    // Background fields from CellGame_Layers.package (optional). Look up a layer by name, or the
    // first with a given component count (1 = density, 3 = advect), or null when none was loaded.
    const LayerField* layer_named(const std::string& name) const;
    const LayerField* first_layer(u32 components) const;

    // Look up a steering table by name (e.g. "NPC_idle", "player_moving"). Null when absent.
    const LookTable* look_table(const char* name) const;
    // Get a steering weight from a table by entry type (0=up, 1=forward, ..., 11=player).
    float look_weight(const char* table_name, u32 entry_type) const;

    // Look up a property tuning curve by name (e.g. "hp", "chaseSpeed"). Null when absent.
    const PropertyCurve* property_curve(const char* name) const;
    // Evaluate a property curve at x (linear interpolation). Returns default_val if not found.
    float curve_value(const char* name, float x, float default_val = 0.0f) const;

    // List available world biomes (level names from `world` resources).
    std::vector<std::string> biome_names() const;
    // List available level names from `populate` resources.
    std::vector<std::string> level_names() const;
    // The shipped biome list, taken from the `CellGame` globals record's world references.
    std::vector<std::string> level_list() const;
    // Get the next level name in progression (e.g. "level_water" -> "level_water_2").
    std::string next_level(const std::string& current) const;

    // Get the structure parts for an archetype (part entries with parent/flag/scale).
    const std::vector<StructurePart>* parts_for(u32 archetype_id) const;
    // Get the loot table for a biome (e.g. "loot_water" for level_water).
    const LootTable* biome_loot(const char* biome_name) const;

private:
    void spawn_from_level(const Population& pop);
    void respawn_player();
    void drop_meat(const Entity& from);
    void update_diet();
    float hp_for_size(float size) const;
    float stat_for_archetype(u32 archetype_id, u32 offset) const;
    bool is_predator_archetype(u32 archetype_id) const;
    void drop_loot(const Entity& from);

    GameData m_data;
    bool m_loaded = false;
    std::string m_source;
    std::string m_level;
    std::vector<Entity> m_entities;
    // Food archetypes captured at spawn time, so the tide pool can be restocked as it is grazed.
    std::vector<Entity> m_food_templates;
    int m_target_food = 0;
    float m_refill = 0.0f;
    int m_player = -1;
    u64 m_ticks = 0;
    float m_time = 0.0f;
    float m_arena = 60.0f;

    // Player + camera state.
    bool m_player_alive = true;
    float m_hp = 100.0f, m_max_hp = 100.0f;
    int m_eaten = 0;
    int m_dna = 0;
    Diet m_diet = Diet::Herbivore;
    int m_eaten_plant = 0, m_eaten_meat = 0;
    bool m_evolved = false;
    int m_evo_points = 0;
    std::string m_selected_part;
    std::vector<std::string> m_selected_parts;
    std::vector<Particle> m_particles;
    std::string m_last_sound;
    float m_respawn = 0.0f;
    bool m_has_target = false;
    float m_target_x = 0.0f, m_target_y = 0.0f;
    float m_cam_x = 0.0f, m_cam_y = 0.0f;
};

}  // namespace os::cell
