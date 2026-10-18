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

constexpr int kMaxEntities = 400;
constexpr float kPlayerAccel = 70.0f;
constexpr float kPlayerSpeed = 24.0f;
constexpr float kDrag = 4.0f;
constexpr float kPredatorAccel = 34.0f;
constexpr float kPredatorSpeed = 13.0f;
constexpr float kPreyAccel = 30.0f;
constexpr float kPreySpeed = 15.0f;
constexpr float kAggro = 24.0f;
constexpr float kPredatorFraction = 1.05f;
constexpr float kGrowRate = 0.55f;
constexpr float kMaxSize = 16.0f;
constexpr float kPlayerBaseSize = 1.0f;
constexpr float kEvolveSize = 8.0f;  // SEED: not recovered
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
        const cell::CreatureStats& c = creatures[0];
        const float px = static_cast<float>(static_cast<i32>(c.pos_x)) / 1000.0f;
        const float py = static_cast<float>(static_cast<i32>(c.pos_y)) / 1000.0f;
        const float vx = static_cast<float>(static_cast<i32>(c.pos_z)) / 100.0f;
        const float vy = static_cast<float>(static_cast<i32>(c.pos_w)) / 100.0f;
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
    }
    return true;
}

bool World::is_predator_archetype(u32 archetype_id) const {
    for (const Archetype& a : m_data.archetypes) {
        if (a.id != archetype_id) continue;
        const std::string name = archetype_label(m_data, a);
        if (contains_ci(name, "chomper") || contains_ci(name, "jawhead") ||
            contains_ci(name, "megamouth") || contains_ci(name, "squid") ||
            contains_ci(name, "poker") || contains_ci(name, "puffish") ||
            contains_ci(name, "booster") || contains_ci(name, "charger")) {
            return true;
        }
        return false;
    }
    return false;
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

bool World::write_save(const std::string& path, std::string& error) const {
    if (path.empty()) {
        error = "empty save path";
        return false;
    }
    std::vector<u8> payload;
    auto put32 = [&](u32 v) { payload.push_back(v & 0xFF); payload.push_back((v >> 8) & 0xFF); payload.push_back((v >> 16) & 0xFF); payload.push_back((v >> 24) & 0xFF); };
    auto putf32 = [&](float f) { u32 bits; memcpy(&bits, &f, 4); put32(bits); };
    put32(0x0240E421); put32(0x0240E422); put32(0x0240E423); put32(0x02E9EFD3); put32(0x03CDD54B);
    const Entity* p = player();
    if (p) {
        put32(static_cast<u32>(static_cast<i32>(p->x * 1000.0f)));
        put32(static_cast<u32>(static_cast<i32>(p->y * 1000.0f)));
        put32(static_cast<u32>(static_cast<i32>(p->vx * 100.0f)));
        put32(static_cast<u32>(static_cast<i32>(p->vy * 100.0f)));
    } else {
        put32(0); put32(0); put32(0); put32(0);
    }
    for (int i = 0; i < 25; ++i) put32(0);
    for (int i = 0; i < 11; ++i) put32(0);
    for (int i = 0; i < 4; ++i) put32(0);
    put32(m_dna); put32(m_eaten);
    for (int i = 0; i < 35; ++i) put32(0);
    if (p) putf32(p->size); else putf32(1.0f);
    for (int i = 0; i < 7; ++i) put32(0);
    for (int i = 0; i < 24; ++i) payload.push_back(0);
    std::vector<u8> out;
    auto put32be = [&](u32 v) { out.push_back((v >> 24) & 0xFF); out.push_back((v >> 16) & 0xFF); out.push_back((v >> 8) & 0xFF); out.push_back(v & 0xFF); };
    out.insert(out.end(), {'D', 'B', 'P', 'F'});
    put32be(3);  // major version
    put32be(0);  // minor version
    put32be(0x60);  // index offset
    put32be(8 + 28);  // index size (1 record)
    out.resize(0x60, 0);
    out.insert(out.end(), payload.begin(), payload.end());
    put32be(0x018EB4B7);  // type
    put32be(0);  // group
    put32be(0);  // instance
    put32be(0x60);  // offset
    put32be(static_cast<u32>(payload.size()));  // size
    put32be(0x80000000u | static_cast<u32>(payload.size()));  // compressed size
    put32be(static_cast<u32>(payload.size()));  // uncompressed size
    put32be(0x00010000u);  // flags
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
    for (const NamedResource& w : m_data.worlds) {
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

std::string World::next_level(const std::string& current) const {
    const u32 cur_id = fnv1_lower(current.c_str());
    for (const Population& p : m_data.populations) {
        if (p.id == cur_id || p.name == current) {
            if (p.type == 4) {
                std::string next = current + "_2";
                const u32 next_id = fnv1_lower(next.c_str());
                for (const Population& np : m_data.populations) {
                    if (np.id == next_id || np.name == next) return next;
                }
            }
            return "";
        }
    }
    return "";
}

const LootTable* World::biome_loot(const char* biome_name) const {
    if (!biome_name || !*biome_name) return nullptr;
    std::string prefix = "loot_";
    std::string table_name = prefix + biome_name;
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
                en.hatch_time = curve_value("hatchDuration", 0.0f, 3.0f);
            }
            float struct_scale = 0.0f;
            if (en.structure >= 0 && static_cast<size_t>(en.structure) < m_data.structures.size()) {
                const float s = m_data.structures[static_cast<size_t>(en.structure)].scale[0];
                if (s > 0.0f) struct_scale = s;
            }
            if (struct_scale > 0.0f) {
                en.size = food ? struct_scale * 0.3f : struct_scale;
            } else {
                en.size = food ? (0.40f + 0.12f * hash01(e.cell))
                               : (0.55f + 1.35f * hash01(e.cell));
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
                const bool food = looks_like_food(name);
                for (int k = 0; k < n && static_cast<int>(m_entities.size()) < kMaxEntities; ++k) {
                    Entity en;
                    en.archetype = e.cell;
                    if (arch) en.structure = structure_of(*arch);
                    en.role = food ? Role::Food : Role::Creature;
                    en.diet = diet_from_name(name);
                    en.size = food ? (0.40f + 0.12f * hash01(e.cell))
                                   : (0.55f + 1.35f * hash01(e.cell));
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

float World::stat_for_archetype(u32 archetype_id, u32 offset) const {
    for (const Archetype& a : m_data.archetypes) {
        if (a.id == archetype_id) return a.stat(offset);
    }
    return 0.0f;
}

float World::hp_for_size(float size) const {
    return 50.0f + size * 25.0f;
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
    if (biome_lt && biome_lt != lt) {
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
        const float dx = px - e.x, dy = py - e.y;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const float eye_count = curve_value("eye_count", 0.0f, 1.0f);
        const float aggro = kAggro * eye_count;
        if (dist < aggro && dist > 0.001f) {
            const bool predator = e.role == Role::Creature &&
                (e.size > psize * kPredatorFraction || is_predator_archetype(e.archetype));
            const bool prey = e.role == Role::Food ||
                              (e.role == Role::Creature && e.size <= psize * kPredatorFraction);
            const char* table = predator ? "npc_mad" : (prey ? "npc_scared" : "NPC_idle");
            const float pursue_w = look_weight(table, 4);   // pursue_target
            const float flee_w = look_weight(table, 5);     // flee_target
            const float turn_factor = curve_value("npc_turn_factor", e.size, 1.0f);
            if (predator) {
                const float w = (pursue_w > 0.f ? pursue_w : 1.0f) * turn_factor;
                e.vx += (dx / dist) * kPredatorAccel * w * dt;
                e.vy += (dy / dist) * kPredatorAccel * w * dt;
            } else if (prey && e.role == Role::Creature) {
                const float w = (flee_w > 0.f ? flee_w : 1.0f) * turn_factor;
                e.vx -= (dx / dist) * kPreyAccel * w * dt;
                e.vy -= (dy / dist) * kPreyAccel * w * dt;
            }
            const float flock_w = look_weight("NPC_idle", 6);
            if (flock_w > 0.0f && e.role == Role::Creature) {
                float avg_x = 0.0f, avg_y = 0.0f;
                int count = 0;
                for (const Entity& other : m_entities) {
                    if (!other.alive || other.role != Role::Creature) continue;
                    if (other.archetype == e.archetype) {
                        const float dx2 = other.x - e.x, dy2 = other.y - e.y;
                        if (dx2 * dx2 + dy2 * dy2 < kAggro * kAggro) {
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
            const float cap = predator ? kPredatorSpeed : kPreySpeed;
            const float sp = std::sqrt(e.vx * e.vx + e.vy * e.vy);
            if (sp > cap) {
                e.vx *= cap / sp;
                e.vy *= cap / sp;
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
            if (e.role == Role::Food || e.role == Role::Meat ||
                e.size <= pl->size * kPredatorFraction) {
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
            } else {
                const float protect_time = curve_value("protectTime", 0.0f, 0.5f);
                if (pl->protect_time <= 0.0f) {
                    m_hp -= e.size * 18.0f * dt;
                    pl->protect_time = protect_time;
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
