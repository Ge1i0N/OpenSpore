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

struct AnimationDescriptor {
    ImageBinding image;
    i32 frames=0,rows=1,columns=0;
};
// Static AutoPlay=false subset with explicit positive frame/grid counts and obsolete width=0.
// Auto playback / image-dependent native grid normalization remain unsupported, not guessed.
// Constructor frame1 is not an observation of native post-load frame (normalization resets to0).
bool animation_descriptor(const Layout& layout,u16 window,AnimationDescriptor& out,std::string& error);
bool animation_quad(const Layout& layout,u16 window,const Rect& area,ImageQuad& out);

}  // namespace os::spui
