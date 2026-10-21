// Constructed-input tests for cell_game.cpp — no game data required.
// With an optional argv[1] = a real CellGame.package, also decodes it and prints a summary.
#include "engine/cell_game.h"
#include <cstring>
#include <iostream>
#include <vector>

using namespace os;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; }
}

struct Rec { u32 type, group, instance; std::vector<u8> payload; };

// Build a DBPF (all little-endian) with index kind 4: type/group per record,
// unknown shared in the prefix. Stride 28, prefix 8.
static std::vector<u8> make_dbpf(const std::vector<Rec>& recs) {
    u32 payload_off = 0x60;
    std::vector<u8> body;
    std::vector<u32> offs, sizes;
    for (auto& r : recs) {
        offs.push_back(payload_off + static_cast<u32>(body.size()));
        sizes.push_back(static_cast<u32>(r.payload.size()));
        body.insert(body.end(), r.payload.begin(), r.payload.end());
    }
    u32 index_offset = payload_off + static_cast<u32>(body.size());
    u32 index_size = 8 + 28 * static_cast<u32>(recs.size());
    std::vector<u8> out(index_offset + index_size, 0);
    memcpy(out.data(), "DBPF", 4);
    auto put32 = [&](size_t at, u32 v) { memcpy(out.data() + at, &v, 4); };
    put32(4, 3); put32(8, 0); put32(0x24, static_cast<u32>(recs.size()));
    put32(0x2C, index_size); put32(0x3C, 3); put32(0x40, index_offset);
    memcpy(out.data() + payload_off, body.data(), body.size());
    size_t ix = index_offset;
    put32(ix, 4); put32(ix + 4, 0);  // kind 4, shared unknown
    ix += 8;
    for (size_t k = 0; k < recs.size(); ++k, ix += 28) {
        put32(ix + 0, recs[k].type);
        put32(ix + 4, recs[k].group);
        put32(ix + 8, recs[k].instance);
        put32(ix + 12, offs[k]);
        put32(ix + 16, 0x80000000u | sizes[k]);
        put32(ix + 20, sizes[k]);
        put32(ix + 24, 0x00010000u);  // raw + flags 1
    }
    return out;
}

static void put_le32(std::vector<u8>& v, u32 x) {
    v.push_back(x & 0xFF); v.push_back((x >> 8) & 0xFF);
    v.push_back((x >> 16) & 0xFF); v.push_back((x >> 24) & 0xFF);
}
static void put_le_f32(std::vector<u8>& v, float f) {
    u32 bits; memcpy(&bits, &f, 4); put_le32(v, bits);
}

static std::vector<u8> make_archetype() {
    std::vector<u8> d(400, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0x000, 0xBABB7F95u);              // template id
    const char* name = "Snorf";           // UTF-16LE at 0x004
    for (size_t i = 0; name[i]; ++i) { d[4 + i * 2] = name[i]; }
    u16 pc = 0x26; memcpy(d.data() + 0x0A4, &pc, 2);   // part_count
    float f = 2.0f; u32 fb; memcpy(&fb, &f, 4); put(0x0E0, fb);
    return d;
}

static std::vector<u8> make_look_table() {
    std::vector<u8> d;
    put_le32(d, 8); put_le32(d, 2);        // off=8, count=2
    put_le32(d, 1); put_le_f32(d, 4.0f);   // forward 4.0
    put_le32(d, 5); put_le_f32(d, 0.25f);  // flee_target 0.25
    return d;
}

static std::vector<u8> make_loot_table() {
    std::vector<u8> d;
    put_le32(d, 36); put_le32(d, 1);       // off=36, count=1
    put_le_f32(d, 1.0f); put_le_f32(d, 2.0f); put_le_f32(d, 0.5f); put_le_f32(d, 3.0f);
    put_le32(d, 0x12345678u);              // effect
    put_le32(d, 1);                        // mustHavePart
    put_le_f32(d, 0.5f);                   // delay
    put_le32(d, 1);                        // type cell
    put_le32(d, fnv1_lower("cell_unlock_mandible"));
    put_le32(d, 0);                        // table
    put_le_f32(d, 90.0f);                  // weight
    put_le32(d, 1); put_le32(d, 0); put_le32(d, 1);
    return d;
}

static std::vector<u8> make_population() {
    std::vector<u8> d(16 + 2 * 64, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0, 4); put(8, 2);                 // header: type 4, 2 entries
    put(16 + 0x18, fnv1_lower("cell_food_L0")); put(16 + 0x2C, 0);   // count 5.0 below
    float c5 = 5.0f, c9 = 9.0f;
    memcpy(d.data() + 16 + 0x2C, &c5, 4);
    put(16 + 64 + 0x18, fnv1_lower("cell_pinky"));
    memcpy(d.data() + 16 + 64 + 0x2C, &c9, 4);
    return d;
}

static std::vector<u8> make_world() {
    // A `world` record: a 4-word header, then two 12-byte level slots.
    std::vector<u8> d(16 + 2 * 12, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0x00, 2); put(0x04, 0x10); put(0x08, 0); put(0x0C, 0);
    put(0x10, fnv1_lower("level_water")); put(0x14, 1); put(0x18, 0xFFFFFFFFu);
    put(0x1C, fnv1_lower("level_water_2")); put(0x20, 0); put(0x24, 0xFFFFFFFFu);
    return d;
}

static std::vector<u8> make_structure() {
    std::vector<u8> d(0x50 + 32, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0x0C, 0x46748976u);                       // format marker
    put(0x10, fnv1_lower("cell_deathNPC"));       // death archetype
    put(0x40, fnv1_lower("amoeba_cell"));         // base body
    float s = 1.0f; memcpy(d.data() + 0x44, &s, 4);
    put(0x50 + 0x00, 0xFFFFFFFFu);                // root parent
    put(0x50 + 0x10, fnv1_lower("CG_poison_L1")); // part id
    float px = 0.1f; memcpy(d.data() + 0x50 + 0x14, &px, 4);
    return d;
}

static std::vector<u8> make_globals() {
    std::vector<u8> d(196, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0x00, fnv1_lower("cell_food_L0"));        // ref 0
    put(0x40, fnv1_lower("CellBackgrounds"));     // ref 16 (the last of the leading ids)
    float f5 = 5.0f; memcpy(d.data() + 0x44, &f5, 4);   // first float
    float f2 = 0.5f; memcpy(d.data() + 0x48, &f2, 4);
    put(0x80, fnv1_lower("default_look_algorithm"));  // the id between the two float runs
    float f3 = 1.0f; memcpy(d.data() + 0x84, &f3, 4);
    put(0xBC, 10);   // numHighLOD_FG
    put(0xC0, 2);    // numHighLOD_BG
    return d;
}

static std::vector<u8> make_background() {
    std::vector<u8> d;
    put_le32(d, 0); put_le32(d, 2);               // 8-byte header
    put_le_f32(d, 0.75f); put_le_f32(d, 0.90f); put_le_f32(d, 0.92f); put_le_f32(d, 0.0f);
    put_le_f32(d, 0.00f); put_le_f32(d, 0.10f); put_le_f32(d, 0.55f); put_le_f32(d, 15.0f);
    return d;
}

static std::vector<u8> make_density() {
    std::vector<u8> d(128 * 128 * 4, 0);
    auto put = [&](size_t at, float f) { memcpy(d.data() + at, &f, 4); };
    put(0, 0.5f);
    put((128 * 129 / 2) * 4, 0.9f);
    return d;
}

// The `prop` property table (tools/prop.py): u32 BE entry_count, then per entry
// {u32 BE key, u32 BE type, u32 BE item_count, u32 BE item_size} + item data. A float2 curve has
// type 0x0030009C, item_size 8, and little-endian (x, y) points. `time_dilation` is the shipped
// key 0x18B75C47 (reg_property.txt).
static std::vector<u8> make_prop() {
    std::vector<u8> d;
    auto put_be32 = [&](u32 x) {
        d.push_back((x >> 24) & 0xFF); d.push_back((x >> 16) & 0xFF);
        d.push_back((x >> 8) & 0xFF); d.push_back(x & 0xFF);
    };
    put_be32(1);                        // one entry
    put_be32(0x18B75C47u);              // time_dilation
    put_be32(0x0030009Cu);              // float2 array
    put_be32(2);                        // 2 points
    put_be32(8);                        // 8 bytes/point
    put_le_f32(d, 0.5f); put_le_f32(d, 4.0f);
    put_le_f32(d, 1.0f); put_le_f32(d, 2.0f);
    return d;
}

int main(int argc, char** argv) {
    const u32 squid = fnv1_lower("cell_squid");
    const u32 booster = fnv1_lower("loot_booster");
    const u32 npc_idle = fnv1_lower("NPC_idle");

    // 1: archetype
    {
        std::vector<Rec> recs = {{cell::kTypeCell, 0, squid, make_archetype()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode archetype");
        check(g.archetypes.size() == 1, "one archetype");
        check(g.archetypes[0].template_id == 0xBABB7F95u, "template id");
        check(g.archetypes[0].display == "Snorf", "display name");
        check(g.archetypes[0].part_count == 0x26, "part count");
        check(g.archetypes[0].stat(0x0E0) == 2.0f, "stat 0x0E0");
        check(g.archetypes[0].name.empty() || g.archetypes[0].name == "cell_squid", "instance name");
        check(g.total_resources == 1, "total resources");
    }

    // 2: look_table
    {
        std::vector<Rec> recs = {{cell::kTypeLookTable, 0, npc_idle, make_look_table()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode look_table");
        check(g.look_tables.size() == 1, "one look_table");
        check(g.look_tables[0].entries.size() == 2, "two look entries");
        check(g.look_tables[0].entries[0].type == 1, "look entry 0 type");
        check(g.look_tables[0].entries[0].weight == 4.0f, "look entry 0 weight");
        check(g.look_tables[0].entries[1].weight == 0.25f, "look entry 1 weight");
    }

    // 3: loot_table
    {
        std::vector<Rec> recs = {{cell::kTypeLootTable, 0, booster, make_loot_table()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode loot_table");
        check(g.loot_tables.size() == 1, "one loot_table");
        check(g.loot_tables[0].entries.size() == 1, "one loot entry");
        check(g.loot_tables[0].entries[0].weight == 90.0f, "loot weight");
        check(g.loot_tables[0].entries[0].cell == fnv1_lower("cell_unlock_mandible"), "loot cell id");
        check(g.loot_tables[0].effect == 0x12345678u, "loot effect");
        check(g.loot_tables[0].must_have_part == 1, "loot mustHavePart");
        check(g.loot_tables[0].delay == 0.5f, "loot delay");
    }

    // 4: mixed package type counts
    {
        std::vector<Rec> recs = {
            {cell::kTypeCell, 0, squid, make_archetype()},
            {cell::kTypeLootTable, 0, booster, make_loot_table()},
            {cell::kTypeLookTable, 0, npc_idle, make_look_table()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode mixed");
        check(g.total_resources == 3, "three resources");
        check(g.type_counts.size() == 3, "three distinct types");
        check(g.archetypes.size() == 1 && g.loot_tables.size() == 1 && g.look_tables.size() == 1,
              "one of each decoded");
    }

    // 5: invalid DBPF rejected
    {
        std::vector<u8> bad = {0x4E, 0x4F, 0x54, 0x44};
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(bad.data(), bad.size(), "bad", g, err);
        check(!ok, "invalid DBPF rejected");
        check(!err.empty(), "error present");
    }

    // 6: spawn population
    {
        std::vector<Rec> recs = {{cell::kTypePopulate, 0, fnv1_lower("level_water"), make_population()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode population");
        check(g.populations.size() == 1, "one population");
        check(g.populations[0].type == 4, "population type");
        check(g.populations[0].entries.size() == 2, "two spawn entries");
        check(g.populations[0].entries[0].cell == fnv1_lower("cell_food_L0"), "spawn cell 0");
        check(g.populations[0].entries[0].count == 5.0f, "spawn count 0");
        check(g.populations[0].entries[1].count == 9.0f, "spawn count 1");
        check(g.populations[0].name.empty() || g.populations[0].name == "level_water", "pop name");
    }

    // 7: structure (header + parts)
    {
        std::vector<Rec> recs = {{cell::kTypeStructure, 0, fnv1_lower("cell_hero"), make_structure()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode structure");
        check(g.structures.size() == 1, "one structure");
        check(g.structures[0].death == fnv1_lower("cell_deathNPC"), "structure death id");
        check(g.structures[0].base == fnv1_lower("amoeba_cell"), "structure base id");
        check(g.structures[0].scale[0] == 1.0f, "structure scale");
        check(g.structures[0].parts.size() == 1, "one structure part");
        check(g.structures[0].parts[0].parent == 0xFFFFFFFFu, "part parent root");
        check(g.structures[0].parts[0].part == fnv1_lower("CG_poison_L1"), "part id");
    }

    // 8: globals
    {
        std::vector<Rec> recs = {{cell::kTypeGlobals, 0, fnv1_lower("cellgame"), make_globals()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode globals");
        check(g.globals.size() == 1, "one globals");
        check(g.globals[0].refs.size() == 18, "18 globals refs");
        check(g.globals[0].refs[0] == fnv1_lower("cell_food_L0"), "globals ref 0");
        check(g.globals[0].refs[16] == fnv1_lower("CellBackgrounds"), "globals ref 16");
        check(g.globals[0].refs[17] == fnv1_lower("default_look_algorithm"), "globals ref 17");
        check(g.globals[0].floats.size() == 29, "globals float count");
        check(g.globals[0].floats[0] == 5.0f && g.globals[0].floats[1] == 0.5f, "globals floats");
        check(g.globals[0].floats[15] == 1.0f, "globals float after the mid-record id");
        check(g.globals[0].num_high_lod_fg == 10 && g.globals[0].num_high_lod_bg == 2,
              "globals numHighLOD");
    }

    // 9: world level slots
    {
        std::vector<Rec> recs = {{cell::kTypeWorld, 0, fnv1_lower("world_water"), make_world()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode world");
        check(g.worlds.size() == 1, "one world");
        check(g.worlds[0].level_count == 2, "world level count");
        check(g.worlds[0].levels.size() == 2, "world level slots");
        if (g.worlds[0].levels.size() == 2) {
            check(g.worlds[0].levels[0].ref == fnv1_lower("level_water"), "world level slot 0");
            check(g.worlds[0].levels[0].flag == 1, "world level slot 0 flag");
            check(g.worlds[0].levels[1].ref == fnv1_lower("level_water_2"), "world level slot 1");
            check(g.worlds[0].levels[1].flag == 0, "world level slot 1 flag");
        }
    }

    // 10: backgroundMap parallax bands
    {
        std::vector<Rec> recs = {{cell::kTypeBackgroundMap, 0, fnv1_lower("CellBackgrounds"),
                                  make_background()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode backgroundMap");
        check(g.background.size() == 2, "two background bands");
        check(g.background[0].r == 0.75f && g.background[0].g == 0.90f, "band 0 rgb");
        check(g.background[1].depth == 15.0f, "band 1 depth");
    }

    // 11: a density layer keeps its payload (for the backdrop texture)
    {
        std::vector<Rec> recs = {{cell::kTypeDensity, 0, fnv1_lower("Level1-zone1"), make_density()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode density layer");
        check(g.layers.size() == 1, "one layer");
        check(g.layers[0].components == 1, "density components");
        check(g.layers[0].data.size() == 65536, "density payload retained");
    }

    // 12: prop property table — the real tuning-curve format
    {
        std::vector<Rec> recs = {{cell::kTypeProp, 0, 0xC951EB63u, make_prop()}};
        auto dbpf = make_dbpf(recs);
        cell::GameData g; std::string err;
        bool ok = cell::decode_from_memory(dbpf.data(), dbpf.size(), "test", g, err);
        check(ok, "decode prop");
        check(g.properties.size() == 1, "one prop curve");
        if (!g.properties.empty()) {
            check(g.properties[0].key == "time_dilation", "prop curve keyed by property name");
            check(g.properties[0].points.size() == 2, "prop curve point count");
            check(g.properties[0].points[0].first == 0.5f && g.properties[0].points[0].second == 4.0f,
                  "prop curve point 0");
            check(g.properties[0].points[1].first == 1.0f && g.properties[0].points[1].second == 2.0f,
                  "prop curve point 1");
        }
    }

    // Optional: real package.
    if (argc > 1 && argv[1] && argv[1][0]) {
        cell::GameData g; std::string err;
        if (cell::decode(argv[1], g, err)) {
            std::cout << "real package: " << g.total_resources << " resources, "
                      << g.archetypes.size() << " archetypes, " << g.look_tables.size()
                      << " look_tables, " << g.loot_tables.size() << " loottables, "
                      << g.populations.size() << " populations, " << g.random_spawns.size()
                      << " random_spawns, " << g.worlds.size() << " worlds, "
                      << g.structures.size() << " structures, " << g.globals.size()
                      << " globals, " << g.background.size() << " background bands, "
                      << g.layers.size() << " layers, " << g.properties.size()
                      << " property curves\n";
            // The shipped package carries all 17 tuning curves; assert a known one resolves.
            bool have_time = false;
            for (const auto& c : g.properties) if (c.key == "time_dilation") have_time = true;
            check(have_time, "real package has time_dilation curve");
        } else {
            std::cerr << "real package decode failed: " << err << "\n";
            ++failures;
        }
    }

    if (failures) {
        std::cerr << failures << " cell_game_verify check(s) failed\n";
        return 1;
    }
    std::cout << "cell_game_verify: 12 constructed-input checks PASS\n";
    return 0;
}
