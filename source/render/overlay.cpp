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
    // POINT filtering: LINEAR bleeds neighbouring atlas cells into each other at these
    // small cell sizes (7x15), which would smear the glyphs back into bars.
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
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
    LOG_INFO("overlay", "font atlas ready: cell=%dx%d advance=%d cols=%d glyphs=%d", m_cell_w, m_cell_h, m_advance,
             m_cols, kGlyphCount);
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
    // Cell geometry comes from the FONT, not from a round number. The first version used a
    // square 16x16 cell with a 0.6*cell advance, so every glyph overlapped its neighbour by
    // ~6 px and the loader text rendered as solid white bars (visible in the first captured
    // frame). GetTextMetricW gives the true average advance; we make cell width == advance
    // so the pen step can never cause overlap for a monospace face.
    m_cell_h = 16;
    m_cols = 16;
    {
        HDC s = GetDC(nullptr);
        HDC m = CreateCompatibleDC(s);
        HFONT f = CreateFontW(-13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, PROOF_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
        HFONT of = (HFONT)SelectObject(m, f);
        TEXTMETRICW tm{};
        // GetTextMetricsW (plural) is the real API; `GetTextMetricW` does not exist and the
        // first revision of this function failed to compile on exactly that typo.
        if (GetTextMetricsW(m, &tm)) {
            m_advance = tm.tmAveCharWidth > 0 ? tm.tmAveCharWidth : 8;
            if (tm.tmHeight > 0) m_cell_h = tm.tmHeight;
        } else {
            m_advance = 8;
        }
        SelectObject(m, of);
        DeleteObject(f);
        DeleteDC(m);
        ReleaseDC(nullptr, s);
    }
    m_cell_w = m_advance;
    const int rows = (kGlyphCount + m_cols - 1) / m_cols;  // 6
    const int W = m_cell_w * m_cols;
    const int H = m_cell_h * rows;

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
    SetBkMode(mem, OPAQUE);
    // ETO_OPAQUE fills the rect with the *background* colour before drawing the glyph.
    // Leaving the default (white) made every cell solid after the luminance->alpha
    // conversion below, so the loader text rendered as solid white bars in the captured
    // frame. Black background + white foreground is what makes alpha mean ink.
    SetBkColor(mem, RGB(0, 0, 0));
    SetTextColor(mem, RGB(255, 255, 255));

    auto* px = (u32*)bits;
    memset(px, 0, (size_t)W * H * 4);
    for (int i = 0; i < kGlyphCount; ++i) {
        int cx = (i % m_cols) * m_cell_w;
        int cy = (i / m_cols) * m_cell_h;
        wchar_t ch = (wchar_t)(kFirstChar + i);
        RECT rc{cx, cy, cx + m_cell_w, cy + m_cell_h};
        // No ETO_CLAMPED: it is absent from SDK 10.0.26100 (grep found 0 hits), and it is
        // unnecessary here - the cell width equals the font's average advance, and for a
        // FIXED_PITCH face the ink of every glyph fits inside one advance.
        ExtTextOutW(mem, cx, cy, ETO_OPAQUE, &rc, &ch, 1, nullptr);
    }
    // Luminance -> alpha (white RGB, A=lum) so one blend mode serves every tint.
    u32 ink = 0;
    for (int i = 0; i < W * H; ++i) {
        u32 c = px[i];
        u8 r = (u8)(c & 0xFF), g = (u8)((c >> 8) & 0xFF), b = (u8)((c >> 16) & 0xFF);
        u8 lum = (u8)((r * 77 + g * 151 + b * 28) >> 8);
        px[i] = 0x00FFFFFFu | ((u32)lum << 24);
        if (lum) ++ink;
    }
    // Ink coverage is the atlas's own health check: a solid-background bug shows up here as
    // ~100% before it ever reaches a screenshot, and an empty atlas shows up as 0%.
    LOG_INFO("overlay", "atlas %dx%d ink=%u/%u (%.1f%%)", W, H, ink, (u32)(W * H), 100.0 * ink / (double)(W * H));
    if (ink == 0 || ink > 90 * (u32)(W * H) / 100) {
        LOG_ERROR("overlay", "atlas ink coverage %.1f%% is implausible - glyphs are %s",
                  100.0 * ink / (double)(W * H), ink == 0 ? "missing" : "solid (background not black?)");
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
    // ONE DrawPrimitiveUP for the whole batch. The previous version issued one call per
    // quad; when the loader started drawing the 57-line package list that became ~3400
    // draw calls per frame and took the app from 30 fps to 4. Vertices are emitted as a
    // triangle list (6 per glyph) precisely so they can go out in a single call.
    m_dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)m_vcount / 3, m_verts, sizeof(Vert));
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
    const int rows = (kGlyphCount + m_cols - 1) / m_cols;
    const float aw = (float)(m_cell_w * m_cols);
    const float ah = (float)(m_cell_h * rows);
    float fx = x;
    for (unsigned char ch : s) {
        if (ch < kFirstChar || ch > kLastChar) {
            fx += m_advance * 0.5f;
            continue;
        }
        if (m_vcount + 6 > (int)(sizeof(m_verts) / sizeof(m_verts[0]))) flush_text();
        int gi = ch - kFirstChar;
        int gx = (gi % m_cols) * m_cell_w;
        int gy = (gi / m_cols) * m_cell_h;
        float u0 = gx / aw, v0 = gy / ah, u1 = (gx + m_cell_w) / aw, v1 = (gy + m_cell_h) / ah;
        float x0 = fx, x1 = fx + (float)m_cell_w, y0 = y, y1 = y + (float)m_cell_h;
        // 6 verts (two triangles), so the whole string can go out in ONE draw call.
        Vert* v = &m_verts[m_vcount];
        m_vcount += 6;
        v[0] = {x0, y0, 0, 1, argb, u0, v0};
        v[1] = {x0, y1, 0, 1, argb, u0, v1};
        v[2] = {x1, y0, 0, 1, argb, u1, v0};
        v[3] = {x1, y0, 0, 1, argb, u1, v0};
        v[4] = {x0, y1, 0, 1, argb, u0, v1};
        v[5] = {x1, y1, 0, 1, argb, u1, v1};
        fx += (float)m_advance;  // cell width == advance, so overlap is impossible
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

float Overlay::text_width(const std::string& s) const { return (float)s.size() * (float)m_advance; }

Overlay& overlay() { return g_overlay; }

}  // namespace os::render
