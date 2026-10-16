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
//   0x4B9EF6DC structure       — part layout (counted)
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

struct Archetype {
    u32 id = 0;
    u32 template_id = 0;   // record +0x000
    u16 part_count = 0;    // record +0x0A4 (low 16 bits)
    std::string display;   // record +0x004, UTF-16LE
    std::string name;      // reg_file.txt name for `id`, or "" when unknown
    std::vector<u8> record;  // the full 400-byte record

    // Read a little-endian f32 at a documented stat offset (returns 0 if out of range).
    float stat(u32 offset) const;
    u32 dword(u32 offset) const;
};

// Named resources that are decoded as counts only (world/random_creature/populate/structure).
struct NamedResource {
    u32 id = 0;
    std::string name;
    u32 size = 0;
};

struct GameData {
    std::vector<Archetype> archetypes;
    std::vector<LookTable> look_tables;
    std::vector<LootTable> loot_tables;
    std::vector<NamedResource> worlds;
    std::vector<NamedResource> random_creatures;
    std::vector<NamedResource> populations;
    std::vector<NamedResource> structures;
    std::map<u32, u32> type_counts;  // type id -> resource count (every type in the package)
    u32 total_resources = 0;
};

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
