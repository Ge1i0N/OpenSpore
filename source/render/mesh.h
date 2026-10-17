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
    Mesh() = default;
    // Owns D3D buffers: copy would double-free, so it is move-only.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& o) noexcept { adopt(o); }
    Mesh& operator=(Mesh&& o) noexcept {
        if (this != &o) { shutdown(); adopt(o); }
        return *this;
    }
    ~Mesh();
    void shutdown();

    // Positions are 3 floats/vertex, uvs are 2 floats/vertex (may be null), indices are u16 into
    // that array (shared across sub-meshes, as gmdl stores them). Returns false and logs on any
    // failure; never asserts.
    bool create(IDirect3DDevice9* dev, const float* positions, const float* uvs, u32 vertex_count,
                const u16* indices, u32 index_count);

    // Draw inside `viewport`, spinning the model about the world Z axis (Spore models are Z-up).
    // `angle` is in radians. If `tex` is non-null it is applied (UVs are used); otherwise the mesh
    // is drawn flat-shaded. Saves and restores the device viewport.
    void draw(IDirect3DDevice9* dev, int vx, int vy, int vw, int vh, float angle,
              IDirect3DTexture9* tex = nullptr);

    // --- Top-down scene drawing (the cell stage) ------------------------------------------
    // Configure WORLD/VIEW/PROJECTION and the viewport for a top-down orthographic camera
    // looking down -Z at a Z-up world: world point (camx, camy) is the screen centre and one
    // world unit spans `px_per_world` pixels. Call once per frame before draw_at().
    static void set_topdown_camera(IDirect3DDevice9* dev, float camx, float camy,
                                   float px_per_world, int view_w, int view_h);

    // Draw this mesh into that scene: centred at world (px, py), rotated about the world Z axis
    // by `angle` radians, scaled so its model radius becomes `scale` world units, tinted by
    // `tint` (0xAARRGGBB). Depth-tests against the frame's Z buffer (cleared by Device::clear).
    void draw_at(IDirect3DDevice9* dev, float px, float py, float scale, float angle, D3DCOLOR tint,
                 IDirect3DTexture9* tex = nullptr);

    u32 vertex_count() const { return m_vertex_count; }
    u32 index_count() const { return m_index_count; }
    u32 triangle_count() const { return m_index_count / 3; }
    const float* center() const { return m_center; }
    float radius() const { return m_radius; }
    bool ready() const { return m_vb != nullptr && m_ib != nullptr; }

private:
    void bind_and_draw(IDirect3DDevice9* dev, D3DCOLOR tint, IDirect3DTexture9* tex);
    // Steal another mesh's buffers (used by the move ctor/assign). Leaves `o` empty.
    void adopt(Mesh& o);

    IDirect3DVertexBuffer9* m_vb = nullptr;
    IDirect3DIndexBuffer9* m_ib = nullptr;
    u32 m_vertex_count = 0, m_index_count = 0;
    float m_center[3] = {0, 0, 0};
    float m_radius = 1.f;
};

}  // namespace os::render
