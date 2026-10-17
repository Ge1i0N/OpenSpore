#include "engine/cell_world.h"

#include <cstdio>

#include "engine/fs.h"
#include "engine/naming.h"

namespace os::cell {
namespace {

// A tiny deterministic PRNG (xorshift32) seeded from the level name. Placement is a seed,
// not the original's algorithm (see the header): the only thing this guarantees is that the
// same data always produces the same world.
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

const Archetype* find_archetype(const GameData& d, const char* name) {
    for (const Archetype& a : d.archetypes) {
        if (a.name == name) return &a;
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

bool World::load_default(std::string& error) {
    const auto path = fs::resolve("Data/CellGame.package");
    if (path.empty()) {
        error = "Data/CellGame.package not found on the search paths";
        return false;
    }
    return load_path(path.string(), error);
}

bool World::set_level(const std::string& level, std::string& error) {
    if (!m_loaded) {
        error = "no CellGame data loaded";
        return false;
    }
    const Population* pop = nullptr;
    const u32 want = fnv1_lower(level.c_str());
    for (const Population& p : m_data.populations) {
        // Match by resolved name, or by the level's own FNV-1 instance id (so a level still
        // resolves when the name registry is unavailable).
        if (p.name == level || p.id == want) {
            pop = &p;
            break;
        }
    }
    if (!pop) {
        error = "unknown level '" + level + "'";
        return false;
    }
    // Valid level: only now reset the world, so a bad name leaves the current world intact.
    m_entities.clear();
    m_player = -1;
    m_ticks = 0;
    m_time = 0.0f;
    m_level = level;

    Rng rng(fnv1_lower(level.c_str()));
    for (const SpawnEntry& e : pop->entries) {
        if (e.cell == 0 || e.count <= 0.0f) continue;
        int n = static_cast<int>(e.count + 0.5f);
        if (n < 1) n = 1;
        for (int k = 0; k < n && static_cast<int>(m_entities.size()) < kMaxEntities; ++k) {
            Entity en;
            en.archetype = e.cell;
            for (const Archetype& a : m_data.archetypes) {
                if (a.id == e.cell) {
                    en.structure = structure_of(a);
                    break;
                }
            }
            if (en.structure >= 0) {
                const float sx = m_data.structures[en.structure].scale[0];
                if (sx > 0.0f) en.size = sx;
            }
            en.x = rng.signed_unit() * m_arena;
            en.y = rng.signed_unit() * m_arena;
            en.vx = rng.signed_unit() * 2.0f;
            en.vy = rng.signed_unit() * 2.0f;
            m_entities.push_back(en);
        }
    }

    // The player: the shipped hero archetype (`cell_hero_L0`), else the Minno part cell. A
    // seed choice, flagged as such - the original's player-creation path is not recovered.
    const Archetype* hero = find_archetype(m_data, "cell_hero_L0");
    if (!hero) hero = find_archetype(m_data, "cell_minno1");
    if (hero) {
        Entity en;
        en.archetype = hero->id;
        en.structure = structure_of(*hero);
        if (en.structure >= 0) {
            const float sx = m_data.structures[en.structure].scale[0];
            if (sx > 0.0f) en.size = sx;
        }
        m_entities.push_back(en);
        m_player = static_cast<int>(m_entities.size()) - 1;
    } else if (!m_entities.empty()) {
        m_player = 0;
    }
    error.clear();
    return true;
}

void World::tick(float dt) {
    if (!m_loaded) return;
    ++m_ticks;
    m_time += dt;
    const float span = m_arena * 2.0f;
    for (Entity& e : m_entities) {
        e.x += e.vx * dt;
        e.y += e.vy * dt;
        // Wrap inside the arena so a sandbox cannot escape.
        if (e.x < -m_arena) e.x += span;
        if (e.x > m_arena) e.x -= span;
        if (e.y < -m_arena) e.y += span;
        if (e.y > m_arena) e.y -= span;
    }
}

void World::clear() {
    m_data = GameData{};
    m_loaded = false;
    m_source.clear();
    m_level.clear();
    m_entities.clear();
    m_player = -1;
    m_ticks = 0;
    m_time = 0.0f;
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
    char b[160];
    std::snprintf(b, sizeof(b), "%s: %zu cells, player=%s",
                  m_level.empty() ? "(no level)" : m_level.c_str(), m_entities.size(), pl.c_str());
    return b;
}

}  // namespace os::cell
