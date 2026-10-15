#include "engine/appmodes.h"
#include <cctype>

namespace os::appmodes {
namespace {
// GA 0x136bce0 constructs the wide name at 0x16a0d24 from 0x147d3f4.
// 0xde5900 hashes it, passes the descriptor to GlobalUI +8 -> 0xe035f0 -> 0x8121c0.
// 0x812100 forwards the mount ID to 0x8106c0 for a parent lookup; our presenter does not attach
// this layout into that original UI tree. Parsed root identity is kept unchanged.
const UiBinding kGgeUi{
    "GalaxyGameEntry", "GlobalUIGGE-9", 0x52c07413u, 0x2edd95cau, 0x00de5900u, 0x0136bce0u,
    "GGE default primary UI subset; galaxy scene, zoom panels, events and full driver not implemented"
};

bool same(const char* a, const char* b) {
    if (!a || !b) return false;
    while (*a && *b) {
        if (std::tolower((unsigned char)*a++) != std::tolower((unsigned char)*b++)) return false;
    }
    return *a == *b;
}
}  // namespace

const UiBinding* primary_ui(const char* mode) {
    return same(mode, kGgeUi.mode) ? &kGgeUi : nullptr;
}
}  // namespace os::appmodes
