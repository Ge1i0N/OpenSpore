#include "engine/cell_game.h"

#include <cstdio>
#include <cstring>

#include "engine/naming.h"
#include "engine/pkg.h"

namespace os::cell {
namespace {

u32 le32(const u8* p) { return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24); }
i32 le_i32(const u8* p) { return static_cast<i32>(le32(p)); }
u16 le16(const u8* p) { return static_cast<u16>(u32(p[0]) | (u32(p[1]) << 8)); }

// Big-endian u32: the `prop` property table's headers are big-endian (tools/prop.py).
u32 be32(const u8* p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]); }

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

// The `ai` cAIData block at record +0xe0 (see AiData in cell_game.h).
void parse_ai(const std::vector<u8>& d, AiData& a) {
    constexpr size_t kBase = 0xe0;
    auto f = [&](size_t off) { return off + 4 <= d.size() ? le_f32(d.data() + off) : 0.0f; };
    auto dw = [&](size_t off) { return off + 4 <= d.size() ? le32(d.data() + off) : 0u; };
    auto b = [&](size_t off) { return off < d.size() ? d[off] : u8(0); };
    a.awareness_radius = f(kBase + 0x04);
    a.awareness_radius_food = f(kBase + 0x08);
    a.awareness_radius_predator = f(kBase + 0x0c);
    a.movement_style = dw(kBase + 0x10);
    a.flocking = f(kBase + 0x14);
    a.speed = f(kBase + 0x18);
    a.chase_speed = f(kBase + 0x1c);
    a.wander_speed = f(kBase + 0x20);
    a.flee_speed = f(kBase + 0x24);
    a.fears_nearby_damage_radius = f(kBase + 0x28);
    a.fears_nearby_death_radius = f(kBase + 0x2c);
    a.fears_nearby_damage_time = f(kBase + 0x30);
    a.fears_nearby_death_time = f(kBase + 0x34);
    a.protect_radius = f(kBase + 0x38);
    a.protect_time = f(kBase + 0x3c);
    a.turn_factor = f(kBase + 0x40);
    a.axial_movement = f(kBase + 0x44);
    a.spawn_time = f(kBase + 0x48);
    a.spawn_rest_time = f(kBase + 0x4c);
    a.spawn_output = f(kBase + 0x50);
    a.arc_length = f(kBase + 0x54);
    a.arc_length_secondary = f(kBase + 0x58);
    a.num_arcs = f(kBase + 0x5c);
    a.key_transformation = dw(kBase + 0x60);
    a.key_projectile = dw(kBase + 0x64);
    a.food = f(kBase + 0x68);
    a.grow_count = f(kBase + 0x6c);
    a.digestion_count = f(kBase + 0x70);
    a.digestion_time = f(kBase + 0x74);
    a.digestion_output = f(kBase + 0x78);
    a.flee_time = f(kBase + 0x7c);
    a.flee_rest_time = f(kBase + 0x80);
    a.chase_time = f(kBase + 0x84);
    a.chase_rest_time = f(kBase + 0x88);
    a.chases_damage = b(kBase + 0x8c);
    a.fears_mouths = b(kBase + 0x8d);
    a.fears_weapons = b(kBase + 0x8e);
    a.fears_damage = b(kBase + 0x8f);
    a.fears_electric = b(kBase + 0x90);
    a.fears_poison = b(kBase + 0x91);
    a.ignores_food = b(kBase + 0x92);
    a.awake_time = f(kBase + 0x94);
    a.sleep_time = f(kBase + 0x98);
    a.grow_amount = f(kBase + 0x9c);
    a.hatch_duration = f(kBase + 0xa0);
    a.poison_recharge = f(kBase + 0xa4);
    a.electric_recharge = f(kBase + 0xa8);
    a.electric_recharge_vs_small = f(kBase + 0xac);
    a.electric_discharge = f(kBase + 0xb0);
}

Archetype parse_archetype(u32 id, const std::vector<u8>& d) {
    Archetype a;
    a.id = id;
    a.record = d;
    if (d.size() >= 4) a.template_id = le32(d.data());
    a.display = utf16_name(d, 4);
    if (d.size() >= 0x0A6) a.part_count = le16(d.data() + 0x0A4);
    auto dw = [&](size_t off) { return off + 4 <= d.size() ? le32(d.data() + off) : 0u; };
    a.scalars.hp = dw(0x0a8);
    a.scalars.fixed_orientation = dw(0x0ac);
    a.scalars.flags = dw(0x0b0);
    a.scalars.cell_type = dw(0x0b4);
    a.scalars.unlock_type = dw(0x0b8);
    a.scalars.density = dw(0x0bc);
    a.scalars.sound = dw(0x0c0);
    a.scalars.break_id = dw(0x0c4);
    a.scalars.pieces = dw(0x0c8);
    a.scalars.leak = dw(0x0cc);
    a.scalars.expel = dw(0x0d0);
    a.scalars.loot = dw(0x0d4);
    a.scalars.explosion_table = dw(0x0d8);
    a.scalars.poison = dw(0x0dc);
    parse_ai(d, a.ai);
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
    t.must_have_part = le32(d.data() + 28);
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

// The `prop` property table (tools/prop.py): u32 BE entry_count, then per entry
// {u32 BE key, u32 BE type, u32 BE item_count, u32 BE item_size} followed by that much item data.
// A float2 curve has type 0x0030009C and item_size 8; its points are little-endian (x, y) pairs.
// Each entry's key is a hashed tuning-property id (reg_property.txt), so the curve is keyed by its
// resolved property NAME (e.g. "time_dilation"), which is what World::curve_value() looks up.
std::vector<PropertyCurve> parse_prop_curves(const std::vector<u8>& d) {
    std::vector<PropertyCurve> out;
    if (d.size() < 4) return out;
    const u32 count = be32(d.data());
    if (count == 0 || count > 100000) return out;
    size_t off = 4;
    for (u32 k = 0; k < count; ++k) {
        if (off + 16 > d.size()) break;
        const u32 key = be32(d.data() + off);
        const u32 type = be32(d.data() + off + 4);
        const u32 icnt = be32(d.data() + off + 8);
        const u32 isz = be32(d.data() + off + 12);
        off += 16;
        if (off + static_cast<size_t>(icnt) * isz > d.size()) break;
        if (type == 0x0030009Cu && isz == 8 && icnt >= 1) {
            PropertyCurve c;
            if (const char* n = naming::property_name(key)) {
                c.key = n;
            } else {
                char b[24];
                std::snprintf(b, sizeof(b), "0x%08X", key);
                c.key = b;
            }
            for (u32 j = 0; j < icnt; ++j) {
                const float x = le_f32(d.data() + off + static_cast<size_t>(j) * 8);
                const float y = le_f32(d.data() + off + static_cast<size_t>(j) * 8 + 4);
                c.points.push_back({x, y});
            }
            out.push_back(std::move(c));
        }
        off += static_cast<size_t>(icnt) * isz;
    }
    return out;
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
        case kTypeProp:
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
        case kTypeProp: {
            // One `prop` resource holds every tuning curve (17 in the shipped package).
            std::vector<PropertyCurve> curves = parse_prop_curves(d);
            for (PropertyCurve& c : curves) out.properties.push_back(std::move(c));
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
