#pragma once
// Immediate-mode 2D overlay: quads + text, drawn through the D3D9 device.
//
// This is OUR scaffolding layer (loader screen, FPS, console echo), not a RenderWare
// reimplementation. It deliberately avoids RW until the load chain needs it (field guide
// §3 stays on the roadmap; see VERIFY.md).
//
// Font: a GDI-rasterised ASCII atlas (32..126) uploaded as a D3D9 texture. Chosen over
// embedding font bytes because it is deterministic, needs no asset files, and the
// original's own text stack (UTFWin/IMM32/USP10) is out of scope for the boot scaffold.
// The field guide's warning about text was about *replacing the game's* renderer with
// guessed states; here we log the actual state at draw time (atlas size, cell, count).
#include <d3d9.h>

#include <string>

#include "common.h"

namespace os::render {

constexpr u32 rgba(u8 r, u8 g, u8 b, u8 a = 255) { return ((u32)a << 24) | ((u32)r << 16) | ((u32)g << 8) | b; }

// Textured and flat (rectangle) vertices share the first 20 bytes; two FVFs, one buffer.
struct Vert {
    FLOAT x, y, z, rhw;
    DWORD color;
    FLOAT u, v;
};
constexpr DWORD kFVF_TEX = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
constexpr DWORD kFVF_FLAT = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

class Overlay {
public:
    bool init(IDirect3DDevice9* dev);
    void shutdown();

    void begin(int screen_w, int screen_h);
    void end();

    void rect(float x, float y, float w, float h, u32 argb);
    void text(float x, float y, u32 argb, const std::string& s);
    void text_fmt(float x, float y, u32 argb, const char* fmt, ...);
    float text_width(const std::string& s) const;

    int font_height() const { return m_cell_h; }

private:
    bool build_atlas(IDirect3DDevice9* dev);
    void flush_text();

    IDirect3DDevice9* m_dev = nullptr;
    IDirect3DTexture9* m_atlas = nullptr;
    int m_cell_w = 8;   // glyph cell width, px  (== the advance; monospace, so one value serves both)
    int m_cell_h = 16;  // glyph cell height, px
    int m_cols = 16;    // atlas grid width in cells
    int m_advance = 8;  // horizontal pen step, px
    int m_screen_w = 0, m_screen_h = 0;
    Vert m_verts[4096];
    int m_vcount = 0;
    bool m_flat_drawn = false;  // a rect() reset the states; text must re-arm
};

Overlay& overlay();

}  // namespace os::render
