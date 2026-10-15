#pragma once
#include "engine/spui.h"

namespace os::spui {

struct ImageQuad {
    u16 external = 0xffff;
    Rect area;
    Rect uv;
    std::shared_ptr<const ImagePixels> pixels;
};

// GA subset: plain ImageDrawable (tiling None) and non-tileable standard/radio buttons.
// State is the drawable state, not Window flags: 0 normal, 1 disabled, 2 hover, 3 active,
// plus bit 3 selected. Returns false for missing/unsupported data; output stays unchanged.
bool drawable_quad(const Layout& layout, u16 drawable, const Rect& area, u32 state, ImageQuad& out);
// Explicit ImageDrawable class only (CF3C4EAE), GA QI EF3C47CF / getter +0x18. An override
// can supply its absent image link, but cannot fabricate the drawable/interface itself.
bool image_drawable_binding(const Layout& layout,u16 drawable,ImageBinding& out);
bool valid_image_binding(const ImageBinding& image); // bounded pixels + UV/logical geometry

}  // namespace os::spui
