#include "engine/appmodes.h"
#include <cstring>
#include <iostream>

int main() {
    using namespace os;
    const auto* binding = appmodes::primary_ui("GalaxyGameEntry");
    if (!binding || std::strcmp(binding->layout, "GlobalUIGGE-9") ||
        fnv1_lower(binding->layout) != binding->instance || binding->instance != 0x52c07413u ||
        binding->mount_control != 0x2edd95cau || binding->load_function != 0x00de5900u ||
        appmodes::primary_ui("galaxygameentry") != binding || appmodes::primary_ui(nullptr) ||
        appmodes::primary_ui("") || appmodes::primary_ui("Game_Creature") ||
        appmodes::primary_ui("GalaxyGameEntryExtra")) {
        std::cerr << "mode UI binding / hash / exact-name regression\n";
        return 1;
    }
    std::cout << "GA GGE primary UI binding/hash/exact-name boundaries PASS (not a mode implementation)\n";
}
