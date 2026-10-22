#include "engine/cell_world.h"

#include <cctype>
#include <cmath>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

#include "engine/cell_save.h"
#include "engine/fs.h"
#include "engine/naming.h"

namespace os::cell {
namespace {
struct Rng {
    u32 s;
    explicit Rng(u32 seed) : s(seed ? seed : 0x9E3779B9u) {}
    u32 next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return s;
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFF) / 16777216.0f; }  // [0,1)
    float signed_unit() { return unit() * 2.0f - 1.0f; }                          // [-1,1)
};

constexpr int kMaxEntities = 1000;
constexpr float kPlayerAccel = 70.0f;
constexpr float kPlayerSpeed = 24.0f;
constexpr float kDrag = 4.0f;
constexpr float kPredatorAccel = 34.0f;  // SEED: not recovered
constexpr float kPredatorSpeed = 10.0f;   // SEED: fallback when the archetype has no speed
constexpr float kPreyAccel = 20.0f;       // SEED: not recovered
constexpr float kPreySpeed = 5.0f;        // SEED: fallback when the archetype has no speed
constexpr float kAggro = 24.0f;           // SEED: not recovered
constexpr float kGrowRate = 0.55f;        // SEED: not recovered
constexpr float kPredatorFraction = 1.05f;  // SEED: not recovered
constexpr float kMaxSize = 16.0f;
constexpr float kPlayerBaseSize = 1.0f;
constexpr float kEvolveSize = 3.0f;  // documented evolve threshold (analysis/game-stages.md)
constexpr float kRespawnDelay = 1.5f;  // SEED: not in cell_properties
constexpr float kMeatTtl = 14.0f;
constexpr float kRefillInterval = 3.0f;
constexpr int kMeatDna = 2, kPlantDna = 1, kPreyDna = 3;  // SEED: not in cell_properties
constexpr float kActionAnimDuration = 0.5f;

bool contains_ci(const std::string& hay, const char* needle) {
    if (hay.empty()) return false;
    std::string h;
    h.reserve(hay.size());
    for (char c : hay) h.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return h.find(needle) != std::string::npos;
}
std::string archetype_label(const GameData& d, const Archetype& a) {
    (void)d;
    if (const char* n = naming::instance_name(a.id)) return n;
    if (!a.name.empty()) return a.name;
    return a.display;
}

bool looks_like_food(const std::string& name) {
    return contains_ci(name, "food") || contains_ci(name, "flora") || contains_ci(name, "plant") ||
           contains_ci(name, "clump") || contains_ci(name, "egg");
}
int diet_from_name(const std::string& name) {
    if (contains_ci(name, "carnivore")) return 1;
    if (contains_ci(name, "herbivore")) return 0;
    if (contains_ci(name, "omnivore")) return 2;
    return -1;  // unknown
}
bool is_unnamed_food_prop(u32 archetype_id) {
    static const std::vector<std::pair<u32, u32>> kFoodPropRanges = {
        {0x5a5c0f00u, 0x5a5c0f03u},  // level_water food props
        {0x63166098u, 0x6316609eu},  // level_plant food props
        {0x0e09bb72u, 0x0e09bb75u},  // level_ice food props
        {0x0feb4d91u, 0x0feb4d96u},  // level_random/beach food props
    };
    for (const auto& [lo, hi] : kFoodPropRanges) {
        if (archetype_id >= lo && archetype_id <= hi) return true;
    }
    return false;
}

float hash01(u32 h) {
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return static_cast<float>(h & 0xFFFFFF) / 16777216.0f;
}

// A cell's body radius. SEED: the archetype's real size field is `size` @ record +0x304, which
// is OUTSIDE the shipped 400-byte record, so no size ships with the data. The cAIData
// `growAmount` is a GROWTH amount, not a size, so it must not be used here.
float archetype_size(const Archetype& a) {
    return 0.55f + 1.35f * hash01(a.id);
}

// A cell's max HP. The archetype's `hp` (record +0xa8) is a small tier value (hero 3, squid 2,
// Ducky 2); the shipped HP formula was not recovered, so this is a documented seed.
float archetype_hp(const Archetype& a) {
    const float tier = a.scalars.hp > 0 ? static_cast<float>(a.scalars.hp) : 1.0f;
    return tier * 20.0f;
}

// A loot table with `mustHavePart` set only drops when the player has a part equipped.
bool loot_allowed(const LootTable& t, const std::vector<std::string>& parts) {
    return t.must_have_part == 0 || !parts.empty();
}

const Archetype* find_archetype(const GameData& d, const char* name) {
    const u32 id = fnv1_lower(name);
    for (const Archetype& a : d.archetypes) {
        if (a.id == id || a.name == name) return &a;
    }
    return nullptr;
}

}  // namespace

int World::structure_of(const Archetype& a) const {
    for (size_t i = 0; i < m_data.structures.size(); ++i) {
        if (m_data.structures[i].id == a.template_id) return static_cast<int>(i);
    }
    return -1;
}

bool World::load_path(const std::string& path, std::string& error) {
    clear();
    if (path.empty()) {
        error = "empty package path";
        return false;
    }
    if (!decode(path, m_data, error)) return false;
    m_loaded = true;
    m_source = path;
    return true;
}

bool World::load_memory(const u8* data, size_t size, const std::string& label, std::string& error) {
    clear();
    if (!decode_from_memory(data, size, label, m_data, error)) return false;
    m_loaded = true;
    m_source = label;
    return true;
}

bool World::load_save(const std::string& path, std::string& error) {
    if (path.empty()) {
        error = "empty save path";
        return false;
    }
    std::vector<cell::CreatureStats> creatures;
    cell::StreamInfo info;
    if (!cell::decode_save(path, creatures, info, error)) {
        return false;
    }
    if (!creatures.empty()) {
        // The shipped save carries NO player marker: all 250 groups are identical apart from
        // their object sequence id (0x03CDD54B, counting 79, 80, 81 …). Our own writer emits
        // exactly one group - the player - so a single group is unambiguous. With several
        // groups the engine takes the one with the LOWEST sequence id, i.e. the first creature
        // created, which is the player at level start. That choice is a host policy, not a
        // recovered rule.
        size_t player_group = 0;
        for (size_t i = 1; i < creatures.size(); ++i) {
            if (creatures[i].stat4 < creatures[player_group].stat4) player_group = i;
        }
        const cell::CreatureStats& c = creatures[player_group];
        // The position block holds packed int16 pairs: 0x024162C0 is (x, y) and 0x024162C1 is
        // (vx, vy). The wire scale is a SEED (1/100 of a world unit) - the shipped values span
        // the whole int16 range and the game's world size is not recovered.
        const auto unpack2 = [](u32 v, float& a, float& b) {
            const i32 ia = static_cast<i32>(static_cast<std::int16_t>((v >> 16) & 0xFFFF));
            const i32 ib = static_cast<i32>(static_cast<std::int16_t>(v & 0xFFFF));
            a = static_cast<float>(ia) / 100.0f;
            b = static_cast<float>(ib) / 100.0f;
        };
        float px = 0.0f, py = 0.0f, vx = 0.0f, vy = 0.0f;
        unpack2(c.pos_y, px, py);
        unpack2(c.pos_z, vx, vy);
        if (m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()) {
            m_entities[static_cast<size_t>(m_player)].x = px;
            m_entities[static_cast<size_t>(m_player)].y = py;
            m_entities[static_cast<size_t>(m_player)].vx = vx;
            m_entities[static_cast<size_t>(m_player)].vy = vy;
        }
        if (c.stat9.has_value()) {
            const float sz = c.stat9.value();
            if (sz > 0.0f && m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()) {
                m_entities[static_cast<size_t>(m_player)].size = sz;
            }
        }
        m_dna = static_cast<int>(c.dna);
        m_eaten = static_cast<int>(c.eaten);
    }
    return true;
}

// The player's invulnerability window after a hit: the archetype's cAIData `protectTime` when it
// is set, else a documented seed. There is NO `protectTime` curve in the shipped prop table (its
// 17 keys are the tuning names), so the old curve lookup could only ever return its default.
float World::player_protect_time() const {
    if (m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()) {
        const u32 id = m_entities[static_cast<size_t>(m_player)].archetype;
        for (const Archetype& a : m_data.archetypes) {
            if (a.id == id && a.ai.protect_time > 0.0f) return a.ai.protect_time;
        }
    }
    return 0.5f;  // SEED: not recovered
}

std::vector<const Entity*> World::electric_targets(const Entity& source, float range) const {
    std::vector<const Entity*> targets;
    const float jumps = curve_value("electric_jumps_count", 0.0f, 1.0f);
    const float max_targets = jumps > 0.0f ? jumps : 1.0f;
    for (const Entity& e : m_entities) {
        if (!e.alive || e.role != Role::Creature) continue;
        if (&e == &source) continue;
        const float dx = e.x - source.x, dy = e.y - source.y;
        if (dx * dx + dy * dy <= range * range) {
            targets.push_back(&e);
            if (static_cast<int>(targets.size()) >= static_cast<int>(max_targets)) break;
        }
    }
    return targets;
}

std::vector<const Entity*> World::poison_targets(const Entity& source, float range) const {
    std::vector<const Entity*> targets;
    for (const Entity& e : m_entities) {
        if (!e.alive || e.role != Role::Creature) continue;
        if (&e == &source) continue;
        const float dx = e.x - source.x, dy = e.y - source.y;
        if (dx * dx + dy * dy <= range * range) {
            targets.push_back(&e);
        }
    }
    return targets;
}

void World::spawn_particles(float x, float y, int count, u32 color) {
    for (int i = 0; i < count && static_cast<int>(m_particles.size()) < 200; ++i) {
        Particle p;
        p.x = x;
        p.y = y;
        const float angle = static_cast<float>(m_ticks + i) * 2.399963f;
        const float speed = 2.0f + static_cast<float>(i % 5);
        p.vx = std::cos(angle) * speed;
        p.vy = std::sin(angle) * speed;
        p.life = 1.0f;
        p.color = color;
        m_particles.push_back(p);
    }
}
void World::play_sound(const char* name) {
    if (!name || !*name) return;
    m_last_sound = name;
    if (strcmp(name, "eat") == 0) {
        Beep(800, 50);
        Beep(1000, 30);
    } else if (strcmp(name, "death") == 0) {
        Beep(200, 200);
        Beep(150, 300);
    } else if (strcmp(name, "evolve") == 0) {
        Beep(1200, 100);
        Beep(1400, 100);
        Beep(1600, 100);
    } else if (strcmp(name, "unlock") == 0) {
        Beep(1000, 75);
        Beep(1200, 50);
    } else if (strcmp(name, "hatch") == 0) {
        Beep(600, 50);
        Beep(800, 50);
    }
}

bool World::select_part(const std::string& part_name) {
    const std::string unlock_name = "cell_unlock_" + part_name;
    const u32 unlock_id = fnv1_lower(unlock_name.c_str());
    bool unlocked = false;
    for (const Archetype& a : m_data.archetypes) {
        if (a.id == unlock_id) { unlocked = true; break; }
    }
    if (unlocked) {
        m_selected_part = part_name;
        return true;
    }
    return false;
}

std::vector<World::PartInfo> World::parts_list() const {
    // The part list IS the shipped data: each `cell_unlock_<part>` archetype in CellGame.package
    // (reg_file.txt confirms the names) is a part the cell editor offers. The `_pickup` variants are
    // the in-world pickup props, not separate parts. DNA cost is NOT in the shipped record, so it is
    // a documented seed (see cell_world.h).
    std::vector<PartInfo> parts;
    for (const Archetype& a : m_data.archetypes) {
        if (a.name.rfind("cell_unlock_", 0) != 0) continue;
        if (a.name.find("_pickup") != std::string::npos) continue;
        PartInfo info;
        info.name = a.name.substr(std::strlen("cell_unlock_"));
        info.display = info.name;
        info.cost = 10;  // SEED: DNA cost not recovered from the shipped data
        info.unlocked = true;
        info.cell_type = a.scalars.cell_type;
        parts.push_back(info);
    }
    return parts;
}

bool World::toggle_part(const std::string& part_name) {
    const std::string unlock_name = "cell_unlock_" + part_name;
    const u32 unlock_id = fnv1_lower(unlock_name.c_str());
    bool unlocked = false;
    for (const Archetype& a : m_data.archetypes) {
        if (a.id == unlock_id) { unlocked = true; break; }
    }
    if (!unlocked) return false;
    for (auto it = m_selected_parts.begin(); it != m_selected_parts.end(); ++it) {
        if (*it == part_name) {
            m_selected_parts.erase(it);
            return true;
        }
    }
    m_selected_parts.push_back(part_name);
    return true;
}

void World::clear_selected_parts() {
    m_selected_parts.clear();
}

bool World::write_save(const std::string& path, std::string& error) const {
    if (path.empty()) {
        error = "empty save path";
        return false;
    }
    // The cell save stream is a sequence of field frames, each
    // [tag 0x01A80D26 BE][length BE][count BE][fid BE][size BE]*[payloads], grouped into one
    // creature by a 24-byte object trailer (see engine/cell_save.cpp). The shipped creature uses
    // the 8-frame field-count pattern [5][4][25][11][4][2][35][8].
    //
    // The wire ids and payload sizes below are the SHIPPED ones, taken from the shipped save
    // audit (analysis/audit/cell-save-full.json, stream 0x018eb4b7): that stream's 150 creatures
    // all use the identical 8-frame id/size pattern, so this is the shipped layout and not an
    // invented one.
    static const u32 kFrame0Ids[] = {0x0240E421u, 0x0240E422u, 0x0240E423u, 0x02E9EFD3u, 0x03CDD54Bu};
    static const u32 kFrame0Sizes[] = {4, 1, 4, 4, 4};
    static const u32 kFrame1Ids[] = {0x024162BFu, 0x024162C0u, 0x024162C1u, 0x024162C2u};
    static const u32 kFrame1Sizes[] = {4, 4, 4, 4};
    static const u32 kFrame2Ids[] = {
        0x02416C5Eu, 0x02416C60u, 0x02416C61u, 0x02416C62u, 0x02416C63u, 0x02416C64u, 0x02416C65u,
        0x02416C66u, 0x02416C67u, 0x02416C68u, 0x02416C69u, 0x02416C6Au, 0x02416C6Bu, 0x02416C6Cu,
        0x02416C6Du, 0x02416C6Eu, 0x0316F520u, 0x0445DC86u, 0x047DF70Du, 0x047DF723u, 0x049100ECu,
        0x05E8FAA8u, 0x0771D7B4u, 0xB1A64F7Bu, 0xD24B464Bu};
    static const u32 kFrame2Sizes[] = {12, 16, 1, 1, 1, 4, 1, 1, 1, 1, 1, 1, 4, 1, 4,
                                       4, 4, 1, 12, 24, 4, 24, 1, 1, 1};
    static const u32 kFrame3Ids[] = {0x02415689u, 0x0241568Au, 0x0241568Cu, 0x0241568Fu, 0x02415690u,
                                    0x02415692u, 0x02415693u, 0x037D3DF5u, 0x039A844Fu, 0x049489A3u,
                                    0x04C919ABu};
    static const u32 kFrame3Sizes[] = {1, 4, 4, 4, 4, 12, 4, 4, 4, 12, 4};
    static const u32 kFrame4Ids[] = {0x02415EB8u, 0x02415EB9u, 0x02415EBAu, 0x02415EBBu};
    static const u32 kFrame4Sizes[] = {4, 4, 4, 4};
    static const u32 kFrame5Ids[] = {0x043C4AB2u, 0x04BFED68u};
    static const u32 kFrame5Sizes[] = {4, 4};
    static const u32 kFrame6Ids[] = {
        0x021D1BA5u, 0x021D1BAAu, 0x021D1BABu, 0x021D1BACu, 0x021D239Fu, 0x02415EBCu, 0x02415EBDu,
        0x02415EBEu, 0x02415EC6u, 0x02415EC8u, 0x02415ECAu, 0x024CD902u, 0x0303FB98u, 0x03D97B19u,
        0x03F64A78u, 0x042A2B0Eu, 0x04653B60u, 0x051B38FFu, 0x0521E608u, 0x0528933Eu, 0x0535A140u,
        0x0547C1BFu, 0x05501F64u, 0x05501F65u, 0x05675535u, 0x05EF360Eu, 0x06948B64u, 0x076C8F46u,
        0x076C8F4Du, 0x33ACE6B5u, 0x5242896Au, 0x52773F6Fu, 0x72774074u, 0xB2774021u, 0xD277405Au};
    static const u32 kFrame6Sizes[] = {4, 4, 4, 4, 4, 1, 1, 1, 1, 4, 4, 1, 1, 4, 12, 1, 37, 4, 4, 8,
                                       8, 4, 4, 4, 4, 4, 1, 92, 92, 4, 4, 4, 1, 4, 4};
    static const u32 kFrame7Ids[] = {0x02E6E1ABu, 0x02E6E1ADu, 0x02E6E1AFu, 0x02E6E1B0u, 0x02E6E1B1u,
                                     0x02E6E1B2u, 0x05F0D6F3u, 0x06A814A9u};
    static const u32 kFrame7Sizes[] = {1, 4, 4, 4, 4, 4, 4, 4};
    auto put32be = [](std::vector<u8>& v, u32 x) {
        v.push_back((x >> 24) & 0xFF); v.push_back((x >> 16) & 0xFF);
        v.push_back((x >> 8) & 0xFF); v.push_back(x & 0xFF);
    };
    auto putf32be = [&](std::vector<u8>& v, float f) { u32 b; memcpy(&b, &f, 4); put32be(v, b); };
    auto zeros = [](std::vector<u8>& v, size_t n) { v.insert(v.end(), n, u8(0)); };
    auto be4 = [&](u32 x) { std::vector<u8> v; put32be(v, x); return v; };
    auto be1 = [&](u8 x) { return std::vector<u8>{x}; };
    auto beN = [&](u32 n) { return std::vector<u8>(n, u8(0)); };
    auto beFloat = [&](float f) { std::vector<u8> v; putf32be(v, f); return v; };
    // Emit one frame: the header, the id/size pairs, then the payloads (each already the length
    // its shipped size demands).
    auto emit = [&](std::vector<u8>& v, const u32* ids, const u32* sizes, u32 n,
                    const std::vector<std::vector<u8>>& vals) {
        u32 sum = 0;
        for (u32 i = 0; i < n; ++i) sum += sizes[i];
        put32be(v, 0x01A80D26u);
        put32be(v, 4 + 8 * n + sum);
        put32be(v, n);
        for (u32 i = 0; i < n; ++i) { put32be(v, ids[i]); put32be(v, sizes[i]); }
        for (u32 i = 0; i < n; ++i) v.insert(v.end(), vals[i].begin(), vals[i].end());
    };

    const Entity* p = player();
    std::vector<u8> blob;
    // Stream header: [count][crc][token][12-byte header]. The count word holds the byte size of
    // ONE creature group - the stream header, the frames and the 24-byte trailer - which is
    // how the shipped streams read: the shipped 150-group stream is 225450 B = 150 x 1503.
    // This writer emits a single group, so the count is the blob's size.
    put32be(blob, 0);               // group byte size (patched below)
    put32be(blob, 0);               // crc
    put32be(blob, 0);               // token
    zeros(blob, 12);
    // Frame 0: 5 stat fields (the shipped values).
    emit(blob, kFrame0Ids, kFrame0Sizes, 5,
         {be4(0), be1(0), be4(0xFFFFFFFFu), be4(0), be4(79)});
    // Frame 1: the position block. The shipped save stores the coordinates as packed int16
    // pairs in the SECOND field (0x024162C0) and leaves the first and the last at 0, so
    // the writer does the same: 0, (x, y), (vx, vy), 0. The wire scale is a SEED - the
    // shipped values span the whole int16 range and the game's world size is not recovered -
    // so 1/100 of a world unit is used, which fits the engine's arena in an int16.
    const auto pack2 = [](float a, float b) {
        const i32 ia = static_cast<i32>(a * 100.0f);
        const i32 ib = static_cast<i32>(b * 100.0f);
        const u32 ua = static_cast<u32>(ia & 0xFFFF);
        const u32 ub = static_cast<u32>(ib & 0xFFFF);
        return (ua << 16) | ub;
    };
    const u32 packed_pos = p ? pack2(p->x, p->y) : 0u;
    const u32 packed_vel = p ? pack2(p->vx, p->vy) : 0u;
    emit(blob, kFrame1Ids, kFrame1Sizes, 4,
         {be4(0u), be4(packed_pos), be4(packed_vel), be4(0u)});
    // Frame 2: 25 detailed fields. 0x02416C60 is the position triple plus a trailing 1.0, and
    // 0x02416C64 is the creature's scale.
    {
        std::vector<std::vector<u8>> f;
        for (u32 i = 0; i < 25; ++i) f.push_back(beN(kFrame2Sizes[i]));
        std::vector<u8> pos;
        putf32be(pos, p ? p->x : 0.0f);
        putf32be(pos, p ? p->y : 0.0f);
        putf32be(pos, p ? p->z : 0.0f);
        putf32be(pos, 1.0f);
        f[1] = pos;
        f[5] = beFloat(p ? p->size : 1.0f);
        emit(blob, kFrame2Ids, kFrame2Sizes, 25, f);
    }
    // Frame 3: 11 fields.
    {
        std::vector<std::vector<u8>> f;
        for (u32 i = 0; i < 11; ++i) f.push_back(beN(kFrame3Sizes[i]));
        emit(blob, kFrame3Ids, kFrame3Sizes, 11, f);
    }
    // Frame 4: 4 fields.
    {
        std::vector<std::vector<u8>> f;
        for (u32 i = 0; i < 4; ++i) f.push_back(beN(kFrame4Sizes[i]));
        emit(blob, kFrame4Ids, kFrame4Sizes, 4, f);
    }
    // Frame 5: the 2 progress fields (dna, eaten) - the shipped 2-field frame.
    emit(blob, kFrame5Ids, kFrame5Sizes, 2,
         {be4(static_cast<u32>(m_dna)), be4(static_cast<u32>(m_eaten))});
    // Frame 6: 35 mixed fields.
    {
        std::vector<std::vector<u8>> f;
        for (u32 i = 0; i < 35; ++i) f.push_back(beN(kFrame6Sizes[i]));
        emit(blob, kFrame6Ids, kFrame6Sizes, 35, f);
    }
    // Frame 7: 8 fields.
    {
        std::vector<std::vector<u8>> f;
        for (u32 i = 0; i < 8; ++i) f.push_back(beN(kFrame7Sizes[i]));
        emit(blob, kFrame7Ids, kFrame7Sizes, 8, f);
    }
    // 24-byte object trailer closes the group.
    zeros(blob, 24);
    const size_t group_bytes = blob.size();
    for (int i = 0; i < 4; ++i) blob[i] = static_cast<u8>((group_bytes >> (24 - 8 * i)) & 0xFF);

    // DBPF (little-endian): header, then the payload at 0x60, then a kind-4 index (prefix 8 +
    // one 28-byte record: type, group, instance, offset, size, usize, sentinel).
    std::vector<u8> out;
    auto put32le = [&](u32 v) {
        out.push_back(v & 0xFF); out.push_back((v >> 8) & 0xFF);
        out.push_back((v >> 16) & 0xFF); out.push_back((v >> 24) & 0xFF);
    };
    out.insert(out.end(), {'D', 'B', 'P', 'F'});
    put32le(3);                                   // major version
    put32le(0);                                   // minor version
    out.resize(0x24, 0); put32le(1);              // count = 1
    out.resize(0x2C, 0); put32le(36);             // index size = 8 + 28
    out.resize(0x3C, 0); put32le(3);              // field_3c
    out.resize(0x40, 0);
    const u32 payload_off = 0x60;
    const u32 index_offset = payload_off + static_cast<u32>(blob.size());
    put32le(index_offset);
    out.resize(payload_off, 0);
    out.insert(out.end(), blob.begin(), blob.end());
    const size_t ix = index_offset;
    out.resize(ix + 36, 0);
    auto put32at = [&](size_t at, u32 v) { memcpy(out.data() + at, &v, 4); };
    put32at(ix + 0, 4);                     // kind 4
    put32at(ix + 4, 0);                     // shared unknown
    put32at(ix + 8, 0x018EB4B7u);           // type
    put32at(ix + 12, 0);                    // group
    put32at(ix + 16, 0);                    // instance
    put32at(ix + 20, payload_off);          // offset
    put32at(ix + 24, static_cast<u32>(blob.size()));  // size (raw)
    put32at(ix + 28, static_cast<u32>(blob.size()));  // usize
    put32at(ix + 32, 0x00010000u);          // sentinel/flags
    FILE* f = nullptr;
    errno_t err = fopen_s(&f, path.c_str(), "wb");
    if (err != 0 || !f) {
        error = "cannot open save file for writing";
        return false;
    }
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
    return true;
}

bool World::load_default(std::string& error) {
    const auto path = fs::resolve("Data/CellGame.package");
    if (path.empty()) {
        error = "Data/CellGame.package not found on the search paths";
        return false;
    }
    if (!load_path(path.string(), error)) return false;
    const auto layers = fs::resolve("Data/CellGame_Layers.package");
    if (!layers.empty()) {
        std::string lerr;
        if (!decode(layers.string(), m_data, lerr)) {
            error = lerr;  // non-fatal: report why the backdrop is absent, keep the world
        }
    }
    return true;
}

const LayerField* World::layer_named(const std::string& name) const {
    for (const LayerField& f : m_data.layers) {
        if (f.name == name) return &f;
    }
    return nullptr;
}

const LayerField* World::first_layer(u32 components) const {
    for (const LayerField& f : m_data.layers) {
        if (f.components == components) return &f;
    }
    return nullptr;
}

const LookTable* World::look_table(const char* name) const {
    if (!name || !*name) return nullptr;
    const u32 id = fnv1_lower(name);
    for (const LookTable& t : m_data.look_tables) {
        if (t.id == id) return &t;
    }
    return nullptr;
}

float World::look_weight(const char* table_name, u32 entry_type) const {
    const LookTable* t = look_table(table_name);
    if (!t) return 0.0f;
    for (const LookEntry& e : t->entries) {
        if (e.type == entry_type) return e.weight;
    }
    return 0.0f;
}

const PropertyCurve* World::property_curve(const char* name) const {
    if (!name || !*name) return nullptr;
    for (const PropertyCurve& c : m_data.properties) {
        if (c.key == name) return &c;
    }
    return nullptr;
}

std::vector<std::string> World::biome_names() const {
    std::vector<std::string> names;
    for (const WorldResource& w : m_data.worlds) {
        if (!w.name.empty()) names.push_back(w.name);
    }
    return names;
}

std::vector<std::string> World::level_names() const {
    std::vector<std::string> names;
    for (const Population& p : m_data.populations) {
        if (!p.name.empty()) names.push_back(p.name);
    }
    return names;
}

const std::vector<StructurePart>* World::parts_for(u32 archetype_id) const {
    for (const Archetype& a : m_data.archetypes) {
        if (a.id == archetype_id) {
            int idx = structure_of(a);
            if (idx >= 0 && static_cast<size_t>(idx) < m_data.structures.size()) {
                return &m_data.structures[static_cast<size_t>(idx)].parts;
            }
            return nullptr;
        }
    }
    return nullptr;
}

std::vector<u32> World::level_refs() const {
    std::vector<u32> out;
    for (const Globals& g : m_data.globals) {
        for (u32 wref : g.refs) {
            const WorldResource* w = nullptr;
            for (const WorldResource& cand : m_data.worlds) {
                if (cand.id == wref) { w = &cand; break; }
            }
            if (!w || w->levels.empty()) continue;
            // A world is playable when its first level slot has flag 1; the `_bg` worlds have
            // flag 0 on every slot and only host scenery.
            if (w->levels.front().flag != 1) continue;
            for (const LevelSlot& s : w->levels) {
                if (std::find(out.begin(), out.end(), s.ref) == out.end()) out.push_back(s.ref);
            }
        }
    }
    return out;
}

std::vector<std::string> World::level_list() const {
    std::vector<std::string> out;
    for (u32 ref : level_refs()) {
        for (const Population& p : m_data.populations) {
            if (p.id != ref || p.name.empty()) continue;
            out.push_back(p.name);
            break;
        }
    }
    return out;
}

std::string World::next_level(const std::string& current) const {
    const std::vector<u32> refs = level_refs();
    if (refs.empty()) return "";
    const u32 want = fnv1_lower(current.c_str());
    for (size_t i = 0; i < refs.size(); ++i) {
        if (refs[i] != want) continue;
        for (size_t k = i + 1; k < refs.size(); ++k) {
            for (const Population& p : m_data.populations) {
                if (p.id != refs[k] || p.name.empty()) continue;
                return p.name;
            }
        }
        return "";
    }
    return "";
}

const LootTable* World::biome_loot(const char* biome_name) const {
    if (!biome_name || !*biome_name) return nullptr;
    // A level name is "level_<biome>"; the shipped loot tables are "loot_<biome>"
    // (loot_water / loot_beach / loot_ice / loot_rock / loot_plant / loot_random).
    std::string name = biome_name;
    static const char* kLevelPrefix = "level_";
    if (name.rfind(kLevelPrefix, 0) == 0) name = name.substr(std::strlen(kLevelPrefix));
    std::string table_name = "loot_" + name;
    const u32 table_id = fnv1_lower(table_name.c_str());
    for (const LootTable& t : m_data.loot_tables) {
        if (t.id == table_id || t.name == table_name) return &t;
    }
    return nullptr;
}

float World::curve_value(const char* name, float x, float default_val) const {
    const PropertyCurve* c = property_curve(name);
    if (!c || c->points.empty()) return default_val;
    if (c->points.size() == 1) return c->points[0].second;
    if (x <= c->points.front().first) return c->points.front().second;
    if (x >= c->points.back().first) return c->points.back().second;
    for (size_t i = 1; i < c->points.size(); ++i) {
        if (x <= c->points[i].first) {
            float x0 = c->points[i - 1].first, y0 = c->points[i - 1].second;
            float x1 = c->points[i].first, y1 = c->points[i].second;
            if (x1 == x0) return y1;
            return y0 + (y1 - y0) * (x - x0) / (x1 - x0);
        }
    }
    return default_val;
}

void World::spawn_from_level(const Population& pop) {
    Rng rng(fnv1_lower(m_level.c_str()));
    for (const SpawnEntry& e : pop.entries) {
        if (e.cell == 0 || e.count <= 0.0f) continue;
        int n = static_cast<int>(e.count + 0.5f);
        if (n < 1) n = 1;
        const Archetype* arch = nullptr;
        for (const Archetype& a : m_data.archetypes) {
            if (a.id == e.cell) {
                arch = &a;
                break;
            }
        }
        const std::string name = arch ? archetype_label(m_data, *arch) : std::string();
        const bool food = looks_like_food(name) || is_unnamed_food_prop(e.cell);
        const int diet = diet_from_name(name);
        for (int k = 0; k < n && static_cast<int>(m_entities.size()) < kMaxEntities; ++k) {
            Entity en;
            en.archetype = e.cell;
            if (arch) en.structure = structure_of(*arch);
            en.role = food ? Role::Food : Role::Creature;
            en.diet = diet;
            if (contains_ci(name, "egg")) {
                // The egg's hatch time is the archetype's cAIData `hatchDuration` (a cAIData
                // field, not a curve - the shipped prop table has no `hatchDuration` key).
                en.hatch_time = arch && arch->ai.hatch_duration > 0.0f ? arch->ai.hatch_duration
                                                                      : 3.0f;  // SEED
            }
            // The archetype's real size field (`size` @ record +0x304) is outside the shipped
            // 400-byte record, so the body radius is a SEED (see archetype_size).
            en.size = archetype_size(arch ? *arch : Archetype{});
            if (arch) {
                en.hp = en.max_hp = archetype_hp(*arch);
                en.move_speed = arch->ai.speed;
                en.chase_speed = arch->ai.chase_speed;
                en.flee_speed = arch->ai.flee_speed;
                en.wander_speed = arch->ai.wander_speed;
                en.turn_factor = arch->ai.turn_factor;
                en.flocking = arch->ai.flocking;
                en.aware_food = arch->ai.awareness_radius_food;
                en.aware_predator = arch->ai.awareness_radius_predator;
            }
            en.x = rng.signed_unit() * m_arena;
            en.y = rng.signed_unit() * m_arena;
            en.vx = rng.signed_unit() * 2.0f;
            en.vy = rng.signed_unit() * 2.0f;
            m_entities.push_back(en);
            if (food) m_food_templates.push_back(en);  // remembered so the pool can be restocked
        }
    }
    m_target_food = 0;
    for (const Entity& e : m_entities) if (e.role == Role::Food) ++m_target_food;
}

bool World::set_level(const std::string& level, std::string& error) {
    if (!m_loaded) {
        error = "no CellGame data loaded";
        return false;
    }
    const Population* pop = nullptr;
    const u32 want = fnv1_lower(level.c_str());
    for (const Population& p : m_data.populations) {
        if (p.name == level || p.id == want) {
            pop = &p;
            break;
        }
    }
    if (!pop) {
        error = "unknown level '" + level + "'";
        return false;
    }
    m_entities.clear();
    m_food_templates.clear();
    m_target_food = 0;
    m_refill = 0.0f;
    m_player = -1;
    m_ticks = 0;
    m_time = 0.0f;
    m_level = level;
    m_player_alive = true;
    m_hp = m_max_hp = 100.0f;
    m_eaten = 0;
    m_dna = 0;
    m_diet = Diet::Herbivore;
    m_eaten_plant = m_eaten_meat = 0;
    m_evolved = false;
    m_respawn = 0.0f;
    m_has_target = false;
    m_cam_x = m_cam_y = 0.0f;

    spawn_from_level(*pop);
    std::string bg_level = level + "_bg";
    const u32 bg_id = fnv1_lower(bg_level.c_str());
    for (const Population& p : m_data.populations) {
        if (p.id == bg_id || p.name == bg_level) {
            Rng rng(fnv1_lower(bg_level.c_str()));
            for (const SpawnEntry& e : p.entries) {
                if (e.cell == 0 || e.count <= 0.0f) continue;
                int n = static_cast<int>(e.count + 0.5f);
                if (n < 1) n = 1;
                const Archetype* arch = nullptr;
                for (const Archetype& a : m_data.archetypes) {
                    if (a.id == e.cell) { arch = &a; break; }
                }
                const std::string name = arch ? archetype_label(m_data, *arch) : std::string();
                for (int k = 0; k < n && static_cast<int>(m_entities.size()) < kMaxEntities; ++k) {
                    Entity en;
                    en.archetype = e.cell;
                    if (arch) en.structure = structure_of(*arch);
                    // The `_bg` population is scenery: it drifts, never hunts and is not eaten.
                    en.role = Role::Prop;
                    en.diet = diet_from_name(name);
                    en.size = archetype_size(arch ? *arch : Archetype{});
                    en.x = rng.signed_unit() * m_arena;
                    en.y = rng.signed_unit() * m_arena;
                    en.vx = rng.signed_unit() * 2.0f;
                    en.vy = rng.signed_unit() * 2.0f;
                    m_entities.push_back(en);
                }
            }
            break;
        }
    }
    const Archetype* hero = find_archetype(m_data, "cell_hero_L0");
    if (!hero) hero = find_archetype(m_data, "cell_minno1");
    if (hero) {
        Entity en;
        en.archetype = hero->id;
        en.structure = structure_of(*hero);
        en.role = Role::Creature;
        en.size = kPlayerBaseSize;
        m_entities.push_back(en);
        m_player = static_cast<int>(m_entities.size()) - 1;
    } else if (!m_entities.empty()) {
        m_player = 0;
    }
    error.clear();
    return true;
}

void World::set_target(float wx, float wy) {
    m_has_target = true;
    m_target_x = wx;
    m_target_y = wy;
}

void World::clear_target() { m_has_target = false; }

void World::teleport_player(float wx, float wy) {
    if (m_player < 0 || static_cast<size_t>(m_player) >= m_entities.size()) return;
    Entity& p = m_entities[static_cast<size_t>(m_player)];
    p.x = wx;
    p.y = wy;
    p.vx = p.vy = 0.0f;
}

void World::reset_protection() {
    if (m_player < 0 || static_cast<size_t>(m_player) >= m_entities.size()) return;
    m_entities[static_cast<size_t>(m_player)].protect_time = 0.0f;
}

float World::hp_for_size(float size) const {
    // The archetype's `hp` (record +0xa8) is a small TIER (hero 3, squid 2, Ducky 2). The shipped
    // HP formula was not recovered, so the tier -> points mapping below is a documented seed.
    // Record +0x148 is NOT the HP: per the recovered descriptor that slot is the cAIData `food`
    // value (the hero's is 10000), which is why it must not be read as hit points.
    if (m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()) {
        const Entity& p = m_entities[static_cast<size_t>(m_player)];
        for (const Archetype& a : m_data.archetypes) {
            if (a.id != p.archetype) continue;
            if (a.scalars.hp > 0) return archetype_hp(a);
            break;
        }
    }
    return 50.0f + size * 25.0f;  // SEED fallback
}

void World::respawn_player() {
    Entity* p = m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()
                    ? &m_entities[static_cast<size_t>(m_player)]
                    : nullptr;
    if (p) {
        p->x = p->y = 0.0f;
        p->vx = p->vy = 0.0f;
        p->size = kPlayerBaseSize * 0.8f;
        p->alive = true;
    }
    m_max_hp = hp_for_size(p ? p->size : kPlayerBaseSize);
    m_hp = m_max_hp;
    m_player_alive = true;
    m_respawn = 0.0f;
    m_has_target = false;
}

void World::drop_meat(const Entity& from) {
    if (static_cast<int>(m_entities.size()) >= kMaxEntities) return;
    Entity m;
    m.archetype = from.archetype;  // keep the species so the chunk renders as the fallen cell
    m.structure = from.structure;
    m.role = Role::Meat;
    m.size = from.size * 0.6f;
    m.x = from.x;
    m.y = from.y;
    m.ttl = kMeatTtl;
    m_entities.push_back(m);
}

void World::drop_loot(const Entity& from) {
    const char* name = naming::instance_name(from.archetype);
    if (!name) return;
    std::string prefix = "loot_";
    std::string table_name = prefix + name;
    const u32 table_id = fnv1_lower(table_name.c_str());
    const LootTable* lt = nullptr;
    for (const LootTable& t : m_data.loot_tables) {
        if (t.id == table_id || t.name == table_name) { lt = &t; break; }
    }
    const LootTable* biome_lt = biome_loot(m_level.c_str());
    if (biome_lt && biome_lt != lt && loot_allowed(*biome_lt, m_selected_parts)) {
        for (const LootEntry& e : biome_lt->entries) {
            if (e.type == 1 && e.cell != 0 && e.weight > 0.0f) {
                if (static_cast<int>(m_entities.size()) >= kMaxEntities) return;
                Entity loot;
                loot.archetype = e.cell;
                loot.structure = -1;
                loot.role = Role::Food;
                loot.size = 0.3f;
                loot.x = from.x;
                loot.y = from.y;
                loot.vx = loot.vy = 0.0f;
                loot.ttl = 30.0f;
                m_entities.push_back(loot);
                break;
            }
        }
    }
    if (!lt || lt->entries.empty()) return;
    if (!loot_allowed(*lt, m_selected_parts)) return;
    float total_weight = 0.0f;
    for (const LootEntry& e : lt->entries) {
        if (e.weight > 0.0f) total_weight += e.weight;
    }
    if (total_weight <= 0.0f) return;
    float roll = static_cast<float>(m_ticks % 1000) / 1000.0f * total_weight;
    float accum = 0.0f;
    for (const LootEntry& e : lt->entries) {
        if (e.weight <= 0.0f) continue;
        accum += e.weight;
        if (roll <= accum) {
            int count = e.count > 0 ? e.count : 1;
            if (e.count_delta > 0) count += e.count_delta;
            if (e.level_offset > 0) count += e.level_offset;
            if (count > 3) count = 3;  // cap at 3 pickups per roll
            for (int i = 0; i < count; ++i) {
                if (e.type == 1 && e.cell != 0) {
                    if (static_cast<int>(m_entities.size()) >= kMaxEntities) return;
                    Entity loot;
                    loot.archetype = e.cell;
                    loot.structure = -1;
                    loot.role = Role::Food;  // unlock pickups are consumed like food
                    loot.size = 0.3f;
                    loot.x = from.x + static_cast<float>(i) * 0.5f;
                    loot.y = from.y + static_cast<float>(i) * 0.5f;
                    loot.vx = loot.vy = 0.0f;
                    loot.ttl = 30.0f;  // pickups despawn after 30 s
                    m_entities.push_back(loot);
                }
                if (e.type == 2 && e.table != 0) {
                    for (const LootTable& nested : m_data.loot_tables) {
                        if (nested.id != e.table) continue;
                        for (const LootEntry& ne : nested.entries) {
                            if (ne.type == 1 && ne.cell != 0 && ne.weight > 0.0f) {
                                if (static_cast<int>(m_entities.size()) >= kMaxEntities) return;
                                Entity loot;
                                loot.archetype = ne.cell;
                                loot.structure = -1;
                                loot.role = Role::Food;
                                loot.size = 0.3f;
                                loot.x = from.x + static_cast<float>(i) * 0.5f;
                                loot.y = from.y + static_cast<float>(i) * 0.5f;
                                loot.vx = loot.vy = 0.0f;
                                loot.ttl = 30.0f;
                                m_entities.push_back(loot);
                                break;
                            }
                        }
                        break;
                    }
                }
            }
            break;
        }
    }
}

void World::update_diet() {
    if (m_eaten_plant > 0 && m_eaten_meat > 0) {
        m_diet = Diet::Omnivore;
    } else if (m_eaten_meat > 0) {
        m_diet = Diet::Carnivore;
    } else {
        m_diet = Diet::Herbivore;
    }
}

void World::tick(float dt) {
    if (!m_loaded) return;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;  // a stalled frame must not tunnel the simulation
    const Entity* pl0 = player();
    if (pl0) {
        const float dilation = curve_value("time_dilation", pl0->size, 1.0f);
        if (dilation > 0.0f) dt *= dilation;
    }
    ++m_ticks;
    m_time += dt;

    Entity* pl = m_player >= 0 && static_cast<size_t>(m_player) < m_entities.size()
                     ? &m_entities[static_cast<size_t>(m_player)]
                     : nullptr;

    if (!pl) return;

    if (!m_player_alive) {
        m_respawn -= dt;
        if (m_respawn <= 0.0f) respawn_player();
    }

    const float px = pl->x, py = pl->y;
    const float psize = pl->size;
    for (size_t i = 0; i < m_entities.size(); ++i) {
        Entity& e = m_entities[i];
        if (!e.alive) continue;
        if (static_cast<int>(i) == m_player) {
            if (m_has_target && m_player_alive) {
                float dx = m_target_x - e.x, dy = m_target_y - e.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.001f) {
                    e.vx += (dx / len) * kPlayerAccel * dt;
                    e.vy += (dy / len) * kPlayerAccel * dt;
                }
            }
            const float sp = std::sqrt(e.vx * e.vx + e.vy * e.vy);
            if (sp > kPlayerSpeed) {
                e.vx *= kPlayerSpeed / sp;
                e.vy *= kPlayerSpeed / sp;
            }
            e.vx -= e.vx * kDrag * dt;
            e.vy -= e.vy * kDrag * dt;
            continue;
        }
        if (!m_player_alive) continue;  // food drifts; nothing hunts a dead player
        if (e.role != Role::Creature) continue;  // scenery and food never hunt
        const float dx = px - e.x, dy = py - e.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        // Awareness: the archetype's cAIData awareness radius (the food-specific one, else the
        // predator one), scaled by the shipped `awareness_dilation` curve (perception vs size).
        const float base_aware = e.aware_food > 0.0f ? e.aware_food
                              : (e.aware_predator > 0 ? static_cast<float>(e.aware_predator)
                                                      : kAggro);
        const float aware = base_aware * curve_value("awareness_dilation", e.size, 1.0f);
        if (dist < aware && dist > 0.001f) {
            // A creature is a predator when it is bigger than the player, prey when smaller (the
            // real mechanic: bigger eats smaller). This replaces the old speed-slot test.
            // The GA compares classes rather than raw sizes, but the class follows the cell's
            // size integer, and the engine's cells do not grow that integer yet (the GA grows
            // it from the digested food value, see analysis/cell-game.md), so a class
            // comparison here would make every NPC flee as soon as the player grew.
            const bool predator = e.size > psize * kPredatorFraction;
            const char* table = predator ? "npc_mad" : "npc_scared";
            const float w = look_weight(table, predator ? 4 : 5);   // pursue / flee target
            // The archetype's own speeds, used as world-unit speeds: the GA binary scales them by
            // `npcSpeedMultiplier` (1.0) and no per-mode base speed was recovered, so the shipped
            // value is the speed. The shipped range is 0.2 (a nearly stationary "booster") to 45 (a
            // darting small creature), which is plausible against the player's 24 seed.
            // `chaseSpeed` drives a predator, `speed` drives a normal move. The shipped
            // `fleeSpeed` is 0 in 188 of the 189 archetypes, so it cannot be the flee speed.
            // The steering acceleration is the integer `turnFactor`; the exact
            // original formula was not recovered, so this is a host policy over shipped values.
            const float speed = predator
                ? (e.chase_speed > 0.0f ? e.chase_speed
                                        : (e.move_speed > 0.0f ? e.move_speed : kPredatorSpeed))
                : (e.flee_speed > 0.0f ? e.flee_speed
                                       : (e.move_speed > 0.0f ? e.move_speed : kPreySpeed));
            const float turn = e.turn_factor > 0 ? static_cast<float>(e.turn_factor) : 1.0f;
            {
                const float dir = predator ? 1.0f : -1.0f;
                const float ww = (w > 0.0f ? w : 1.0f) * turn;
                e.vx += (dx / dist) * speed * dir * ww * dt;
                e.vy += (dy / dist) * speed * dir * ww * dt;
                const float sp = std::sqrt(e.vx * e.vx + e.vy * e.vy);
                if (sp > speed) { e.vx *= speed / sp; e.vy *= speed / sp; }
            }
            // Cohesion with same-archetype neighbours, weighted by the archetype's `flocking`.
            const float flock_w = e.flocking;
            if (flock_w > 0.0f) {
                float avg_x = 0.0f, avg_y = 0.0f;
                int count = 0;
                for (const Entity& other : m_entities) {
                    if (!other.alive || other.role != Role::Creature) continue;
                    if (other.archetype == e.archetype) {
                        const float dx2 = other.x - e.x, dy2 = other.y - e.y;
                        if (dx2 * dx2 + dy2 * dy2 < aware * aware) {
                            avg_x += other.x;
                            avg_y += other.y;
                            ++count;
                        }
                    }
                }
                if (count > 0) {
                    avg_x /= count;
                    avg_y /= count;
                    const float fdx = avg_x - e.x, fdy = avg_y - e.y;
                    const float flen = std::sqrt(fdx * fdx + fdy * fdy);
                    if (flen > 0.001f) {
                        e.vx += (fdx / flen) * kPreyAccel * flock_w * dt;
                        e.vy += (fdy / flen) * kPreyAccel * flock_w * dt;
                    }
                }
            }
        }
    }
    const float span = m_arena * 2.0f;
    for (Entity& e : m_entities) {
        if (!e.alive) continue;
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        if (e.x < -m_arena) e.x += span;
        if (e.x > m_arena) e.x -= span;
        if (e.y < -m_arena) e.y += span;
        if (e.y > m_arena) e.y -= span;
    }
    std::vector<Entity> drops;  // corpses become chunks AFTER the loop (else they're re-eaten now)
    if (m_player_alive && pl->alive) {
        for (size_t i = 0; i < m_entities.size(); ++i) {
            if (static_cast<int>(i) == m_player) continue;
            Entity& e = m_entities[i];
            if (!e.alive) continue;
            const float dx = e.x - pl->x, dy = e.y - pl->y;
            const float reach = pl->size + e.size;
            if (dx * dx + dy * dy > reach * reach) continue;
            if (e.role != Role::Prop &&
                (e.role == Role::Food || e.role == Role::Meat ||
                 e.size <= pl->size * kPredatorFraction)) {
                const float area = pl->size * pl->size + e.size * e.size * kGrowRate;
                pl->size = std::sqrt(area) > kMaxSize ? kMaxSize : std::sqrt(area);
                if (e.role == Role::Food) {
                    ++m_eaten_plant;
                    m_dna += kPlantDna;
                    m_evo_points += 20;
                    const char* eaten_name = naming::instance_name(e.archetype);
                    if (eaten_name && strncmp(eaten_name, "cell_unlock_", 12) == 0) {
                        pl->unlocked = true;
                    }
                } else {
                    ++m_eaten_meat;
                    m_dna += (e.role == Role::Meat) ? kMeatDna : kPreyDna;
                    m_evo_points += 40;  // seed reinterpretation of per-level curve
                    if (e.role == Role::Creature) {
                        drops.push_back(e);  // the corpse becomes a chunk
                        drop_loot(e);        // the corpse may drop a part unlock
                    }
                }
                e.alive = false;
                ++m_eaten;
                pl->anim = kAnimEat;
                pl->anim_time = 0.0f;
                play_sound("eat");
            } else if (e.role != Role::Prop) {
                // Scenery (the level's `_bg` population) is neither eaten nor harmful: it drifts
                // and must never damage, knock back or kill the player.
                if (pl->protect_time <= 0.0f) {
                    m_hp -= e.size * 18.0f * dt;
                    pl->protect_time = player_protect_time();
                }
                // The player bites back: a bigger creature still takes damage from the overlap.
                if (e.role == Role::Creature && e.hp > 0.0f) {
                    e.hp -= pl->size * 2.0f * dt;  // SEED: bite damage not recovered
                    if (e.hp <= 0.0f) {
                        e.hp = 0.0f;
                        e.alive = false;
                        drops.push_back(e);
                        drop_loot(e);
                        ++m_eaten;
                        ++m_eaten_meat;
                        m_dna += kPreyDna;
                        pl->anim = kAnimEat;
                        pl->anim_time = 0.0f;
                    }
                }
                std::string attacker_label;
                for (const Archetype& a : m_data.archetypes) {
                    if (a.id == e.archetype) {
                        attacker_label = archetype_label(m_data, a);
                        break;
                    }
                }
                const bool has_poison = contains_ci(attacker_label, "poison");
                if (has_poison) {
                    pl->poison_time = 3.0f;
                    if (pl->anim != kAnimGetHitSpike) {
                        pl->anim = kAnimGetHitPoison;
                        pl->anim_time = 0.0f;
                    }
                    const float poison_range = curve_value("poison_count", 0.0f, 3.0f);
                    std::vector<const Entity*> poisoned = poison_targets(e, poison_range);
                    for (const Entity* target : poisoned) {
                        if (target->alive) {
                            const_cast<Entity*>(target)->poison_time = 2.0f;
                        }
                    }
                }
                const bool has_electric = contains_ci(attacker_label, "elec");
                if (has_electric) {
                    pl->electric_time = 1.0f;
                    pl->anim = kAnimGetHitElec;
                    pl->anim_time = 0.0f;
                    const float range = curve_value("electric_range_count", 0.0f, 5.0f);
                    std::vector<const Entity*> chained = electric_targets(e, range);
                    for (const Entity* target : chained) {
                        if (target->alive) {
                            const_cast<Entity*>(target)->electric_time = 1.0f;
                        }
                    }
                }
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.001f) {
                    pl->vx -= (dx / len) * 40.0f * dt;
                    pl->vy -= (dy / len) * 40.0f * dt;
                }
                pl->anim = kAnimGetHitSpike;
                pl->anim_time = 0.0f;
                if (m_hp <= 0.0f) {
                    m_hp = 0.0f;
                    m_player_alive = false;
                    m_respawn = kRespawnDelay;
                    pl->alive = false;
                    pl->anim = kAnimDeath;
                    pl->anim_time = 0.0f;
                    play_sound("death");
                    spawn_particles(pl->x, pl->y, 15, 0xFF4040FFu);
                    if (pl->structure >= 0 && static_cast<size_t>(pl->structure) < m_data.structures.size()) {
                        const u32 death_id = m_data.structures[static_cast<size_t>(pl->structure)].death;
                        if (death_id != 0 && death_id != 0xFFFFFFFFu && static_cast<int>(m_entities.size()) < kMaxEntities) {
                            Entity death_ent;
                            death_ent.archetype = death_id;
                            death_ent.structure = -1;
                            death_ent.role = Role::Food;
                            death_ent.size = pl->size * 0.5f;
                            death_ent.x = pl->x;
                            death_ent.y = pl->y;
                            death_ent.ttl = 5.0f;
                            m_entities.push_back(death_ent);
                        }
                    }
                }
            }
        }
    }

    for (const Entity& d : drops) drop_meat(d);
    if (pl->alive && pl->size >= kEvolveSize) m_evolved = true;
    update_diet();
    for (Entity& e : m_entities) {
        if (!e.alive || e.role != Role::Meat) continue;
        e.ttl -= dt;
        if (e.ttl <= 0.0f) e.alive = false;
    }
    for (Entity& e : m_entities) {
        if (!e.alive || e.hatch_time <= 0.0f) continue;
        e.hatch_time -= dt;
        if (e.hatch_time <= 0.0f && static_cast<int>(m_entities.size()) < kMaxEntities) {
            e.hatch_time = 0.0f;
            e.role = Role::Creature;
            e.size = 0.3f;
            e.anim = kAnimHatch;
            e.anim_time = 0.0f;
            e.anim_duration = 0.5f;
            play_sound("hatch");
            spawn_particles(e.x, e.y, 8, 0xFF80FF80u);
        }
    }
    m_refill -= dt;
    if (m_refill <= 0.0f && !m_food_templates.empty()) {
        m_refill = kRefillInterval;
        int alive_food = 0;
        for (const Entity& e : m_entities) if (e.alive && e.role == Role::Food) ++alive_food;
        Rng rng(static_cast<u32>(m_ticks * 2654435761u) ^ fnv1_lower(m_level.c_str()));
        while (alive_food < m_target_food && static_cast<int>(m_entities.size()) < kMaxEntities) {
            Entity en = m_food_templates[rng.next() % m_food_templates.size()];
            en.x = rng.signed_unit() * m_arena;
            en.y = rng.signed_unit() * m_arena;
            en.vx = rng.signed_unit() * 2.0f;
            en.vy = rng.signed_unit() * 2.0f;
            en.alive = true;
            m_entities.push_back(en);
            ++alive_food;
        }
    }
    for (Particle& p : m_particles) {
        if (p.life <= 0.0f) continue;
        p.vy += 10.0f * dt;
        p.vx *= 1.0f - 0.5f * dt;
        p.vy *= 1.0f - 0.5f * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.life -= dt * 0.5f;
    }
    m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(),
                                     [](const Particle& p) { return p.life <= 0.0f; }),
                      m_particles.end());
    if (pl->alive) {
        if (pl->protect_time > 0.0f) pl->protect_time -= dt;
        if (pl->poison_time > 0.0f) {
            pl->poison_time -= dt;
            const float poison_dps = curve_value("poison_count", 0.0f, 0.0f);
            m_hp -= poison_dps * dt;
                if (m_hp <= 0.0f) {
                    m_hp = 0.0f;
                    m_player_alive = false;
                    m_respawn = kRespawnDelay;
                    pl->alive = false;
                    pl->anim = kAnimDeath;
                    pl->anim_time = 0.0f;
                    pl->anim_duration = 0.5f;
                    play_sound("death");
                    spawn_particles(pl->x, pl->y, 15, 0xFF4040FFu);
                }
        }
        if (pl->electric_time > 0.0f) {
            pl->electric_time -= dt;
            pl->vx = pl->vy = 0.0f;
        }
        const float sp = std::sqrt(pl->vx * pl->vx + pl->vy * pl->vy);
        const bool is_action = (pl->anim == kAnimEat || pl->anim == kAnimGetHitSpike);
        if (is_action && pl->anim_time >= kActionAnimDuration) {
            pl->anim = kAnimIdle;
            pl->anim_time = 0.0f;
        }
        if (!is_action) {
            if (sp > 0.5f) {
                if (pl->anim != kAnimMove) {
                    pl->anim = kAnimMove;
                    pl->anim_time = 0.0f;
                }
            } else {
                if (pl->anim != kAnimIdle) {
                    pl->anim = kAnimIdle;
                    pl->anim_time = 0.0f;
                }
            }
        }
        const float anim_dilation = curve_value("anim_dilation", pl->size, 1.0f);
        pl->anim_time += dt * (anim_dilation > 0.0f ? anim_dilation : 1.0f);
    }
    if (pl->alive) {
        const float speed = std::sqrt(pl->vx * pl->vx + pl->vy * pl->vy);
        const float lookahead = curve_value("camera_lookahead", speed, 0.0f);
        const float lerp_factor = curve_value("camera_lerpfactor", speed, 0.0f);
        float target_x = pl->x;
        float target_y = pl->y;
        if (speed > 0.1f && lookahead > 0.0f) {
            target_x += (pl->vx / speed) * lookahead;
            target_y += (pl->vy / speed) * lookahead;
        }
        const float lerp = lerp_factor > 0.0f ? lerp_factor * dt : 1.0f;
        m_cam_x += (target_x - m_cam_x) * lerp;
        m_cam_y += (target_y - m_cam_y) * lerp;
    } else {
        m_cam_x = m_cam_y = 0.0f;
    }
}

float World::pixels_per_unit() const {
    const float s = player() ? player()->size : kPlayerBaseSize;
    return 16.0f * (1.5f / (0.5f + s));
}

void World::clear() {
    m_data = GameData{};
    m_loaded = false;
    m_source.clear();
    m_level.clear();
    m_entities.clear();
    m_food_templates.clear();
    m_target_food = 0;
    m_refill = 0.0f;
    m_player = -1;
    m_ticks = 0;
    m_time = 0.0f;
    m_player_alive = true;
    m_hp = m_max_hp = 100.0f;
    m_eaten = 0;
    m_dna = 0;
    m_diet = Diet::Herbivore;
    m_eaten_plant = m_eaten_meat = 0;
    m_evolved = false;
    m_respawn = 0.0f;
    m_has_target = false;
    m_target_x = m_target_y = 0.0f;
    m_cam_x = m_cam_y = 0.0f;
}

const Entity* World::player() const {
    if (m_player < 0 || static_cast<size_t>(m_player) >= m_entities.size()) return nullptr;
    return &m_entities[static_cast<size_t>(m_player)];
}

std::string World::status() const {
    if (!m_loaded) return "no world data";
    std::string pl = "-";
    if (const Entity* p = player()) {
        const char* n = naming::instance_name(p->archetype);
        pl = n ? std::string(n) : ("0x" + std::to_string(p->archetype));
    }
    char b[192];
    const char* diet = m_diet == Diet::Carnivore   ? "carnivore"
                       : m_diet == Diet::Omnivore ? "omnivore"
                                                  : "herbivore";
    std::snprintf(b, sizeof(b), "%s: %zu cells, player=%s, eaten=%d, hp=%.0f, dna=%d, diet=%s",
                  m_level.empty() ? "(no level)" : m_level.c_str(), m_entities.size(), pl.c_str(),
                  m_eaten, m_hp, m_dna, diet);
    return b;
}

}  // namespace os::cell
