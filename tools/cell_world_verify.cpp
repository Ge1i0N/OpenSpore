// Constructed-input tests for cell_world.cpp (D3D-free). With an optional argv[1] =
// a real CellGame.package, also decodes it, spawns level_water and ticks once.
#include "engine/cell_world.h"
#include "engine/cell_game.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
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
            check(w.eaten_plant() >= 1, "eating plankton counts as herbivore food");
            check(w.dna() > 0, "eating awards DNA");
            check(w.diet() == Diet::Herbivore, "diet is herbivore after eating plants");
            check(strcmp(w.player()->anim, cell::kAnimEat) == 0, "eating sets ANIM_EAT_JAW animation");
        }

        // Eating a smaller cell counts as flesh: the diet shifts and a meat chunk is dropped.
        int ci = -1;
        for (size_t i = 0; i < w.entities().size(); ++i) {
            const Entity& e = w.entities()[i];
            if ((int)i != w.player_index() && e.alive && e.role == Role::Creature &&
                e.size <= w.player()->size * 1.05f) { ci = (int)i; break; }
        }
        check(ci >= 0, "a prey creature exists");
        if (ci >= 0) {
            const size_t before = w.entities().size();
            w.teleport_player(w.entities()[ci].x, w.entities()[ci].y);
            w.tick(1.0f / 60.0f);
            check(w.eaten_meat() >= 1, "eating a smaller cell counts as flesh");
            check(w.entities().size() > before, "a meat chunk is dropped where the cell died");
            check(w.diet() != Diet::Herbivore, "diet is no longer herbivore after eating flesh");
            bool meat = false;
            for (const Entity& e : w.entities()) if (e.alive && e.role == Role::Meat) meat = true;
            check(meat, "the dropped chunk is edible meat");
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
                // Reset protection so the predator can deal damage.
                w.reset_protection();
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

    // 11: save round-trip — write_save emits 0x01A80D26 frames that load_save reads back.
    {
        const u32 hero = fnv1_lower("cell_hero_L0");
        const u32 food = fnv1_lower("cell_food_L0");
        std::vector<Rec> recs = {
            {cell::kTypeCell, 0, hero, make_archetype(fnv1_lower("struct_hero"), "Hero")},
            {cell::kTypeCell, 0, food, make_archetype(fnv1_lower("struct_food"), "food")},
            {cell::kTypePopulate, 0, lvl, make_population({{food, 4.0f}})},
        };
        auto dbpf = make_dbpf(recs);
        World w; std::string err;
        check(w.load_memory(dbpf.data(), dbpf.size(), "save", err), "save load");
        std::string lerr;
        check(w.set_level("level_water", lerr), "save set level");
        // Eat a food item so the save carries a non-zero DNA / eaten count.
        for (size_t i = 0; i < w.entities().size(); ++i) {
            if (w.entities()[i].role == Role::Food && w.entities()[i].alive) {
                w.teleport_player(w.entities()[i].x, w.entities()[i].y);
                w.tick(0.05f);
                break;
            }
        }
        check(w.dna() > 0, "eating awards DNA before the save");
        const int dna_before = w.dna();
        const int eaten_before = w.eaten();
        if (w.player()) {
            w.teleport_player(12.0f, -7.0f);
            w.tick(0.05f);
        }
        const char* path = "cell_roundtrip_test.spo";
        std::string werr;
        check(w.write_save(path, werr), "write_save");
        // Re-open a fresh world and restore the save.
        World w2; std::string lerr2;
        check(w2.load_memory(dbpf.data(), dbpf.size(), "save2", lerr2), "save2 load");
        std::string lerr3;
        check(w2.set_level("level_water", lerr3), "save2 set level");
        std::string serr;
        check(w2.load_save(path, serr), "load_save");
        if (w2.player()) {
            check(std::fabs(w2.player()->x - 12.0f) < 0.5f, "save restores player x");
            check(std::fabs(w2.player()->y - (-7.0f)) < 0.5f, "save restores player y");
        } else {
            check(false, "save restores a player");
        }
        check(w2.dna() == dna_before, "save restores the DNA count");
        check(w2.eaten() == eaten_before, "save restores the eaten count");
        _unlink(path);
    }

    // 12: the `_bg` population is scenery (Role::Prop), never hunted or eaten.
    {
        const u32 hero = fnv1_lower("cell_hero_L0");
        const u32 food = fnv1_lower("cell_food_L0");
        std::vector<Rec> recs = {
            {cell::kTypeCell, 0, hero, make_archetype(fnv1_lower("struct_hero"), "Hero")},
            {cell::kTypeCell, 0, food, make_archetype(fnv1_lower("struct_food"), "food")},
            {cell::kTypePopulate, 0, fnv1_lower("level_water"), make_population({{food, 2.0f}})},
            {cell::kTypePopulate, 0, fnv1_lower("level_water_bg"),
             make_population({{fnv1_lower("cell_jawhead"), 3.0f}})},
        };
        auto dbpf = make_dbpf(recs);
        World w; std::string err;
        check(w.load_memory(dbpf.data(), dbpf.size(), "bg", err), "bg load");
        std::string lerr;
        check(w.set_level("level_water", lerr), "bg set level");
        int props = 0, others = 0;
        for (const Entity& e : w.entities()) {
            if (e.role == Role::Prop) ++props; else ++others;
        }
        check(props == 3, "the _bg population spawns as scenery props");
        check(others == 3, "the main level and the player spawn normally");
    }

    // 13: a missing or corrupt save is an error, not a crash (the stage loads a user-writable
    // .spo at startup).
    {
        World w; std::string err;
        check(!w.load_save("cell_missing_save.spo", err) && !err.empty(), "missing save is an error");
        FILE* f = nullptr;
        if (fopen_s(&f, "cell_corrupt_test.spo", "wb") == 0 && f) {
            const char* junk = "not a dbpf file at all";
            fwrite(junk, 1, 26, f);
            fclose(f);
        }
        World w2; std::string err2;
        check(!w2.load_save("cell_corrupt_test.spo", err2) && !err2.empty(), "corrupt save is an error");
        _unlink("cell_corrupt_test.spo");
    }

    // 14: the shipped animation table resolves every cell-stage animation event.
    {
        static const char* kEvents[] = {
            "ANIM_MOVE", "ANIM_IDLE", "ANIM_EAT_JAW", "ANIM_EAT_FAIL_JAW", "ANIM_EAT_FILTER",
            "ANIM_EAT_PROB", "ANIM_DEATH", "ANIM_DEAD", "ANIM_HATCH", "ANIM_GROW", "ANIM_ZAP",
            "ANIM_GET_HIT_SPIKE", "ANIM_GET_HIT_POISON", "ANIM_GET_HIT_ELEC", "ANIM_ATTACK_JAW",
            "ANIM_ATTACK_SPIKE", "ANIM_POP", "ANIM_STARTLED", "ANIM_SCARED", "ANIM_MOVE_MAD",
            "ANIM_MOVE_SCARED", "ANIM_POKE_SPIKE", "ANIM_PROB_STUN", "ANIM_PROB_TARGET_EXPEL",
            "ANIM_PROB_SOURCE_EXPELED", "ANIM_GLANCE_JAW", "ANIM_GLANCE_SPIKE", "ANIM_JOUST_SPIKE"};
        bool all = true;
        for (const char* e : kEvents) {
            if (!cell::find_anim(e)) all = false;
        }
        check(all, "every cell animation event resolves in the shipped table");
    }

    // Optional: real package.
    if (argc > 1 && argv[1] && argv[1][0]) {
        World w; std::string err;
        if (w.load_path(argv[1], err)) {
            std::string lerr;
            if (w.set_level("level_water", lerr)) {
                w.tick(0.016f);
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    for (int i = 0; i < 60; ++i) w.tick(1.0f / 60.0f);
                    const auto t1 = std::chrono::steady_clock::now();
                    const double ms =
                        std::chrono::duration<double, std::milli>(t1 - t0).count();
                    std::printf("cell_world_verify: %zu entities, 60 ticks in %.1f ms "
                                "(%.3f ms/tick)\n",
                                w.entities().size(), ms, ms / 60.0);
                }
                std::cout << "real world: " << w.data().archetypes.size() << " archetypes, "
                          << w.data().structures.size() << " structures; " << w.status() << "\n";
            } else {
                std::cerr << "real level spawn failed: " << lerr << "\n";
                ++failures;
            }
            // The shipped archetype record carries the real cAIData/scalar fields.
            const Archetype* hero = nullptr;
            for (const auto& a : w.data().archetypes) if (a.name == "cell_hero_L0") hero = &a;
            check(hero != nullptr, "real package has cell_hero_L0");
            if (hero) {
                check(hero->scalars.hp == 3, "hero hp tier from record +0xa8");
                check(std::fabs(hero->ai.grow_amount - 0.275f) < 1e-4f, "hero growAmount from cAIData +0x9c");
                check(std::fabs(hero->ai.food - 10000.0f) < 1e-3f, "hero food value from cAIData +0x68");
            }
            // The campaign level list is in the shipped `world` records: every world lists the
            // level populations it hosts and the world_ice record lists all ten campaign levels.
            const std::vector<std::string> levels = w.level_list();
            check(levels.size() == 11, "the shipped world records list eleven campaign levels");
            if (levels.size() == 11) {
                check(levels[0] == "level_ice" && levels[1] == "level_ice_2" &&
                          levels[2] == "level_rock" && levels[3] == "level_rock_2" &&
                          levels[6] == "level_water" && levels[7] == "level_water_2" &&
                          levels[8] == "level_beach" && levels[9] == "level_beach_2" &&
                          levels[10] == "level_random",
                      "the shipped campaign level order");
            }
            check(w.next_level("level_water") == "level_water_2", "next_level uses the shipped stage");
            check(w.next_level("level_water_2") == "level_beach", "next_level walks the shipped biome list");
            check(w.next_level("level_beach_2") == "level_random", "next_level reaches the random level");
            check(w.next_level("level_random").empty(), "the random level is the last one");
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
