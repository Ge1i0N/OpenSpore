#pragma once
// Image loading for decoded game resources.
//
// The unpacker proves the pipeline produces real images: 14,549 `png` resources decode to files
// whose headers are genuine PNG. This module closes the loop by turning those decoded bytes
// into pixels our own renderer can draw, so a real Spore asset appears on screen.
//
// GDI+ is used deliberately: it is part of Windows, needs no third-party dependency, and reads
// PNG from memory (so nothing is written to disk to display it). The output is tightly packed
// 32-bit BGRA in top-down order, which is exactly what a D3D9 A8R8G8B8 texture wants.
#include <cstdint>
#include <vector>

#include "common.h"

namespace os::image {

// One-time GDI+ startup/shutdown. Idempotent.
bool gdiplus_start();
void gdiplus_stop();

// Decode a PNG (or any GDI+ format) from memory into tightly packed BGRA, top-down.
// Returns false on any failure and never throws.
bool decode_from_memory(const u8* data, size_t len, std::vector<u8>& out_bgra, int& out_w, int& out_h);

}  // namespace os::image
