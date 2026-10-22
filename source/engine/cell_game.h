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
//   +0x0E0..+0x194 cAIData block (0xb4 B) - MIXED types (int/float/packed), see struct AiData
//   +0x194..+0x248 cAIData block for the "hard" difficulty (absent from the shipped record)
//   +0x248..+0x2FC cAIData block for the "easy" difficulty (absent from the shipped record)
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
    u32 must_have_part = 0;  // record +0x1c: the table only drops when the player has a part
    float delay = 0.0f;
    std::vector<LootEntry> entries;
};

// The `ai` cAIData block of a cell archetype, at record +0xe0. Field offsets are the REA
// descriptor's (cell type descriptor FUN_01375e30 @0x1375e30; cAIData builder @0x1374725); the
// block is 0xb4 bytes. Values verified against the shipped records (e.g. Cell_DuckyMed: speed 11,
// chaseSpeed 8, wanderSpeed 15, food 180, growAmount 0.4, hatchDuration 0.4, chaseTime 10).
// The `cAIData` block at record +0xE0 (the runtime structure is 0xb4 B; the shipped 400-byte
// record carries only its first 0xb0, so `electric_discharge` @+0xb0 is absent).
//
// The block is MIXED-TYPE, and the types below were measured over all 189 shipped archetypes: a
// field whose raw word is a small integer in every record is an integer, one whose raw word is a
// sane float bit pattern is a float, and the rest are kept raw. Reading an integer field as a
// float yields a denormal (turn_factor 3 read as f32 = 4.2e-45), which silently disabled the
// steering. Only the non-float fields are annotated; everything else is a float.
struct AiData {
    float awareness_radius = 0.0f;            // +0x04
    float awareness_radius_food = 0.0f;       // +0x08
    u32   awareness_radius_predator = 0;      // +0x0c  int  {0,4,8}
    u32   movement_style = 0;                 // +0x10  int
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
    u32   turn_factor = 0;                    // +0x40  int  {0,1,2,3}
    u32   axial_movement = 0;                 // +0x44  raw
    u32   spawn_time = 0;                     // +0x48  raw
    u32   spawn_rest_time = 0;                // +0x4c  int  {0,3}
    u32   spawn_output = 0;                   // +0x50  int
    u32   arc_length = 0;                     // +0x54  int  {0,1}
    float arc_length_secondary = 0.0f;        // +0x58
    u32   num_arcs = 0;                       // +0x5c  raw
    float key_transformation = 0.0f;          // +0x60
    float key_projectile = 0.0f;              // +0x64
    float food = 0.0f;                        // +0x68
    float grow_count = 0.0f;                  // +0x6c
    u32   digestion_count = 0;                // +0x70  raw (packed byte pair)
    u32   digestion_time = 0;                 // +0x74  int  {0,1}
    float digestion_output = 0.0f;            // +0x78
    float flee_time = 0.0f;                   // +0x7c
    u32   flee_rest_time = 0;                 // +0x80  int  {0,1}
    float chase_time = 0.0f;                  // +0x84
    float chase_rest_time = 0.0f;             // +0x88
    u8    chases_damage = 0;                  // +0x8c
    u8    fears_mouths = 0;                   // +0x8d
    u8    fears_weapons = 0;                  // +0x8e
    u8    fears_damage = 0;                   // +0x8f
    u8    fears_electric = 0;                 // +0x90
    u8    fears_poison = 0;                   // +0x91
    u8    ignores_food = 0;                   // +0x92
    u32   awake_time = 0;                     // +0x94  int  {0,1,10,20,21,30}
    u32   sleep_time = 0;                     // +0x98  int  {0,256}
    float grow_amount = 0.0f;                 // +0x9c
    float hatch_duration = 0.0f;              // +0xa0
    u32   poison_recharge = 0;                // +0xa4  int  {3,6,10,15}
    u32   electric_recharge = 0;              // +0xa8  int  {1,2}
    u32   electric_recharge_vs_small = 0;     // +0xac  int  {0,1,256}
    float electric_discharge = 0.0f;          // +0xb0  absent from the shipped record
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

    // Read a little-endian f32 at a raw record offset (returns 0 if out of range).
    float stat(u32 offset) const;
};

// One level slot inside a `world` record. The world record lists the level populations it
// hosts as 12-byte entries: the population id, a flag and an index.
struct LevelSlot {
    u32 ref = 0;     // the `populate` instance id (level_ice, level_ice_2, ...)
    u32 flag = 0;    // 1 on a playable world, 0 on a background one (see analysis/cell-game.md)
    u32 index = 0;   // the level's position (0xFFFFFFFF when the record does not order it)
};

// A `world` record (one biome). Layout, recovered from the shipped records:
//   +0x00 u32 level_count     the number of 12-byte level slots
//   +0x04 u32 0x10            a version word (0 in the empty `cell_empty` record)
//   +0x08 u32 extra_count     the number of 24-byte entries in the second list
//   +0x0C u32 extra_offset    the byte offset of that second list (0 when there is none)
//   +0x10 level_count * 12 B  the level slots: [populate id][flag][index]
// The second list's 24-byte entries are not decoded (their layout is not established).
struct WorldResource {
    u32 id = 0;
    std::string name;
    u32 size = 0;
    u32 level_count = 0;
    u32 extra_count = 0;
    u32 extra_offset = 0;
    std::vector<LevelSlot> levels;
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

// Master tuning record (`globals`, 196 B): 18 resource-id references interleaved with 29 floats.
// The floats are POSITIONAL (no shipped key names), so they are exposed raw. The references ARE
// named: they are the shipped world/biome list (world_ice, world_rock, world_plant,
// world_water, world_beach, then their _bg variants, world_random, world_random_bg), the player
// archetype (cell_hero_L0), the effect map (CellEffects), the background map (CellBackgrounds)
// and the default look algorithm.
struct Globals {
    u32 id = 0;
    std::string name;
    // The record interleaves ids and floats: 17 ids at +0x00..+0x40, 15 floats at
    // +0x44..+0x7C, one more id at +0x80, 14 floats at +0x84..+0xB8, then two integers.
    std::vector<u32> refs;    // the 18 id references, in record order
    std::vector<std::string> ref_names;  // the resolved name of each reference ("" when unknown)
    std::vector<float> floats;  // the 29 tuning floats, in record order
    // The record's last two words (10 and 2). The GA globals type names the same two slots
    // numHighLOD_FG / numHighLOD_BG, but the shipped record is a shorter revision of that
    // type, so the identification is INFERRED, not proven. Nothing consumes them.
    u32 tail_int0 = 0;   // +0xBC
    u32 tail_int1 = 0;   // +0xC0
};

// A tuning curve from the prop/cell_properties resource (type 0x00B1B104).
// Each curve maps a named parameter to (x, y) pairs for interpolation.
struct PropertyCurve {
    std::string key;
    std::vector<std::pair<float, float>> points;
};

// ---------------------------------------------------------------------------
// The GA cell stage's size-class model (recovered from the GA binary).
//
// The GA `cell` type registers a `size` field (record +0x304) that is a FLOAT with
// the type default 1.0, and the GA cell object keeps a size CLASS: `FUN_00e4ee60`
// maps a 0..1000 size integer to one of ten classes through the band table at
// 0x1483c14 (28 bytes per row). The row layout the GA reads is
// (class, band start, growth, hp, hp max, ?, 0):
//
//   class | band start | growth | hp    | max hp
//   ------+------------+--------+-------+--------
//     1   |     0      |   0    |  1.0  |  1.0
//     1   |    50      |   5    |  1.73 |  1.0
//     2   |   100      |  10    |  3.0  |  3.0
//     2   |   150      |  13    |  5.2  |  3.0
//     3   |   200      |  16    | 10.0  | 10.0
//     ...                       x3 per class, with the upper band at x sqrt(3)
//
// A size of exactly 1000 uses the separate row at 0x1483e60 (class 10). The class
// is therefore 1 for the sizes 0..99, 2 for 100..199, ... 10 for 900..1000.
//
// `FUN_00e57340` turns two classes into the GA's relation: clamp(a - b + 2, 0, 4),
// so 0 is "two classes smaller", 2 is "the same class" and 4 is "two classes
// bigger". The GA's AI uses it directly: an NPC hunts the player unless the
// relation is exactly 1 (`FUN_00e57ce0`) and treats the player as prey only at
// relation 4 (`FUN_00e59e50`).
// ---------------------------------------------------------------------------

// One row of the GA size-band table.
struct SizeBand {
    int size_class = 0;   // the class this band belongs to
    int threshold = 0;    // the size integer at which the band starts
    int growth = 0;       // the GA's per-band growth step
    float hp = 0.0f;      // the band's HP value
    float hp_max = 0.0f;  // the class's HP ceiling
};

// The 21 band rows, in table order (the GA's 0x1483c14 block plus the size == 1000 row).
extern const SizeBand kSizeBands[21];
// The separate row the GA uses for a size of exactly 1000 (0x1483e60).
extern const SizeBand kSizeBandMax;

// The GA's size class for a size integer (0..1000). `FUN_00e4ee60` @0xe4ee60.
int size_class_of(int size_int);
// The GA's class relation: clamp(a - b + 2, 0, 4). `FUN_00e57340` @0xe57340.
int size_relation(int class_a, int class_b);
// The class a body radius falls into. The GA's class ladder is the `hp_max` column of
// the band table (1, 3, 10, 30, 100, 300, 1000, 3000, 10000, 30000): a radius belongs
// to the class whose ceiling it is below. The GA expresses the class through the
// cell's size integer, which the engine does not carry, so this is the radius ->
// class mapping the engine uses.
int size_class_for_radius(float radius);
// The class's HP ceiling (the `hp_max` column of the band table).
float size_class_max_hp(int size_class);

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
    std::vector<WorldResource> worlds;
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

// Decode a CellGame.package from disk. Read-only.
bool decode(const std::string& path, GameData& out, std::string& error);
// Decode a package held in memory (tests; no filesystem dependency).
bool decode_from_memory(const u8* data, size_t size, const std::string& label,
                        GameData& out, std::string& error);

}  // namespace os::cell
