#pragma once
#include "engine/spui.h"

namespace os::spui {

struct ImageQuad {
    u16 external = 0xffff;
    Rect area;
    Rect uv;
};

// GA subset: plain ImageDrawable (tiling None) and non-tileable standard/radio buttons.
// State is the drawable state, not Window flags: 0 normal, 1 disabled, 2 hover, 3 active,
// plus bit 3 selected. Returns false for missing/unsupported data; output stays unchanged.
bool drawable_quad(const Layout& layout, u16 drawable, const Rect& area, u32 state, ImageQuad& out);

}  // namespace os::spui
