#pragma once
// Cell-stage game-data reader — decodes Data/CellGame.package (the cell mini-game).
//
// The cell stage ("Game_Cell", see analysis/game-stages.md) is defined ENTIRELY by data, not
// code: its archetypes, tuning globals, steering/loot/spawn tables and animation list all ship
// in CellGame.package (442 resources) + CellGame_Layers.package. This header exposes the pieces
// a playable cell stage needs (spawn archetypes, biome worlds, drop tables).
//
// All integers inside a resource are little-endian (DBPF headers/indexes too). Instance and
// resource-type ids are FNV-1-lower hashes of their names (common.h fnv1_lower; verified on
// cell/structure/world/globals/look_table/loottable and instance names such as cell_squid), so
// they resolve through os::naming against reg_file.txt.
//
// Resource types decoded here:
//   0xDFAD9F51 cell            — creature/prop archetype, 400-byte record
//   0xD92AF091 loottable       — cell loot / part-unlock table (docs/LOOTTABLE_FORMAT.md)
//   0x8C042499 look_table      — steering weight table (docs/LOOKTABLE_FORMAT.md)
//   0x9B8E862F world           — tide-pool biome (counted)
//   0xF9C3D770 random_creature — random spawn table (counted)
//   0xDA141C1B populate        — spawn population (counted)
//   0x4B9EF6DC structure       — part layout (header + N 32-byte part entries)
//   0x2A3CE5B7 globals         — master tuning record (16 id refs + ~31 floats)
//   0x04805684 advect          — 128x128x3 f32 background flow field (Layers pkg)
//   0x5C74D18B density         — 128x128 f32 background density field (Layers pkg)
// Anything else is tallied by type name only.
//
// Archetype record layout (400 B, little-endian), offsets proven to vary across the 189
// shipped records — the float slots are POSITIONAL, not keyed, so they are exposed by offset
// rather than under invented stat names (MEMORY.md rule: names do not prove behavior):
//   +0x000 u32 template_id   shared by variants (cell_chomper == Cell_ChomperMed == 0x7BF0B6F5)
//   +0x004 UTF-16LE display  NUL-terminated creature name ("Snorf", "Chomper")
//   +0x0A4 u16 part_count    low 16 bits of the word (high 16 bits are 0xCCCC padding on most)
//   +0x0C0..+0x0D8 u32 refs  template/skeleton/animation ids
//   +0x0E0..+0x18C f32 stats interleaved with small flag ints; see cell::kStatOffsets
//
// Read-only. No writes; no game-state mutation.
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "common.h"

namespace os::cell {

// Resource type ids (FNV-1-lower of their names).
inline constexpr u32 kTypeCell = 0xDFAD9F51u;
inline constexpr u32 kTypeLootTable = 0xD92AF091u;
inline constexpr u32 kTypeLookTable = 0x8C042499u;
inline constexpr u32 kTypeWorld = 0x9B8E862Fu;
inline constexpr u32 kTypeRandomCreature = 0xF9C3D770u;
inline constexpr u32 kTypePopulate = 0xDA141C1Bu;
inline constexpr u32 kTypeStructure = 0x4B9EF6DCu;
inline constexpr u32 kTypeGlobals = 0x2A3CE5B7u;
inline constexpr u32 kTypeAdvect = 0x04805684u;
inline constexpr u32 kTypeDensity = 0x5C74D18Bu;
inline constexpr u32 kTypeBackgroundMap = 0x612B3191u;
inline constexpr u32 kTypeProp = 0x00B1B104u;

struct LookEntry {
    u32 type = 0;      // see LOOK_ENTRY: 0 up, 1 forward, ... 11 player
    float weight = 0.0f;
};

struct LookTable {
    u32 id = 0;
    std::string name;  // reg_file.txt name, or "" when unknown
    std::vector<LookEntry> entries;
};

struct LootEntry {
    u32 type = 0;          // 0 nothing, 1 cell, 2 table
    u32 cell = 0;          // archetype/part id (e.g. cell_unlock_mandible)
    u32 table = 0;         // nested table id when type==2
    float weight = 0.0f;
    i32 count = 0;
    i32 count_delta = 0;
    i32 level_offset = 0;
};

struct LootTable {
    u32 id = 0;
    std::string name;
    float min_radius = 0.0f, max_radius = 0.0f, initial_alpha = 0.0f, expel_force = 0.0f;
    u32 effect = 0;
    float delay = 0.0f;
    std::vector<LootEntry> entries;
};

// The `ai` cAIData block of a cell archetype, at record +0xe0. Field offsets are the REA
// descriptor's (cell type descriptor FUN_01375e30 @0x1375e30; cAIData builder @0x1374725); the
// block is 0xb4 bytes. Values verified against the shipped records (e.g. Cell_DuckyMed: speed 11,
// chaseSpeed 8, wanderSpeed 15, food 180, growAmount 0.4, hatchDuration 0.4, chaseTime 10).
struct AiData {
    float awareness_radius = 0.0f;            // +0x04
    float awareness_radius_food = 0.0f;       // +0x08
    float awareness_radius_predator = 0.0f;   // +0x0c
    u32   movement_style = 0;                 // +0x10
    float flocking = 0.0f;                    // +0x14
    float speed = 0.0f;                       // +0x18
    float chase_speed = 0.0f;                 // +0x1c
    float wander_speed = 0.0f;                // +0x20
    float flee_speed = 0.0f;                  // +0x24
    float fears_nearby_damage_radius = 0.0f;  // +0x28
    float fears_nearby_death_radius = 0.0f;   // +0x2c
    float fears_nearby_damage_time = 0.0f;    // +0x30
    float fears_nearby_death_time = 0.0f;     // +0x34
    float protect_radius = 0.0f;              // +0x38
    float protect_time = 0.0f;                // +0x3c
    float turn_factor = 0.0f;                 // +0x40
    float axial_movement = 0.0f;              // +0x44
    float spawn_time = 0.0f;                  // +0x48
    float spawn_rest_time = 0.0f;             // +0x4c
    float spawn_output = 0.0f;                // +0x50
    float arc_length = 0.0f;                  // +0x54
    float arc_length_secondary = 0.0f;        // +0x58
    float num_arcs = 0.0f;                    // +0x5c
    u32   key_transformation = 0;             // +0x60
    u32   key_projectile = 0;                 // +0x64
    float food = 0.0f;                        // +0x68
    float grow_count = 0.0f;                  // +0x6c
    float digestion_count = 0.0f;             // +0x70
    float digestion_time = 0.0f;              // +0x74
    float digestion_output = 0.0f;            // +0x78
    float flee_time = 0.0f;                   // +0x7c
    float flee_rest_time = 0.0f;              // +0x80
    float chase_time = 0.0f;                  // +0x84
    float chase_rest_time = 0.0f;             // +0x88
    u8    chases_damage = 0;                  // +0x8c
    u8    fears_mouths = 0;                   // +0x8d
    u8    fears_weapons = 0;                  // +0x8e
    u8    fears_damage = 0;                   // +0x8f
    u8    fears_electric = 0;                 // +0x90
    u8    fears_poison = 0;                   // +0x91
    u8    ignores_food = 0;                   // +0x92
    float awake_time = 0.0f;                  // +0x94
    float sleep_time = 0.0f;                  // +0x98
    float grow_amount = 0.0f;                 // +0x9c
    float hatch_duration = 0.0f;              // +0xa0
    float poison_recharge = 0.0f;             // +0xa4
    float electric_recharge = 0.0f;           // +0xa8
    float electric_recharge_vs_small = 0.0f;  // +0xac
    float electric_discharge = 0.0f;          // +0xb0
};

// Top-level cell-archetype scalars (REA descriptor FUN_01375e30 @0x1375e30). Offsets are byte
// offsets in the 400-byte record; the fields up to +0x0dc fit the shipped record.
struct CellScalars {
    u32 hp = 0;                // +0x0a8
    u32 fixed_orientation = 0; // +0x0ac
    u32 flags = 0;             // +0x0b0
    u32 cell_type = 0;         // +0x0b4
    u32 unlock_type = 0;       // +0x0b8
    u32 density = 0;           // +0x0bc
    u32 sound = 0;             // +0x0c0
    u32 break_id = 0;          // +0x0c4
    u32 pieces = 0;            // +0x0c8
    u32 leak = 0;              // +0x0cc
    u32 expel = 0;             // +0x0d0
    u32 loot = 0;              // +0x0d4
    u32 explosion_table = 0;   // +0x0d8
    u32 poison = 0;            // +0x0dc
};

struct Archetype {
    u32 id = 0;
    u32 template_id = 0;   // record +0x000
    u16 part_count = 0;    // record +0x0A4 (low 16 bits)
    std::string display;   // record +0x004, UTF-16LE
    std::string name;      // reg_file.txt name for `id`, or "" when unknown
    std::vector<u8> record;  // the full 400-byte record
    AiData ai;             // record +0xe0 (`ai` cAIData block)
    CellScalars scalars;   // record +0x0a8..+0x0dc

    // Read a little-endian f32 at a documented stat offset (returns 0 if out of range).
    float stat(u32 offset) const;
    u32 dword(u32 offset) const;
};

// Named resources that are decoded as counts only (world).
struct NamedResource {
    u32 id = 0;
    std::string name;
    u32 size = 0;
};

// One part attached to a structure body. Part entry: +0x00 parent (-1 = root),
// +0x04 flag, +0x10 part id, +0x14 scale xyz.
struct StructurePart {
    u32 parent = 0xFFFFFFFFu;
    u32 flag = 0;
    u32 part = 0;
    float scale[3] = {0.0f, 0.0f, 0.0f};
};

// Cell body model (`structure`, e.g. the layout of cell_hero). 80-byte header then N 32-byte
// part entries. Header: +0x0C format marker, +0x10 death-archetype id, +0x40 base-body id,
// +0x44 scale xyz.
struct Structure {
    u32 id = 0;
    std::string name;
    u32 death = 0;  // header +0x10 (spawned when the cell dies)
    u32 base = 0;   // header +0x40 (base body: amoeba_cell/...)
    float scale[3] = {0.0f, 0.0f, 0.0f};
    std::vector<StructurePart> parts;
};

// Master tuning record (`globals`, 196 B): 16 resource-id references then ~31 floats from
// +0x40. The floats are POSITIONAL (no shipped key names), so they are exposed raw.
struct Globals {
    u32 id = 0;
    std::string name;
    std::vector<u32> refs;    // 16 id references at +0x00..+0x3C
    std::vector<float> floats;  // tuning floats from +0x40
};

// A tuning curve from the prop/cell_properties resource (type 0x00B1B104).
// Each curve maps a named parameter to (x, y) pairs for interpolation.
struct PropertyCurve {
    std::string key;
    std::vector<std::pair<float, float>> points;
};

// A background advection/density field from CellGame_Layers.package.
// advect = 128x128x3 f32 (196608 B), density = 128x128 f32 (65536 B).
struct LayerField {
    u32 id = 0;
    std::string name;
    u32 components = 0;  // 3 = advect (flow), 1 = density
    u32 width = 128, height = 128;
    u32 size = 0;
    std::vector<u8> data;  // the raw payload (little-endian f32), for tiles/textures
};

// One parallax depth band from `backgroundMap` (8-byte header + 12 x {r,g,b,depth} f32).
struct BackgroundBand {
    float r = 0.0f, g = 0.0f, b = 0.0f, depth = 0.0f;
};

// A single spawn line: which cell archetype, and how many / at what weight.
struct SpawnEntry {
    u32 cell = 0;      // cell archetype id (populate) or referenced id (random spawn)
    float count = 0;   // populate: spawn count; random_creature: weight
};

// Spawn population (Data/CellGame.package type `populate`, e.g. level_water).
// 16-byte header {type, ?, entryCount, 16}; entryCount 64-byte entries each with a cell
// archetype id at +0x18 and a spawn count at +0x2C.
struct Population {
    u32 id = 0;
    std::string name;
    u32 type = 0;  // header +0x00: 0 background, 3, 4 creature level
    std::vector<SpawnEntry> entries;
};

// Random spawn table (`random_creature`, e.g. neutralCells -> NeutralCell_01..03).
// A list of id/weight pairs; weight is the f32 following each known archetype id.
struct RandomSpawn {
    u32 id = 0;
    std::string name;
    u32 size = 0;
    std::vector<SpawnEntry> entries;
};

struct GameData {
    std::vector<Archetype> archetypes;
    std::vector<LookTable> look_tables;
    std::vector<LootTable> loot_tables;
    std::vector<Population> populations;
    std::vector<RandomSpawn> random_spawns;
    std::vector<NamedResource> worlds;
    std::vector<Structure> structures;
    std::vector<Globals> globals;
    std::vector<LayerField> layers;
    std::vector<BackgroundBand> background;  // `backgroundMap` parallax bands (12 typical)
    std::vector<PropertyCurve> properties;   // `prop`/`cell_properties` tuning curves
    std::map<u32, u32> type_counts;  // type id -> resource count (every type in the package)
    u32 total_resources = 0;
};

// Diet tags from the binary tuning vocabulary (Simulator/Cell/Data).
// These classify a cell archetype's diet for the food chain.
inline constexpr const char* kDietCarnivore = "cell_carnivore";
inline constexpr const char* kDietHerbivore = "cell_herbivore";
inline constexpr const char* kDietOmnivore = "cell_omnivore";

// Animation event names from the xls animation table (type 0x1F639D98).
// These map game events to animation clips for the cell stage.
inline constexpr const char* kAnimMove = "ANIM_MOVE";
inline constexpr const char* kAnimIdle = "ANIM_IDLE";
inline constexpr const char* kAnimEat = "ANIM_EAT_JAW";
inline constexpr const char* kAnimEatFail = "ANIM_EAT_FAIL_JAW";
inline constexpr const char* kAnimDeath = "ANIM_DEATH";
inline constexpr const char* kAnimDead = "ANIM_DEAD";
inline constexpr const char* kAnimGrow = "ANIM_GROW";
inline constexpr const char* kAnimAttack = "ANIM_ATTACK_JAW";
inline constexpr const char* kAnimHatch = "ANIM_HATCH";
inline constexpr const char* kAnimPop = "ANIM_POP";
inline constexpr const char* kAnimZap = "ANIM_ZAP";
inline constexpr const char* kAnimScared = "ANIM_SCARED";
inline constexpr const char* kAnimMoveMad = "ANIM_MOVE_MAD";
inline constexpr const char* kAnimMoveScared = "ANIM_MOVE_SCARED";
inline constexpr const char* kAnimGetHitElec = "ANIM_GET_HIT_ELEC";
inline constexpr const char* kAnimGetHitPoison = "ANIM_GET_HIT_POISON";
inline constexpr const char* kAnimGetHitSpike = "ANIM_GET_HIT_SPIKE";
inline constexpr const char* kAnimPokeSpike = "ANIM_POKE_SPIKE";
inline constexpr const char* kAnimAttackSpike = "ANIM_ATTACK_SPIKE";
inline constexpr const char* kAnimAttackPosion = "ANIM_ATTACK_POSION";
inline constexpr const char* kAnimGlanceJaw = "ANIM_GLANCE_JAW";
inline constexpr const char* kAnimGlanceSpike = "ANIM_GLANCE_SPIKE";
inline constexpr const char* kAnimJoustSpike = "ANIM_JOUST_SPIKE";
inline constexpr const char* kAnimEatFilter = "ANIM_EAT_FILTER";
inline constexpr const char* kAnimEatFailFilter = "ANIM_EAT_FAIL_FILTER";
inline constexpr const char* kAnimEatProb = "ANIM_EAT_PROB";
inline constexpr const char* kAnimProbTargetExpel = "ANIM_PROB_TARGET_EXPEL";
inline constexpr const char* kAnimProbSourceExpelled = "ANIM_PROB_SOURCE_EXPELED";
inline constexpr const char* kAnimProbStun = "ANIM_PROB_STUN";
inline constexpr const char* kAnimStartled = "ANIM_STARTLED";

// Documented float offsets inside a cell archetype record (see header comment). These are
// offsets that were observed to carry gameplay-scale floats; the mapping to a specific stat is
// NOT proven, so callers read them raw via Archetype::stat().
inline constexpr u32 kStatOffsets[] = {
    0x0E0, 0x0E8, 0x0F4, 0x0F8, 0x0FC, 0x100, 0x104, 0x108, 0x10C, 0x110,
    0x114, 0x138, 0x140, 0x144, 0x148, 0x14C, 0x15C, 0x164, 0x16C, 0x17C, 0x180,
};

// Decode a CellGame.package from disk. Read-only.
bool decode(const std::string& path, GameData& out, std::string& error);
// Decode a package held in memory (tests; no filesystem dependency).
bool decode_from_memory(const u8* data, size_t size, const std::string& label,
                        GameData& out, std::string& error);

}  // namespace os::cell
