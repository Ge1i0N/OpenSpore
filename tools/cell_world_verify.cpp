// Constructed-input tests for cell_world.cpp (D3D-free). With an optional argv[1] =
// a real CellGame.package, also decodes it, spawns level_water and ticks once.
#include "engine/cell_world.h"

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

static std::vector<u8> make_population(u32 cell_a, float a_n, u32 cell_b, float b_n) {
    std::vector<u8> d(16 + 2 * 64, 0);
    auto put = [&](size_t at, u32 x) { memcpy(d.data() + at, &x, 4); };
    put(0, 4); put(8, 2);  // header type 4, 2 entries
    put(16 + 0x18, cell_a); memcpy(d.data() + 16 + 0x2C, &a_n, 4);
    put(16 + 64 + 0x18, cell_b); memcpy(d.data() + 16 + 64 + 0x2C, &b_n, 4);
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
            {cell::kTypePopulate, 0, lvl, make_population(a1, 5.0f, a2, 3.0f)},
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
        // food cells (a1) carry structure s1 -> scale 2.0
        check(w.entities()[0].size == 2.0f, "size from structure scale");
        check(w.player_index() == 0, "player falls back to first cell");

        // spawn distribution uses a deterministic RNG, so it is reproducible across loads.
        float x0 = w.entities()[0].x;
        check(x0 >= -w.arena() && x0 <= w.arena(), "spawn inside arena");

        w.tick(0.5f);
        w.tick(0.5f);
        check(w.ticks() == 2, "tick counter");
        check(w.time() > 0.99f && w.time() < 1.01f, "accumulated time");

        check(w.status().find("level_water") != std::string::npos, "status level");
        check(w.status().find("8 cells") != std::string::npos, "status count");

        // unknown level is an error, not a crash; world stays usable.
        std::string lerr2;
        check(!w.set_level("nope", lerr2) && !lerr2.empty(), "unknown level rejected");

        // entity ids/positions: two loads of the same level agree (deterministic).
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
