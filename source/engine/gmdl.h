#pragma once
// gmdl (generated model) mesh reader - OUR C++ port of the solved layout.
//
// The layout was derived from the shipped bytes in tools/gmdl_mesh.py and validated exactly:
// the extracted positions reproduce the file's own header bounding box with err 0.0000 on
// every variant tried (see MEMORY.md, VERIFY row 34). This reader is the same logic in C++ so
// the engine can draw the geometry; it is not a RenderWare reimplementation.
//
// Layout (little-endian):
//   ...variable header of {tag,value} groups...
//   u32 count, float32 bbox[7], u32 submesh_count,
//   per sub-mesh: u32 format(=4), u32 index_count, u32 reserved(=16), u32 index_bytes(=2*count),
//                 u16 indices[index_count]
//   u32 1, u32 N, u32 0, u32 2, u32 0, u32 0x000C0000, (N-1) x 12 declaration bytes,
//   u32 0, u32 vertex_count, u32 vertex_bytes(=vertex_count*36)
//   float32 vertices[vertex_count][9]   ; stride 36 bytes; POSITION = the first float3
//
// The sub-mesh descriptor is NOT at a fixed offset (the header before it varies), so it is
// FOUND by self-consistency, exactly as the Python tool does - not assumed.
#include <string>
#include <vector>

#include "common.h"

namespace os::gmdl {

struct Mesh {
    bool ok = false;
    std::string why;
    float bbox_min[3] = {0, 0, 0};
    float bbox_max[3] = {0, 0, 0};
    u32 submesh_count = 0;
    u32 vertex_count = 0;
    std::vector<float> positions;  // 3 floats per vertex
    std::vector<float> uvs;        // 2 floats per vertex (offset 16 in the 36-byte vertex, N=6)
    std::vector<u16> indices;      // all sub-meshes concatenated (they share one vertex buffer)
};

// Parse a decoded gmdl payload. Never throws; on any inconsistency ok==false and why() says so.
Mesh parse(const u8* data, size_t size);

}  // namespace os::gmdl
