// Constructed-input tests for cell_world.cpp (D3D-free). With an optional argv[1] =
// a real CellGame.package, also decodes it, spawns level_water and ticks once.
#include "engine/cell_world.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

using namespace os;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; }
}

struct Rec { u32 type, group, instance; std::vector<u8> payload; };

// DBPF index kind 4 (type/group per record, unknown shared in the prefix), stride 28.
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
    put32(ix, 4); put32(ix + 4, 0);
    ix += 8;
    for (size_t k = 0; k < recs.size(); ++k, ix += 28) {
        put32(ix + 0, recs[k].type);
        put32(ix + 4, recs[k].group);
        put32(ix + 8, recs[k].instance);
        put32(ix + 12, offs[k]);
        put32(ix + 16, 0x80000000u | sizes[k]);
        put32(ix + 20, sizes[k]);
        put32(ix + 24, 0x00010000u);
    }
    return out;
}

static std::vector<u8> make_archetype(u32 template_id, const char* disp) {
    std::vector<u8> d(400, 0);
    memcpy(d.data() + 0x000, &template_id, 4);
    for (size_t i = 0; disp[i]; ++i) d[4 + i * 2] = static_cast<u8>(disp[i]);
    return d;
}

static std::vector<u8> make_structure(u32 id, float scale) {
    std::vector<u8> d(0x50, 0);
    (void)id;
    // header +0x44 = base scale xyz
    memcpy(d.data() + 0x44, &scale, 4);
    float s2 = scale; memcpy(d.data() + 0x48, &s2, 4);
    float s3 = scale; memcpy(d.data() + 0x4C, &s3, 4);
    return d;
}

struct PEntry { u32 cell; float np; };
static std::vector<u8> make_population(const std::vector<PEntry>& cells) {
    std::vector<u8> d(16 + cells.size() * 64, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0, 4); put(8, static_cast<u32>(cells.size()));
    for (size_t k = 0; k < cells.size(); ++k) {
        put(16 + k * 64 + 0x18, cells[k].cell);
        memcpy(d.data() + 16 + k * 64 + 0x2C, &cells[k].np, 4);
    }
    return d;
}

int main(int argc, char** argv) {
    using namespace os::cell;

    const u32 s1 = fnv1_lower("struct_food");
    const u32 s2 = fnv1_lower("struct_pink");
    const u32 a1 = fnv1_lower("cell_food_L0");
    const u32 a2 = fnv1_lower("cell_pinky");
    const u32 lvl = fnv1_lower("level_water");

    // 1-8: constructed package: two structures, two archetypes, one level.
    {
        std::vector<Rec> recs = {
            {cell::kTypeCell, 0, a1, make_archetype(s1, "Food")},
            {cell::kTypeCell, 0, a2, make_archetype(s2, "Pink")},
            {cell::kTypeStructure, 0, s1, make_structure(s1, 2.0f)},
            {cell::kTypeStructure, 0, s2, make_structure(s2, 3.0f)},
            {cell::kTypePopulate, 0, lvl, make_population({{a1, 5.0f}, {a2, 3.0f}})},
        };
        auto dbpf = make_dbpf(recs);
        World w; std::string err;
        bool ok = w.load_memory(dbpf.data(), dbpf.size(), "test", err);
        check(ok, "load memory");
        check(w.loaded(), "loaded flag");
        check(w.data().populations.size() == 1, "one population");

        std::string lerr;
        check(w.set_level("level_water", lerr), "set known level");
        check(w.entities().size() == 8, "5+3 cells spawned");
        check(w.level() == "level_water", "level name");

        bool all_struct = true;
        for (const Entity& e : w.entities()) if (e.structure < 0) all_struct = false;
        check(all_struct, "every entity resolves a structure");
        // "Food" is a food name -> Role::Food; entities 0..4 are cell_food_L0, 5..7 cell_pinky.
        check(w.entities()[0].role == Role::Food, "food archetype classified as Food");
        check(w.entities()[5].role == Role::Creature, "non-food archetype classified as Creature");
        check(w.player_index() == 0, "player falls back to first cell");

        float x0 = w.entities()[0].x;
        check(x0 >= -w.arena() && x0 <= w.arena(), "spawn inside arena");

        // dt is clamped to 0.1 s per step, so advance with small steps to accumulate a second.
        for (int i = 0; i < 20; ++i) w.tick(0.05f);
        check(w.ticks() == 20, "tick counter");
        check(w.time() > 0.99f && w.time() < 1.01f, "accumulated time");

        check(w.status().find("level_water") != std::string::npos, "status level");
        check(w.status().find("8 cells") != std::string::npos, "status count");

        std::string lerr2;
        check(!w.set_level("nope", lerr2) && !lerr2.empty(), "unknown level rejected");

        std::string r1 = w.status();
        check(w.set_level("level_water", lerr), "re-spawn level");
        check(w.status() == r1, "deterministic re-spawn");

        w.clear();
        check(!w.loaded() && w.status() == "no world data", "clear resets");
        w.tick(1.0f);
        check(w.ticks() == 0, "tick dropped with no data");
    }

    // 9: load from a non-existent path is an error, not a crash.
    {
        World w; std::string err;
        check(!w.load_path("", err) && !err.empty(), "empty path rejected");
        check(!w.loaded(), "not loaded after empty path");
    }

    // 10+: gameplay - steering, eating/growth, HP/death/respawn, camera. A hero archetype so
    // the player is its own entity; one food species and six creature species so a predator
    // exists with near-certainty (creature size is a hash of the id, range [0.8, 2.4]).
    {
        const u32 hero = fnv1_lower("cell_hero_L0");
        const u32 food = fnv1_lower("cell_food_L0");
        std::vector<Rec> recs;
        recs.push_back({cell::kTypeCell, 0, hero, make_archetype(fnv1_lower("struct_hero"), "Hero")});
        recs.push_back({cell::kTypeCell, 0, food, make_archetype(fnv1_lower("struct_food"), "food")});
        std::vector<PEntry> pop = {{food, 6.0f}};
        for (int i = 0; i < 6; ++i) {
            const std::string nm = "cell_blob" + std::to_string(i);
            const u32 id = fnv1_lower(nm.c_str());
            recs.push_back({cell::kTypeCell, 0, id, make_archetype(id, "Blob")});
            pop.push_back({id, 3.0f});
        }
        recs.push_back({cell::kTypePopulate, 0, lvl, make_population(pop)});

        auto dbpf = make_dbpf(recs);
        World w; std::string err;
        check(w.load_memory(dbpf.data(), dbpf.size(), "gameplay", err), "gameplay load");
        std::string lerr;
        check(w.set_level("level_water", lerr), "gameplay set level");
        check(w.player() != nullptr, "player resolved (hero by id)");
        if (w.player()) {
            check(w.player()->size == 1.0f, "player base size 1.0");
            check(w.player_alive() && w.player_hp() > 0.f, "player starts alive with HP");
        }

        // Steering: a target to the player's +x accelerates it that way; the camera tracks it.
        w.set_target(w.player()->x + 20.f, w.player()->y);
        w.tick(0.1f);
        check(w.player()->vx > 0.f, "steers toward the target");
        check(std::fabs(w.camera_x() - w.player()->x) < 1e-3f, "camera follows the player");

        // Eating food always works (Role::Food is never a predator) and grows the player.
        int fi = -1;
        for (size_t i = 0; i < w.entities().size(); ++i)
            if (w.entities()[i].role == Role::Food && w.entities()[i].alive) { fi = (int)i; break; }
        check(fi >= 0, "a food entity exists");
        if (fi >= 0) {
            const float before = w.player()->size;
            w.teleport_player(w.entities()[fi].x, w.entities()[fi].y);
            w.tick(1.0f / 60.0f);
            check(w.eaten() >= 1, "eating food increments the eaten count");
            check(w.player()->size >= before, "eating grows the player");
        }

        // Predator: overlap a larger creature every step until the player is bitten to death,
        // then confirm it respawns.
        int pri = -1;
        for (size_t i = 0; i < w.entities().size(); ++i) {
            const Entity& e = w.entities()[i];
            if ((int)i != w.player_index() && e.alive && e.role == Role::Creature &&
                e.size > w.player()->size * 1.05f) { pri = (int)i; break; }
        }
        check(pri >= 0, "a predator creature exists");
        if (pri >= 0) {
            for (int k = 0; k < 400 && w.player_alive(); ++k) {
                w.teleport_player(w.entities()[pri].x, w.entities()[pri].y);
                w.tick(0.05f);
            }
            check(!w.player_alive(), "a larger creature bites the player to death");
            bool respawned = false;
            for (int k = 0; k < 200 && !respawned; ++k) {
                w.tick(0.05f);
                if (w.player_alive()) respawned = true;
            }
            check(respawned, "player respawns after death");
            check(w.player_hp() > 0.f, "HP restored on respawn");
        }
    }

    // Optional: real package.
    if (argc > 1 && argv[1] && argv[1][0]) {
        World w; std::string err;
        if (w.load_path(argv[1], err)) {
            std::string lerr;
            if (w.set_level("level_water", lerr)) {
                w.tick(0.016f);
                std::cout << "real world: " << w.data().archetypes.size() << " archetypes, "
                          << w.data().structures.size() << " structures; " << w.status() << "\n";
            } else {
                std::cerr << "real level spawn failed: " << lerr << "\n";
                ++failures;
            }
        } else {
            std::cerr << "real package load failed: " << err << "\n";
            ++failures;
        }
    }

    if (failures) {
        std::cerr << "cell_world_verify: " << failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "cell_world_verify: constructed-input checks PASS\n";
    return 0;
}
