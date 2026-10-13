#include "render/mesh.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "engine/log.h"

namespace os::render {
namespace {

struct Mat4 {
    float m[16];
};

Mat4 identity() {
    Mat4 r{};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f;
    return r;
}

// D3D9 convention: row-major matrices, row vectors (v * M). These are the standard D3DX
// formulas written out by hand (no D3DX available).
Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0.f;
            for (int k = 0; k < 4; ++k) s += a.m[i * 4 + k] * b.m[k * 4 + j];
            r.m[i * 4 + j] = s;
        }
    return r;
}

Mat4 rotation_z(float a) {
    Mat4 r = identity();
    float c = std::cos(a), s = std::sin(a);
    r.m[0] = c; r.m[1] = s;
    r.m[4] = -s; r.m[5] = c;
    return r;
}

Mat4 look_at_lh(const float eye[3], const float at[3], const float up[3]) {
    float z[3] = {at[0] - eye[0], at[1] - eye[1], at[2] - eye[2]};
    float zl = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (zl < 1e-9f) zl = 1.f;
    z[0] /= zl; z[1] /= zl; z[2] /= zl;
    float x[3] = {up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]};
    float xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (xl < 1e-9f) xl = 1.f;
    x[0] /= xl; x[1] /= xl; x[2] /= xl;
    float y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    Mat4 r = identity();
    r.m[0] = x[0]; r.m[1] = y[0]; r.m[2] = z[0];
    r.m[4] = x[1]; r.m[5] = y[1]; r.m[6] = z[1];
    r.m[8] = x[2]; r.m[9] = y[2]; r.m[10] = z[2];
    r.m[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
    r.m[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
    r.m[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
    return r;
}

Mat4 perspective_fov_lh(float fov_y, float aspect, float zn, float zf) {
    float h = 1.f / std::tan(fov_y * 0.5f);
    float w = h / aspect;
    Mat4 r{};
    r.m[0] = w;
    r.m[5] = h;
    r.m[10] = zf / (zf - zn);
    r.m[11] = 1.f;
    r.m[14] = -zn * zf / (zf - zn);
    return r;
}

struct Vtx {
    float x, y, z;
    float nx, ny, nz;
};
constexpr DWORD kFVF = D3DFVF_XYZ | D3DFVF_NORMAL;

}  // namespace

Mesh::~Mesh() { shutdown(); }

void Mesh::shutdown() {
    if (m_ib) { m_ib->Release(); m_ib = nullptr; }
    if (m_vb) { m_vb->Release(); m_vb = nullptr; }
    m_vertex_count = m_index_count = 0;
}

bool Mesh::create(IDirect3DDevice9* dev, const float* positions, u32 vertex_count, const u16* indices,
                  u32 index_count) {
    shutdown();
    if (!dev || !positions || !indices || vertex_count == 0 || index_count < 3) {
        LOG_WARN("mesh", "create: bad input (vc=%u ic=%u)", vertex_count, index_count);
        return false;
    }
    if (index_count % 3 != 0) index_count -= index_count % 3;

    // center + radius (for the camera), then smooth normals from the triangles
    double cx = 0, cy = 0, cz = 0;
    for (u32 i = 0; i < vertex_count; ++i) {
        cx += positions[i * 3 + 0];
        cy += positions[i * 3 + 1];
        cz += positions[i * 3 + 2];
    }
    cx /= vertex_count; cy /= vertex_count; cz /= vertex_count;
    m_center[0] = (float)cx; m_center[1] = (float)cy; m_center[2] = (float)cz;
    m_radius = 0.f;
    for (u32 i = 0; i < vertex_count; ++i) {
        float dx = positions[i * 3 + 0] - m_center[0];
        float dy = positions[i * 3 + 1] - m_center[1];
        float dz = positions[i * 3 + 2] - m_center[2];
        float d = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (d > m_radius) m_radius = d;
    }
    if (m_radius < 1e-6f) m_radius = 1.f;

    std::vector<float> normals((size_t)vertex_count * 3, 0.f);
    for (u32 t = 0; t + 2 < index_count; t += 3) {
        u32 a = indices[t], b = indices[t + 1], c = indices[t + 2];
        if (a >= vertex_count || b >= vertex_count || c >= vertex_count) continue;
        const float* pa = &positions[a * 3];
        const float* pb = &positions[b * 3];
        const float* pc = &positions[c * 3];
        float e1[3] = {pb[0] - pa[0], pb[1] - pa[1], pb[2] - pa[2]};
        float e2[3] = {pc[0] - pa[0], pc[1] - pa[1], pc[2] - pa[2]};
        float n[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                      e1[0] * e2[1] - e1[1] * e2[0]};
        for (u32 v : {a, b, c}) {
            normals[v * 3 + 0] += n[0];
            normals[v * 3 + 1] += n[1];
            normals[v * 3 + 2] += n[2];
        }
    }

    std::vector<Vtx> verts(vertex_count);
    for (u32 i = 0; i < vertex_count; ++i) {
        float nx = normals[i * 3 + 0], ny = normals[i * 3 + 1], nz = normals[i * 3 + 2];
        float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (nl < 1e-9f) { nx = 0; ny = 0; nz = 1; } else { nx /= nl; ny /= nl; nz /= nl; }
        verts[i] = {positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2], nx, ny, nz};
    }

    HRESULT hr = dev->CreateVertexBuffer((UINT)(verts.size() * sizeof(Vtx)), D3DUSAGE_WRITEONLY, kFVF,
                                         D3DPOOL_MANAGED, &m_vb, nullptr);
    if (FAILED(hr) || !m_vb) {
        LOG_ERROR("mesh", "CreateVertexBuffer failed 0x%08lx", (unsigned long)hr);
        shutdown();
        return false;
    }
    void* p = nullptr;
    if (FAILED(m_vb->Lock(0, 0, &p, 0)) || !p) {
        LOG_ERROR("mesh", "vertex buffer lock failed");
        shutdown();
        return false;
    }
    std::memcpy(p, verts.data(), verts.size() * sizeof(Vtx));
    m_vb->Unlock();

    hr = dev->CreateIndexBuffer((UINT)(index_count * sizeof(u16)), D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
                                D3DPOOL_MANAGED, &m_ib, nullptr);
    if (FAILED(hr) || !m_ib) {
        LOG_ERROR("mesh", "CreateIndexBuffer failed 0x%08lx", (unsigned long)hr);
        shutdown();
        return false;
    }
    if (FAILED(m_ib->Lock(0, 0, &p, 0)) || !p) {
        LOG_ERROR("mesh", "index buffer lock failed");
        shutdown();
        return false;
    }
    std::memcpy(p, indices, (size_t)index_count * sizeof(u16));
    m_ib->Unlock();

    m_vertex_count = vertex_count;
    m_index_count = index_count;
    LOG_INFO("mesh", "uploaded %u vertices, %u indices (%u tris), radius %.4f", m_vertex_count,
             m_index_count, m_index_count / 3, m_radius);
    return true;
}

void Mesh::draw(IDirect3DDevice9* dev, int vx, int vy, int vw, int vh, float angle) {
    if (!dev || !ready() || vw < 8 || vh < 8) return;

    D3DVIEWPORT9 saved{};
    dev->GetViewport(&saved);
    D3DVIEWPORT9 vp{};
    vp.X = (DWORD)vx; vp.Y = (DWORD)vy; vp.Width = (DWORD)vw; vp.Height = (DWORD)vh;
    vp.MinZ = 0.f; vp.MaxZ = 1.f;
    dev->SetViewport(&vp);

    // Clear only this viewport's depth so the mesh is not occluded by earlier draws.
    dev->Clear(0, nullptr, D3DCLEAR_ZBUFFER, 0, 1.f, 0);

    const float dist = m_radius * 2.7f;
    float eye[3] = {m_center[0] + dist * 0.62f, m_center[1] - dist * 0.80f, m_center[2] + dist * 0.45f};
    float at[3] = {m_center[0], m_center[1], m_center[2]};
    float up[3] = {0.f, 0.f, 1.f};  // Spore models are Z-up
    Mat4 world = rotation_z(angle);
    Mat4 view = look_at_lh(eye, at, up);
    Mat4 proj = perspective_fov_lh(0.9f, (float)vw / (float)vh, m_radius * 0.02f, m_radius * 40.f);
    dev->SetTransform(D3DTS_WORLD, (const D3DMATRIX*)&world);
    dev->SetTransform(D3DTS_VIEW, (const D3DMATRIX*)&view);
    dev->SetTransform(D3DTS_PROJECTION, (const D3DMATRIX*)&proj);

    dev->SetRenderState(D3DRS_LIGHTING, TRUE);
    dev->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(38, 40, 46));
    dev->SetRenderState(D3DRS_ZENABLE, TRUE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);  // winding is not guaranteed by the format
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);

    D3DMATERIAL9 mat{};
    mat.Diffuse.r = mat.Diffuse.g = mat.Diffuse.b = mat.Diffuse.a = 1.f;
    mat.Ambient.r = mat.Ambient.g = mat.Ambient.b = mat.Ambient.a = 0.55f;
    dev->SetMaterial(&mat);

    D3DLIGHT9 lt{};
    lt.Type = D3DLIGHT_DIRECTIONAL;
    lt.Diffuse.r = 1.f; lt.Diffuse.g = 0.98f; lt.Diffuse.b = 0.92f; lt.Diffuse.a = 1.f;
    lt.Direction = D3DVECTOR{-0.35f, 0.55f, -0.75f};
    dev->SetLight(0, &lt);
    dev->LightEnable(0, TRUE);

    dev->SetStreamSource(0, m_vb, 0, sizeof(Vtx));
    dev->SetIndices(m_ib);
    dev->SetFVF(kFVF);
    // The FVF has no texture coordinates; make sure a texture left bound by the overlay cannot
    // tint the mesh (default COLOROP is MODULATE with the texture).
    dev->SetTexture(0, nullptr);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    dev->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, 0, 0, m_vertex_count, 0, m_index_count / 3);
    dev->LightEnable(0, FALSE);

    dev->SetViewport(&saved);
}

}  // namespace os::render
