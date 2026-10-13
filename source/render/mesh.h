#pragma once
// Minimal indexed 3D mesh on the D3D9 device - the first real GEOMETRY our renderer draws.
//
// This is deliberately small: it takes solved gmdl positions + indices, computes smooth
// normals, uploads a managed vertex/index buffer and draws it with a fixed-function orbit
// camera. It is not the RenderWare bridge (field guide §3 stays on the roadmap); it exists so
// the mesh decoder's output is visible on OUR screen, exactly as the decoded texture already is.
//
// No D3DX: the June 2010 DX SDK is not installed here (see CMakeLists), so the matrices are
// built by hand in mesh.cpp.
#include <d3d9.h>

#include "common.h"

namespace os::render {

class Mesh {
public:
    ~Mesh();
    void shutdown();

    // Positions are 3 floats/vertex; indices are u16 into that array (shared across sub-meshes,
    // as gmdl stores them). Returns false and logs on any failure; never asserts.
    bool create(IDirect3DDevice9* dev, const float* positions, u32 vertex_count, const u16* indices,
                u32 index_count);

    // Draw inside `viewport`, spinning the model about the world Z axis (Spore models are Z-up).
    // `angle` is in radians. Saves and restores the device viewport.
    void draw(IDirect3DDevice9* dev, int vx, int vy, int vw, int vh, float angle);

    u32 vertex_count() const { return m_vertex_count; }
    u32 index_count() const { return m_index_count; }
    u32 triangle_count() const { return m_index_count / 3; }
    const float* center() const { return m_center; }
    float radius() const { return m_radius; }
    bool ready() const { return m_vb != nullptr && m_ib != nullptr; }

private:
    IDirect3DVertexBuffer9* m_vb = nullptr;
    IDirect3DIndexBuffer9* m_ib = nullptr;
    u32 m_vertex_count = 0, m_index_count = 0;
    float m_center[3] = {0, 0, 0};
    float m_radius = 1.f;
};

}  // namespace os::render
