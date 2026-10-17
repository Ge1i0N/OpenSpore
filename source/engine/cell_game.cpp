#include "engine/cell_game.h"

#include <cstring>

#include "engine/naming.h"
#include "engine/pkg.h"

namespace os::cell {
namespace {

u32 le32(const u8* p) { return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24); }
i32 le_i32(const u8* p) { return static_cast<i32>(le32(p)); }
u16 le16(const u8* p) { return static_cast<u16>(u32(p[0]) | (u32(p[1]) << 8)); }

f32 le_f32(const u8* p) {
    u32 bits = le32(p);
    f32 f;
    memcpy(&f, &bits, 4);
    return f;
}

// A NUL-terminated UTF-16LE run at `off` inside a cell archetype -> ASCII name.
std::string utf16_name(const std::vector<u8>& d, size_t off) {
    std::string s;
    for (size_t i = off; i + 1 < d.size(); i += 2) {
        u16 ch = le16(d.data() + i);
        if (ch == 0 && !s.empty()) break;
        if (ch >= 32 && ch < 127) {
            s.push_back(static_cast<char>(ch));
        } else if (!s.empty()) {
            break;
        }
    }
    return s;
}

Archetype parse_archetype(u32 id, const std::vector<u8>& d) {
    Archetype a;
    a.id = id;
    a.record = d;
    if (d.size() >= 4) a.template_id = le32(d.data());
    a.display = utf16_name(d, 4);
    if (d.size() >= 0x0A6) a.part_count = le16(d.data() + 0x0A4);
    const char* n = naming::instance_name(id);
    if (n) a.name = n;
    return a;
}

LookTable parse_look_table(u32 id, const std::vector<u8>& d) {
    LookTable t;
    t.id = id;
    if (const char* n = naming::instance_name(id)) t.name = n;
    if (d.size() < 8) return t;
    i32 off = le_i32(d.data());
    i32 cnt = le_i32(d.data() + 4);
    for (i32 k = 0; k < cnt; ++k) {
        size_t at = static_cast<size_t>(off) + static_cast<size_t>(k) * 8;
        if (at + 8 > d.size()) break;
        LookEntry e;
        e.type = le32(d.data() + at);
        e.weight = le_f32(d.data() + at + 4);
        t.entries.push_back(e);
    }
    return t;
}

LootTable parse_loot_table(u32 id, const std::vector<u8>& d) {
    LootTable t;
    t.id = id;
    if (const char* n = naming::instance_name(id)) t.name = n;
    if (d.size() < 36) return t;
    i32 off = le_i32(d.data());
    i32 cnt = le_i32(d.data() + 4);
    t.min_radius = le_f32(d.data() + 8);
    t.max_radius = le_f32(d.data() + 12);
    t.initial_alpha = le_f32(d.data() + 16);
    t.expel_force = le_f32(d.data() + 20);
    t.effect = le32(d.data() + 24);
    t.delay = le_f32(d.data() + 32);
    for (i32 k = 0; k < cnt; ++k) {
        size_t at = static_cast<size_t>(off) + static_cast<size_t>(k) * 28;
        if (at + 28 > d.size()) break;
        LootEntry e;
        e.type = le32(d.data() + at);
        e.cell = le32(d.data() + at + 4);
        e.table = le32(d.data() + at + 8);
        e.weight = le_f32(d.data() + at + 12);
        e.count = le_i32(d.data() + at + 16);
        e.count_delta = le_i32(d.data() + at + 20);
        e.level_offset = le_i32(d.data() + at + 24);
        t.entries.push_back(e);
    }
    return t;
}

NamedResource named_resource(u32 id, const std::vector<u8>& d) {
    NamedResource r;
    r.id = id;
    r.size = static_cast<u32>(d.size());
    if (const char* n = naming::instance_name(id)) r.name = n;
    return r;
}

Structure parse_structure(u32 id, const std::vector<u8>& d) {
    Structure s;
    s.id = id;
    if (const char* n = naming::instance_name(id)) s.name = n;
    if (d.size() >= 0x50) {
        s.death = le32(d.data() + 0x10);
        s.base = le32(d.data() + 0x40);
        s.scale[0] = le_f32(d.data() + 0x44);
        s.scale[1] = le_f32(d.data() + 0x48);
        s.scale[2] = le_f32(d.data() + 0x4C);
    }
    for (size_t base = 0x50; base + 32 <= d.size(); base += 32) {
        StructurePart p;
        p.parent = le32(d.data() + base);
        p.flag = le32(d.data() + base + 4);
        p.part = le32(d.data() + base + 0x10);
        p.scale[0] = le_f32(d.data() + base + 0x14);
        p.scale[1] = le_f32(d.data() + base + 0x18);
        p.scale[2] = le_f32(d.data() + base + 0x1C);
        s.parts.push_back(p);
    }
    return s;
}

Globals parse_globals(u32 id, const std::vector<u8>& d) {
    Globals g;
    g.id = id;
    if (const char* n = naming::instance_name(id)) g.name = n;
    for (size_t o = 0; o + 4 <= d.size() && o < 0x40; o += 4) g.refs.push_back(le32(d.data() + o));
    for (size_t o = 0x40; o + 4 <= d.size(); o += 4) g.floats.push_back(le_f32(d.data() + o));
    return g;
}

LayerField parse_layer(u32 id, const std::vector<u8>& d, u32 components) {
    LayerField f;
    f.id = id;
    f.components = components;
    f.size = static_cast<u32>(d.size());
    if (const char* n = naming::instance_name(id)) f.name = n;
    f.data = d;  // keep the payload so a caller can build a background tile
    return f;
}

// `backgroundMap`: 8-byte header then 16-byte bands of {r,g,b,depth} f32 (see tools/cell_game.py).
std::vector<BackgroundBand> parse_background(const std::vector<u8>& d) {
    std::vector<BackgroundBand> bands;
    for (size_t o = 8; o + 16 <= d.size(); o += 16) {
        BackgroundBand b;
        b.r = le_f32(d.data() + o);
        b.g = le_f32(d.data() + o + 4);
        b.b = le_f32(d.data() + o + 8);
        b.depth = le_f32(d.data() + o + 12);
        bands.push_back(b);
    }
    return bands;
}

Population parse_population(u32 id, const std::vector<u8>& d) {
    Population p;
    p.id = id;
    if (const char* n = naming::instance_name(id)) p.name = n;
    if (d.size() < 16) return p;
    p.type = le32(d.data());
    u32 cnt = le32(d.data() + 8);
    if (cnt == 0) return p;
    size_t stride = (d.size() - 16) / cnt;
    for (u32 k = 0; k < cnt; ++k) {
        size_t at = 16 + static_cast<size_t>(k) * stride;
        if (at + 0x30 > d.size()) break;
        SpawnEntry e;
        e.cell = le32(d.data() + at + 0x18);
        e.count = le_f32(d.data() + at + 0x2C);
        p.entries.push_back(e);
    }
    return p;
}

RandomSpawn parse_random_spawn(u32 id, const std::vector<u8>& d) {
    RandomSpawn r;
    r.id = id;
    r.size = static_cast<u32>(d.size());
    if (const char* n = naming::instance_name(id)) r.name = n;
    for (size_t o = 0; o + 8 <= d.size(); o += 4) {
        u32 v = le32(d.data() + o);
        if (!naming::instance_name(v)) continue;
        float w = le_f32(d.data() + o + 4);
        if (w > 0.0f && w <= 1000.0f) r.entries.push_back(SpawnEntry{v, w});
    }
    return r;
}

bool decode_package(pkg::Package& p, GameData& out, std::string& error) {
    naming::load();
    const auto& recs = p.records();
    for (size_t i = 0; i < recs.size(); ++i) {
        u32 t = recs[i].type();
        u32 id = recs[i].instance();
        out.type_counts[t] += 1;
        out.total_resources += 1;
        std::vector<u8> d;
        switch (t) {
        case kTypeCell:
        case kTypeLootTable:
        case kTypeLookTable:
        case kTypeWorld:
        case kTypeRandomCreature:
        case kTypePopulate:
        case kTypeStructure:
        case kTypeGlobals:
        case kTypeAdvect:
        case kTypeDensity:
        case kTypeBackgroundMap:
            if (!p.read_resource_decoded(static_cast<u32>(i), d)) {
                error = "resource read failed";
                return false;
            }
            break;
        default:
            continue;
        }
        switch (t) {
        case kTypeCell: out.archetypes.push_back(parse_archetype(id, d)); break;
        case kTypeLootTable: out.loot_tables.push_back(parse_loot_table(id, d)); break;
        case kTypeLookTable: out.look_tables.push_back(parse_look_table(id, d)); break;
        case kTypePopulate: out.populations.push_back(parse_population(id, d)); break;
        case kTypeRandomCreature: out.random_spawns.push_back(parse_random_spawn(id, d)); break;
        case kTypeWorld: out.worlds.push_back(named_resource(id, d)); break;
        case kTypeStructure: out.structures.push_back(parse_structure(id, d)); break;
        case kTypeGlobals: out.globals.push_back(parse_globals(id, d)); break;
        case kTypeAdvect: out.layers.push_back(parse_layer(id, d, 3)); break;
        case kTypeDensity: out.layers.push_back(parse_layer(id, d, 1)); break;
        case kTypeBackgroundMap: {
            std::vector<BackgroundBand> bands = parse_background(d);
            out.background.insert(out.background.end(), bands.begin(), bands.end());
            break;
        }
        default: break;
        }
    }
    error.clear();
    return true;
}

}  // namespace

float Archetype::stat(u32 offset) const {
    if (offset + 4 > record.size()) return 0.0f;
    return le_f32(record.data() + offset);
}

u32 Archetype::dword(u32 offset) const {
    if (offset + 4 > record.size()) return 0u;
    return le32(record.data() + offset);
}

bool decode(const std::string& path, GameData& out, std::string& error) {
    pkg::Package p;
    if (!p.open(path)) {
        error = p.why();
        return false;
    }
    return decode_package(p, out, error);
}

bool decode_from_memory(const u8* data, size_t size, const std::string& label,
                        GameData& out, std::string& error) {
    pkg::Package p;
    if (!p.open_from_memory(data, size, label)) {
        error = p.why();
        return false;
    }
    return decode_package(p, out, error);
}

}  // namespace os::cell
