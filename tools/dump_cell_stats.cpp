#include "engine/cell_game.h"
#include "engine/cell_world.h"

#include <cstdio>
#include <cstring>

using namespace os;

int main(int argc, char** argv) {
    const char* path = argc > 1 ? argv[1] : "Data/CellGame.package";
    cell::GameData data;
    std::string error;
    if (!cell::decode(path, data, error)) {
        std::fprintf(stderr, "decode failed: %s\n", error.c_str());
        return 1;
    }

    std::printf("=== Cell Game Data ===\n");
    std::printf("archetypes: %zu\n", data.archetypes.size());
    std::printf("structures: %zu\n", data.structures.size());
    std::printf("loot_tables: %zu\n", data.loot_tables.size());
    std::printf("look_tables: %zu\n", data.look_tables.size());
    std::printf("populations: %zu\n", data.populations.size());
    std::printf("random_spawns: %zu\n", data.random_spawns.size());
    std::printf("worlds: %zu\n", data.worlds.size());
    std::printf("globals: %zu\n", data.globals.size());
    std::printf("layers: %zu\n", data.layers.size());
    std::printf("background bands: %zu\n", data.background.size());
    std::printf("properties: %zu\n", data.properties.size());
    std::printf("total resources: %u\n", data.total_resources);
    std::printf("\n");

    std::printf("=== Archetypes (first 20) ===\n");
    for (size_t i = 0; i < data.archetypes.size() && i < 20; ++i) {
        const cell::Archetype& a = data.archetypes[i];
        std::printf("0x%08X  display=\"%s\"  name=\"%s\"  template=0x%08X  parts=%u\n",
                    a.id, a.display.c_str(), a.name.c_str(), a.template_id, a.part_count);
        for (u32 off : cell::kStatOffsets) {
            float v = a.stat(off);
            if (v != 0.0f) {
                std::printf("    +0x%03X: %.4f\n", off, v);
            }
        }
        std::printf("\n");
    }

    std::printf("=== Globals ===\n");
    for (const cell::Globals& g : data.globals) {
        std::printf("0x%08X  name=\"%s\"\n", g.id, g.name.c_str());
        std::printf("  refs:");
        for (u32 r : g.refs) std::printf(" 0x%08X", r);
        std::printf("\n");
        std::printf("  floats:");
        for (float f : g.floats) std::printf(" %.4f", f);
        std::printf("\n\n");
    }

    return 0;
}
