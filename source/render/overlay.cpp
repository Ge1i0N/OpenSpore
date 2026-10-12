#include "render/overlay.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include "engine/diag.h"
#include "engine/log.h"

namespace os::render {
namespace {

Overlay g_overlay;

constexpr int kFirstChar = 32;
constexpr int kLastChar = 126;
constexpr int kGlyphCount = kLastChar - kFirstChar + 1;  // 95

void set_text_states(IDirect3DDevice9* dev, IDirect3DTexture9* atlas) {
    dev->SetFVF(kFVF_TEX);
    dev->SetTexture(0, atlas);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

void set_flat_states(IDirect3DDevice9* dev) {
    dev->SetFVF(kFVF_FLAT);
    dev->SetTexture(0, nullptr);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_ZENABLE, FALSE);
}

}  // namespace

bool Overlay::init(IDirect3DDevice9* dev) {
    m_dev = dev;
    if (!dev) return false;
    if (!build_atlas(dev)) return false;
    LOG_INFO("overlay", "font atlas ready: cell=%d cols=%d glyphs=%d", m_cell, m_cols, kGlyphCount);
    return true;
}

void Overlay::shutdown() {
    if (m_atlas) {
        m_atlas->Release();
        m_atlas = nullptr;
    }
    m_dev = nullptr;
}

bool Overlay::build_atlas(IDirect3DDevice9* dev) {
    m_cell = 16;
    m_cols = 16;
    const int rows = (kGlyphCount + m_cols - 1) / m_cols;  // 6
    const int W = m_cell * m_cols;                          // 256
    const int H = m_cell * rows;                            // 96

    // Rasterise with GDI into a DIB, upload while the DIB bits are still alive.
    // Ordering matters: the previous revision deleted the DIB before LockRect and then
    // copied from a dangling pointer. Copy first, tear down after.
    HDC screen = GetDC(nullptr);
    HDC mem = CreateCompatibleDC(screen);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;  // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!dib || !bits) {
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
        LOG_ERROR("overlay", "CreateDIBSection failed - no font atlas");
        return false;
    }
    HBITMAP old_bmp = (HBITMAP)SelectObject(mem, dib);
    HFONT font = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    HFONT old_font = (HFONT)SelectObject(mem, font);
    SetBkMode(mem, TRANSPARENT);
    SetTextColor(mem, RGB(255, 255, 255));

    auto* px = (u32*)bits;
    memset(px, 0, (size_t)W * H * 4);
    for (int i = 0; i < kGlyphCount; ++i) {
        int cx = (i % m_cols) * m_cell;
        int cy = (i / m_cols) * m_cell;
        wchar_t ch = (wchar_t)(kFirstChar + i);
        RECT rc{cx, cy, cx + m_cell, cy + m_cell};
        ExtTextOutW(mem, cx + 1, cy + 1, ETO_OPAQUE, &rc, &ch, 1, nullptr);
    }
    // Luminance -> alpha (white RGB, A=lum) so one blend mode serves every tint.
    for (int i = 0; i < W * H; ++i) {
        u32 c = px[i];
        u8 r = (u8)(c & 0xFF), g = (u8)((c >> 8) & 0xFF), b = (u8)((c >> 16) & 0xFF);
        u8 lum = (u8)((r * 77 + g * 151 + b * 28) >> 8);
        px[i] = 0x00FFFFFFu | ((u32)lum << 24);
    }

    IDirect3DTexture9* tex = nullptr;
    // Upload route: a SYSTEMMEM staging texture (always lockable) + the DEVICE's
    // UpdateSurface. Signatures verified against SDK 10.0.26100 shared\d3d9.h:464 -
    //   IDirect3DDevice9::UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint)
    // (there is no texture-level UpdateSurface, and no UpdateSourceRect in this header at
    // all; both were guessed wrong by earlier iterations of this file). This avoids the
    // whole class of dynamic-lock pitfalls: a D3DPOOL_DEFAULT texture without
    // D3DUSAGE_DYNAMIC rejects LockRect with D3DERR_INVALIDCALL (0x8876086C), which is
    // exactly how the first run failed at the font.atlas marker.
    HRESULT hr = dev->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    if (FAILED(hr)) {
        LOG_ERROR("overlay", "CreateTexture(%dx%d) failed 0x%08lx", W, H, (unsigned long)hr);
        tex = nullptr;
    } else {
        IDirect3DTexture9* stage = nullptr;
        hr = dev->CreateTexture(W, H, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &stage, nullptr);
        if (FAILED(hr) || !stage) {
            LOG_ERROR("overlay", "staging CreateTexture failed 0x%08lx", (unsigned long)hr);
        } else {
            D3DLOCKED_RECT lr{};
            hr = stage->LockRect(0, &lr, nullptr, 0);
            if (SUCCEEDED(hr)) {
                for (int y = 0; y < H; ++y) {
                    memcpy((u8*)lr.pBits + (size_t)y * lr.Pitch, (u8*)bits + (size_t)y * W * 4, (size_t)W * 4);
                }
                stage->UnlockRect(0);
                IDirect3DSurface9* src = nullptr;
                IDirect3DSurface9* dst = nullptr;
                if (SUCCEEDED(stage->GetSurfaceLevel(0, &src)) && SUCCEEDED(tex->GetSurfaceLevel(0, &dst))) {
                    hr = dev->UpdateSurface(src, nullptr, dst, nullptr);
                    if (FAILED(hr)) LOG_ERROR("overlay", "device UpdateSurface failed 0x%08lx", (unsigned long)hr);
                } else {
                    LOG_ERROR("overlay", "GetSurfaceLevel failed for the atlas upload");
                    hr = E_FAIL;
                }
                if (src) src->Release();
                if (dst) dst->Release();
            } else {
                LOG_ERROR("overlay", "staging LockRect failed 0x%08lx", (unsigned long)hr);
            }
            stage->Release();
        }
        if (FAILED(hr)) {
            tex->Release();
            tex = nullptr;
        }
    }

    SelectObject(mem, old_font);
    DeleteObject(font);
    SelectObject(mem, old_bmp);
    DeleteObject(dib);
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    if (!tex) return false;
    m_atlas = tex;
    return true;
}

void Overlay::begin(int screen_w, int screen_h) {
    if (!m_dev || !m_atlas) return;
    m_screen_w = screen_w;
    m_screen_h = screen_h;
    m_vcount = 0;
    m_flat_drawn = false;
}

void Overlay::flush_text() {
    if (m_vcount == 0) return;
    set_text_states(m_dev, m_atlas);
    for (int i = 0; i + 3 < m_vcount; i += 4) {
        m_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, &m_verts[i], sizeof(Vert));
    }
    m_vcount = 0;
    m_flat_drawn = false;
}

void Overlay::end() {
    if (!m_dev || !m_atlas) return;
    flush_text();
}

void Overlay::rect(float x, float y, float w, float h, u32 argb) {
    if (!m_dev || !m_atlas) return;
    flush_text();  // keep each draw batch homogeneous in FVF/state
    set_flat_states(m_dev);
    struct FlatVert {
        FLOAT x, y, z, rhw;
        DWORD color;
    };
    FlatVert v[4] = {
        {x, y, 0, 1, argb}, {x, y + h, 0, 1, argb}, {x + w, y, 0, 1, argb}, {x + w, y + h, 0, 1, argb},
    };
    m_dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(FlatVert));
    m_flat_drawn = true;
}

void Overlay::text(float x, float y, u32 argb, const std::string& s) {
    if (!m_dev || !m_atlas) return;
    const int cell = m_cell;
    const int rows = (kGlyphCount + m_cols - 1) / m_cols;
    const float aw = (float)(cell * m_cols);
    const float ah = (float)(cell * rows);
    float fx = x;
    for (unsigned char ch : s) {
        if (ch < kFirstChar || ch > kLastChar) {
            fx += cell * 0.5f;
            continue;
        }
        if (m_vcount + 4 > (int)(sizeof(m_verts) / sizeof(m_verts[0]))) flush_text();
        int gi = ch - kFirstChar;
        int gx = (gi % m_cols) * cell;
        int gy = (gi / m_cols) * cell;
        float u0 = gx / aw, v0 = gy / ah, u1 = (gx + cell) / aw, v1 = (gy + cell) / ah;
        Vert* v = &m_verts[m_vcount];
        m_vcount += 4;
        v[0] = {fx, y, 0, 1, argb, u0, v0};
        v[1] = {fx, y + cell, 0, 1, argb, u0, v1};
        v[2] = {fx + cell, y, 0, 1, argb, u1, v0};
        v[3] = {fx + cell, y + cell, 0, 1, argb, u1, v1};
        fx += cell * 0.6f;  // Consolas advance at this size
    }
}

void Overlay::text_fmt(float x, float y, u32 argb, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    text(x, y, argb, buf);
}

float Overlay::text_width(const std::string& s) const { return (float)s.size() * m_cell * 0.6f; }

Overlay& overlay() { return g_overlay; }

}  // namespace os::render
