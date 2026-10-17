#include "engine/cell_world.h"

#include <cctype>
#include <cmath>
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

// Simulation seeds (see the header note: these numbers are OURS, not the shipped tide pool's).
constexpr float kPlayerAccel = 70.0f;    // units/s^2 toward the steering target
constexpr float kPlayerSpeed = 24.0f;    // units/s cap
constexpr float kDrag = 4.0f;            // velocity damping per second
constexpr float kPredatorAccel = 34.0f;
constexpr float kPredatorSpeed = 13.0f;
constexpr float kPreyAccel = 30.0f;
constexpr float kPreySpeed = 15.0f;
constexpr float kAggro = 36.0f;          // predator/prey reaction radius
constexpr float kPredatorFraction = 1.05f;  // above player.size * this, a creature bites
constexpr float kGrowRate = 0.55f;       // area added per eaten unit^2 (tuned so a level evolves)
constexpr float kMaxSize = 4.0f;
constexpr float kPlayerBaseSize = 1.0f;
constexpr float kEvolveSize = 3.0f;      // size at which the cell could leave for the Creature stage
constexpr float kRespawnDelay = 1.5f;

bool contains_ci(const std::string& hay, const char* needle) {
    if (hay.empty()) return false;
    std::string h;
    h.reserve(hay.size());
    for (char c : hay) h.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return h.find(needle) != std::string::npos;
}

// Which name does an archetype go by? The registry name for its id when the tables are loaded,
// else the decoded display name. Empty when neither resolves (many spawn ids are assigned, not
// hashed).
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

bool World::load_default(std::string& error) {
    const auto path = fs::resolve("Data/CellGame.package");
    if (path.empty()) {
        error = "Data/CellGame.package not found on the search paths";
        return false;
    }
    return load_path(path.string(), error);
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
        const bool food = looks_like_food(name);
        for (int k = 0; k < n && static_cast<int>(m_entities.size()) < kMaxEntities; ++k) {
            Entity en;
            en.archetype = e.cell;
            if (arch) en.structure = structure_of(*arch);
            en.role = food ? Role::Food : Role::Creature;
            // Sizes are seeds: food is small and uniform-ish; a creature's size is a stable
            // function of its archetype id so one species reads consistently on screen.
            en.size = food ? (0.40f + 0.12f * hash01(e.cell))
                           : (0.80f + 1.60f * hash01(e.cell));
            en.x = rng.signed_unit() * m_arena;
            en.y = rng.signed_unit() * m_arena;
            en.vx = rng.signed_unit() * 2.0f;
            en.vy = rng.signed_unit() * 2.0f;
            m_entities.push_back(en);
        }
    }
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
    m_player_alive = true;
    m_hp = m_max_hp = 100.0f;
    m_eaten = 0;
    m_evolved = false;
    m_respawn = 0.0f;
    m_has_target = false;
    m_cam_x = m_cam_y = 0.0f;

    spawn_from_level(*pop);

    // The player: the shipped hero archetype (`cell_hero_L0`), else the Minno part cell. A
    // seed choice, flagged as such - the original's player-creation path is not recovered.
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
    m_hp = m_max_hp;
    m_player_alive = true;
    m_respawn = 0.0f;
    m_has_target = false;
}

void World::tick(float dt) {
    if (!m_loaded) return;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > 0.1f) dt = 0.1f;  // a stalled frame must not tunnel the simulation
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

    // --- Steering and AI --------------------------------------------------
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
        if (dist < kAggro && dist > 0.001f) {
            const bool predator = e.role == Role::Creature && e.size > psize * kPredatorFraction;
            const bool prey = e.role == Role::Food ||
                              (e.role == Role::Creature && e.size <= psize * kPredatorFraction);
            if (predator) {
                e.vx += (dx / dist) * kPredatorAccel * dt;
                e.vy += (dy / dist) * kPredatorAccel * dt;
            } else if (prey && e.role == Role::Creature) {
                e.vx -= (dx / dist) * kPreyAccel * dt;
                e.vy -= (dy / dist) * kPreyAccel * dt;
            }
            const float cap = predator ? kPredatorSpeed : kPreySpeed;
            const float sp = std::sqrt(e.vx * e.vx + e.vy * e.vy);
            if (sp > cap) {
                e.vx *= cap / sp;
                e.vy *= cap / sp;
            }
        }
    }

    // --- Integrate and wrap ----------------------------------------------
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

    // --- Collisions: eat, grow, bite -------------------------------------
    if (m_player_alive && pl->alive) {
        for (size_t i = 0; i < m_entities.size(); ++i) {
            if (static_cast<int>(i) == m_player) continue;
            Entity& e = m_entities[i];
            if (!e.alive) continue;
            const float dx = e.x - pl->x, dy = e.y - pl->y;
            const float reach = pl->size + e.size;
            if (dx * dx + dy * dy > reach * reach) continue;
            if (e.role == Role::Food || e.size <= pl->size * kPredatorFraction) {
                // Eaten: grow by area (so a big meal matters more), then retire the entity.
                const float area = pl->size * pl->size + e.size * e.size * kGrowRate;
                pl->size = std::sqrt(area) > kMaxSize ? kMaxSize : std::sqrt(area);
                e.alive = false;
                ++m_eaten;
            } else {
                // A larger creature bites: drain HP and shove the player away.
                m_hp -= e.size * 18.0f * dt;
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.001f) {
                    pl->vx -= (dx / len) * 40.0f * dt;
                    pl->vy -= (dy / len) * 40.0f * dt;
                }
                if (m_hp <= 0.0f) {
                    m_hp = 0.0f;
                    m_player_alive = false;
                    m_respawn = kRespawnDelay;
                    pl->alive = false;
                }
            }
        }
    }

    if (pl->alive && pl->size >= kEvolveSize) m_evolved = true;

    // --- Camera follows the player (arena centre once it is gone) ---------
    if (pl->alive) {
        m_cam_x = pl->x;
        m_cam_y = pl->y;
    } else {
        m_cam_x = m_cam_y = 0.0f;
    }
}

float World::pixels_per_unit() const {
    // Tuned for a 720-tall reference viewport; the renderer scales by (height / 720). Zooming out
    // as the player grows is a seed choice, but it keeps the whole arena legible at every size.
    const float s = player() ? player()->size : kPlayerBaseSize;
    return 16.0f * (1.5f / (0.5f + s));
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
    m_player_alive = true;
    m_hp = m_max_hp = 100.0f;
    m_eaten = 0;
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
    std::snprintf(b, sizeof(b), "%s: %zu cells, player=%s, eaten=%d, hp=%.0f",
                  m_level.empty() ? "(no level)" : m_level.c_str(), m_entities.size(), pl.c_str(),
                  m_eaten, m_hp);
    return b;
}

}  // namespace os::cell
