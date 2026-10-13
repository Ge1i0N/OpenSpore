#pragma once
// raster (RenderWare texture) decoder - OUR C++ port of the solved format.
//
// Derived from the shipped bytes (tools/raster_scan.py over 1,200 records, confirmed by a correct
// decode in tools/raster_decode.py):
//     +0x00 u32 = 1            version
//     +0x04 u32 = dim          (h or w; every sampled texture is square)
//     +0x08 u32 = dim          (the other dimension)
//     +0x0c u32 = code         (mip/quality code; meaning open)
//     +0x10 u32 = 8            constant in every sample
//     +0x14 char[4]            format tag: "DXT5", "DXT1", or u32 0x15 for raw BGRA
//     +0x18 u32 = base bytes   (w*h for DXT5, w*h/2 for DXT1, w*h*4 for raw)
//     +0x1c ...                base-level pixels, then mip levels
//
// Only the BASE level is decoded (enough to draw). Output is tightly packed top-down BGRA, exactly
// what render::Device::create_texture_bgra wants. This is not the RenderWare bridge.
#include <string>
#include <vector>

#include "common.h"

namespace os::raster {

struct Texture {
    bool ok = false;
    std::string why;
    int width = 0, height = 0;
    std::string format;       // "DXT1" | "DXT5" | "raw"
    std::vector<u8> bgra;     // width*height*4, top-down
};

// Decode a raster payload (already RefPack-decompressed). Never throws.
Texture decode(const u8* data, size_t size);

}  // namespace os::raster
