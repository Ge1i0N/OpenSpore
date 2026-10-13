#include "engine/gmdl.h"

#include <cmath>
#include <cstring>

namespace os::gmdl {
namespace {

constexpr u32 kFormatCode = 4;
constexpr u32 kReserved = 16;
constexpr u32 kVertexStride = 36;
constexpr size_t kBboxBytes = 0x1C;  // 7 floats

bool read_u32(const u8* d, size_t n, size_t off, u32& out) {
    if (off + 4 > n) return false;
    std::memcpy(&out, d + off, 4);
    return true;
}
bool read_f32(const u8* d, size_t n, size_t off, float& out) {
    if (off + 4 > n) return false;
    std::memcpy(&out, d + off, 4);
    return true;
}

// A descriptor is self-consistent when format==4, reserved==16, count!=0, index_bytes==2*count
// and the index array fits. This is the same predicate tools/gmdl_mesh.py uses.
bool descriptor_at(const u8* d, size_t n, size_t off, u32& count, u32& index_bytes) {
    u32 fmt, res, ib;
    if (!read_u32(d, n, off + 0, fmt)) return false;
    if (!read_u32(d, n, off + 4, count)) return false;
    if (!read_u32(d, n, off + 8, res)) return false;
    if (!read_u32(d, n, off + 12, ib)) return false;
    if (fmt != kFormatCode || res != kReserved || count == 0) return false;
    if (ib != count * 2u) return false;
    if (off + 16 + ib > n) return false;
    index_bytes = ib;
    return true;
}

}  // namespace

Mesh parse(const u8* data, size_t size) {
    Mesh m;
    if (!data || size < 0x40) {
        m.why = "payload too short";
        return m;
    }
    // 1. find the first self-consistent sub-mesh descriptor
    size_t d0 = 0;
    bool found = false;
    for (size_t off = 0; off + 16 <= size; off += 4) {
        u32 c, ib;
        if (descriptor_at(data, size, off, c, ib)) {
            d0 = off;
            found = true;
            break;
        }
    }
    if (!found) {
        m.why = "no self-consistent sub-mesh descriptor";
        return m;
    }
    if (d0 < 0x20) {
        m.why = "descriptor too close to the start";
        return m;
    }

    // 2. bbox and submesh count sit immediately before it
    float b[7];
    for (int i = 0; i < 7; ++i) {
        if (!read_f32(data, size, d0 - 0x20 + (size_t)i * 4, b[i])) {
            m.why = "bbox read out of range";
            return m;
        }
    }
    std::memcpy(m.bbox_min, b, sizeof(m.bbox_min));
    std::memcpy(m.bbox_max, b + 3, sizeof(m.bbox_max));
    read_u32(data, size, d0 - 4, m.submesh_count);

    // 3. walk the sub-meshes, concatenating their indices
    size_t d = d0;
    for (;;) {
        u32 count = 0, ib = 0;
        if (!descriptor_at(data, size, d, count, ib)) break;
        const size_t base = d + 16;
        for (u32 i = 0; i < count; ++i) {
            u16 v = 0;
            std::memcpy(&v, data + base + (size_t)i * 2, 2);
            m.indices.push_back(v);
        }
        d = base + ib;
    }
    if (m.indices.empty()) {
        m.why = "no indices";
        return m;
    }

    // 4. vertex-block header: decl count N at +4, then vertex_count/vertex_bytes
    u32 decl_n = 0;
    if (!read_u32(data, size, d + 4, decl_n) || decl_n == 0) {
        m.why = "missing vertex declaration count";
        return m;
    }
    const size_t vc_off = 0x1C + (size_t)(decl_n - 1) * 12;
    u32 vcount = 0, vbytes = 0;
    if (!read_u32(data, size, d + vc_off, vcount) || !read_u32(data, size, d + vc_off + 4, vbytes)) {
        m.why = "missing vertex count";
        return m;
    }
    if (vcount == 0) {
        m.why = "zero vertices";
        return m;
    }
    if (vbytes != vcount * kVertexStride) {
        // Not fatal for drawing (we trust the count), but worth recording.
        m.why = "vertex_bytes != vertex_count*36";
    }
    const size_t vdata = d + vc_off + 8;
    if (vdata + (size_t)vcount * kVertexStride > size) {
        m.why = "vertex block does not fit";
        return m;
    }

    // 5. positions = the first float3 of each 36-byte vertex; UV = 2 floats at offset 16 (N=6)
    m.vertex_count = vcount;
    m.positions.resize((size_t)vcount * 3);
    m.uvs.resize((size_t)vcount * 2);
    for (u32 i = 0; i < vcount; ++i) {
        const size_t off = vdata + (size_t)i * kVertexStride;
        float x, y, z, u, v;
        read_f32(data, size, off + 0, x);
        read_f32(data, size, off + 4, y);
        read_f32(data, size, off + 8, z);
        read_f32(data, size, off + 16, u);
        read_f32(data, size, off + 20, v);
        m.positions[i * 3 + 0] = x;
        m.positions[i * 3 + 1] = y;
        m.positions[i * 3 + 2] = z;
        m.uvs[i * 2 + 0] = u;
        m.uvs[i * 2 + 1] = v;
    }
    // 6. VALIDATE: the extracted positions must reproduce the file's own bbox (the exactness test
    // that solved gmdl). This rejects the gmdl variants whose vertex layout we have not derived, so
    // a bogus mesh is never reported as ok (and the unpacker falls back to raw bytes for those).
    float mn[3] = {m.positions[0], m.positions[1], m.positions[2]};
    float mx[3] = {m.positions[0], m.positions[1], m.positions[2]};
    for (u32 i = 1; i < vcount; ++i) {
        for (int k = 0; k < 3; ++k) {
            float v = m.positions[i * 3 + k];
            if (v < mn[k]) mn[k] = v;
            if (v > mx[k]) mx[k] = v;
        }
    }
    float err = 0.f;
    for (int k = 0; k < 3; ++k) {
        err += std::abs(mn[k] - m.bbox_min[k]) + std::abs(mx[k] - m.bbox_max[k]);
    }
    if (err > 1e-3f) {
        m.ok = false;
        m.why = "positions do not reproduce the header bbox (layout not handled)";
        return m;
    }
    m.ok = true;
    return m;
}

}  // namespace os::gmdl
