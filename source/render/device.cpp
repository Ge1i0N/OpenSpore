#include "render/device.h"

#include <windows.h>

#include <cstring>

#include "engine/log.h"

#pragma comment(lib, "d3d9.lib")

namespace os::render {
namespace {

Device g_device;

}  // namespace

Device::~Device() { shutdown(); }

void Device::fill_pp(D3DPRESENT_PARAMETERS& pp, int w, int h, bool windowed) const {
    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = windowed ? TRUE : FALSE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = m_hwnd;
    pp.BackBufferFormat = windowed ? D3DFMT_UNKNOWN : m_desktop.Format;
    pp.BackBufferWidth = (UINT)w;
    pp.BackBufferHeight = (UINT)h;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    // VSync only makes sense windowed here; fullscreen interval is decided by the desktop mode.
    pp.PresentationInterval = (m_cfg.vsync && windowed) ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
}

bool Device::init(HWND hwnd, const Config& cfg, std::string& err) {
    m_cfg = cfg;
    m_hwnd = hwnd;

    // 1. The one d3d9 import the original has: Direct3DCreate9 (NOT the Ex entry point).
    m_d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
    if (!m_d3d9) {
        err = "Direct X is unavailable.";  // 0x01413aee
        return false;
    }

    // 2. Desktop mode, same contract as the original's 32-bit requirement.
    ZeroMemory(&m_desktop, sizeof(m_desktop));
    HRESULT hr = m_d3d9->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &m_desktop);
    if (FAILED(hr) || m_desktop.Format != D3DFMT_X8R8G8B8) {
        err = "Please ensure your display is set to 32 bits.";  // 0x014139fe
        LOG_ERROR("render", "GetAdapterDisplayMode hr=0x%08lx fmt=%d (want X8R8G8B8=%d)", (unsigned long)hr,
                  (int)m_desktop.Format, (int)D3DFMT_X8R8G8B8);
        return false;
    }

    // 3. Create the device. The cell stage draws the shipped bodies verbatim - about 3.9M
    //    vertices per frame at level_water - so the vertices go through the GPU whenever it
    //    can: with software vertex processing that same frame took over a second. The device
    //    is created with hardware vertex processing first and falls back to software when the
    //    adapter refuses it (the same pattern as the resolution fallback below).
    m_width = cfg.width;
    m_height = cfg.height;
    fill_pp(m_pp, m_width, m_height, cfg.windowed);
    DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
    hr = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &m_pp, &m_device);
    bool hardware_vp = SUCCEEDED(hr);
    if (FAILED(hr)) {
        LOG_WARN("render", "CreateDevice with hardware vertex processing failed (0x%08lx), "
                 "falling back to software", (unsigned long)hr);
        flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        fill_pp(m_pp, m_width, m_height, cfg.windowed);
        hr = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &m_pp, &m_device);
    }
    if (FAILED(hr) && cfg.windowed) {
        // The original's own fallback resolution: 0x7e74f1 passes 0x320(800) x 0x258(600)
        // to 0x410330 as a (w,h) pair.
        LOG_WARN("render", "CreateDevice %dx%d failed (0x%08lx), falling back to 800x600", m_width, m_height,
                 (unsigned long)hr);
        m_width = 800;
        m_height = 600;
        fill_pp(m_pp, m_width, m_height, true);
        hr = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &m_pp, &m_device);
    }
    if (FAILED(hr)) {
        err = "Could not create the Graphics Device.";  // 0x01413ac8
        LOG_ERROR("render", "CreateDevice hr=0x%08lx after fallback", (unsigned long)hr);
        return false;
    }
    m_present_interval_one = (m_pp.PresentationInterval == D3DPRESENT_INTERVAL_ONE);
    LOG_INFO("render", "device %s %dx%d vsync=%d (desktop %dx%d fmt=%d) flags=%s",
             cfg.windowed ? "windowed" : "fullscreen", m_width, m_height, (int)m_present_interval_one,
             (int)m_desktop.Width, (int)m_desktop.Height, (int)m_desktop.Format,
             hardware_vp ? "HWVP" : "SWVP");
    return true;
}

void Device::shutdown() {
    if (m_device) {
        m_device->Release();
        m_device = nullptr;
    }
    if (m_d3d9) {
        m_d3d9->Release();
        m_d3d9 = nullptr;
    }
}

bool Device::resize(int w, int h) {
    if (!m_device || w <= 0 || h <= 0) return false;
    if (w == m_width && h == m_height) return true;
    D3DPRESENT_PARAMETERS pp = m_pp;
    pp.BackBufferWidth = (UINT)w;
    pp.BackBufferHeight = (UINT)h;
    HRESULT hr = m_device->Reset(&pp);
    if (FAILED(hr)) {
        LOG_ERROR("render", "Reset(%dx%d) failed 0x%08lx", w, h, (unsigned long)hr);
        return false;
    }
    m_pp = pp;
    m_width = w;
    m_height = h;
    LOG_DEBUG("render", "resized to %dx%d", w, h);
    return true;
}

bool Device::begin_frame() {
    if (!m_device) return false;
    HRESULT hr = m_device->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST) {
        m_lost = true;
        Sleep(10);
        return false;
    }
    if (hr == D3DERR_DEVICENOTRESET) {
        m_lost = FAILED(m_device->Reset(&m_pp));
        return false;  // never begin a scene on the frame we spent resetting
    }
    m_lost = false;
    hr = m_device->BeginScene();
    if (FAILED(hr)) {
        LOG_WARN("render", "BeginScene failed 0x%08lx", (unsigned long)hr);
        return false;
    }
    m_in_scene = true;
    return true;
}

void Device::end_frame() {
    if (!m_device) return;
    // Balanced by construction: EndScene/Present only ever run if BeginScene succeeded.
    if (m_in_scene) {
        m_device->EndScene();
        m_in_scene = false;
    }
    // Deferred capture goes HERE, between EndScene and Present. Calling it while the scene
    // is open hangs: GetRenderTargetData on a surface that is the current render target
    // never returns (verified by observation - the capture request logged, cap.bmp was
    // never created, and the process had to be killed). After EndScene the back buffer is
    // complete, and before Present it still holds the frame, so this is the only safe slot.
    if (m_capture_request[0]) {
        char p[MAX_PATH];
        lstrcpynA(p, m_capture_request, MAX_PATH);
        m_capture_request[0] = '\0';
        capture_frame(p);
    }
    if (!m_lost) m_device->Present(nullptr, nullptr, nullptr, nullptr);
}

void Device::clear(u32 argb) {
    if (!m_device || !m_in_scene) return;
    m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, (D3DCOLOR)argb, 1.0f, 0);
}

Device& device() { return g_device; }

IDirect3DTexture9* Device::create_texture_bgra(const u8* bgra, int w, int h) {
    if (!m_device || !bgra || w <= 0 || h <= 0) return nullptr;
    IDirect3DTexture9* tex = nullptr;
    HRESULT hr = m_device->CreateTexture((UINT)w, (UINT)h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr);
    if (FAILED(hr) || !tex) {
        LOG_ERROR("render", "CreateTexture %dx%d failed 0x%08lx", w, h, (unsigned long)hr);
        return nullptr;
    }
    D3DLOCKED_RECT lr{};
    hr = tex->LockRect(0, &lr, nullptr, 0);
    if (FAILED(hr)) {
        LOG_ERROR("render", "texture LockRect failed 0x%08lx", (unsigned long)hr);
        tex->Release();
        return nullptr;
    }
    for (int y = 0; y < h; ++y) {
        memcpy((u8*)lr.pBits + (size_t)y * lr.Pitch, bgra + (size_t)y * w * 4, (size_t)w * 4);
    }
    tex->UnlockRect(0);
    LOG_INFO("render", "uploaded %dx%d BGRA texture (%u bytes)", w, h, (u32)(w * h * 4));
    return tex;
}

void Device::request_capture(const char* path) {
    if (!path || !*path) return;
    lstrcpynA(m_capture_request, path, MAX_PATH);
}

bool Device::capture_frame(const char* path) {
    if (!m_device || !path) return false;
    const UINT w = m_width, h = m_height;
    IDirect3DSurface9* src = nullptr;   // CPU-visible surface we end up reading
    bool owned_by_us = false;           // src was created here (vs. obtained from the device)
    HRESULT hr_front = E_FAIL, hr_back = E_FAIL, hr_grd = E_FAIL;

    // Route 1: GetFrontBufferData. Requires a *persistent* front buffer, which
    // D3DSWAPEFFECT_DISCARD does not give us: with DISCARD the contents are undefined after
    // present and the call fails with D3DERR_INVALIDCALL (0x8876086C) even inside an open
    // scene (verified live: `GetFrontBufferData 0x8876086c (scene_open=1)`). Kept as the
    // first attempt because it is the cheapest and would work if we ever switch swap effect.
    {
        IDirect3DSurface9* f = nullptr;
        hr_front = m_device->CreateOffscreenPlainSurface(w, h, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &f, nullptr);
        if (SUCCEEDED(hr_front) && f) {
            hr_front = m_device->GetFrontBufferData(0, f);
            if (SUCCEEDED(hr_front)) {
                src = f;
                owned_by_us = true;
                f = nullptr;
            }
        }
        if (f) f->Release();
    }

    // Route 2: GetBackBuffer(0,0,MONO) -> GetRenderTargetData into a SYSTEMMEM surface.
    // The destination format must match the source exactly, so it is queried from the
    // surface rather than assumed (in windowed mode we requested D3DFMT_UNKNOWN, which the
    // runtime resolved to the desktop format).
    if (!src) {
        IDirect3DSurface9* back = nullptr;
        hr_back = m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
        if (SUCCEEDED(hr_back) && back) {
            D3DSURFACE_DESC sd{};
            back->GetDesc(&sd);  // IDirect3DSurface9::GetDesc, shared\d3d9.h:1639
                                 // (GetSurfaceDesc does not exist on this interface - the
                                 // first revision of this function guessed it and did not compile)
            IDirect3DSurface9* sys = nullptr;
            hr_back = m_device->CreateOffscreenPlainSurface(sd.Width, sd.Height, sd.Format, D3DPOOL_SYSTEMMEM, &sys,
                                                            nullptr);
            if (SUCCEEDED(hr_back) && sys) {
                hr_grd = m_device->GetRenderTargetData(back, sys);
                if (SUCCEEDED(hr_grd)) {
                    src = sys;
                    owned_by_us = true;
                    sys = nullptr;
                }
            }
            if (sys) sys->Release();
            back->Release();
        }
    }

    if (!src) {
        LOG_ERROR("capture", "both routes failed: front=0x%08lx back=0x%08lx grd=0x%08lx (scene_open=%d swap=DISCARD)",
                  (unsigned long)hr_front, (unsigned long)hr_back, (unsigned long)hr_grd, (int)m_in_scene);
        return false;
    }

    bool ok = false;
    D3DLOCKED_RECT lr{};
    hr_back = src->LockRect(&lr, nullptr, 0);
    if (FAILED(hr_back)) {
        LOG_ERROR("capture", "LockRect 0x%08lx", (unsigned long)hr_back);
        if (owned_by_us) src->Release();
        return false;
    }
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") == 0 && f) {
        // 32bpp BI_RGB, bottom-up, uncompressed. D3D's A8R8G8B8/X8R8G8B8 is already
        // B,G,R,A in memory, which is exactly what a 32-bit BMP row expects.
        const DWORD px = w * h;
        const DWORD img = px * 4;
        const DWORD file_size = 14 + 40 + img;
        u8 hdr[54];
        ZeroMemory(hdr, sizeof(hdr));
        hdr[0] = 'B';
        hdr[1] = 'M';
        memcpy(hdr + 2, &file_size, 4);
        const DWORD off = 54;
        memcpy(hdr + 10, &off, 4);
        const DWORD hs = 40;
        memcpy(hdr + 14, &hs, 4);
        memcpy(hdr + 18, &w, 4);
        memcpy(hdr + 22, &h, 4);
        hdr[26] = 1;
        hdr[28] = 32;
        memcpy(hdr + 34, &img, 4);
        const DWORD ppm = 2835;  // 72 dpi
        memcpy(hdr + 38, &ppm, 4);
        memcpy(hdr + 42, &ppm, 4);
        fwrite(hdr, 1, 54, f);
        for (int y = (int)h - 1; y >= 0; --y) {
            fwrite((const u8*)lr.pBits + (size_t)y * lr.Pitch, 1, (size_t)w * 4, f);
        }
        fflush(f);
        fclose(f);
        ok = true;
        // Report the pixel statistics: "a file was written" is not evidence that anything
        // was drawn (field guide §4.3 - log the state, not the symptom).
        const u32* px32 = (const u32*)lr.pBits;
        const int row_words = lr.Pitch / 4;
        u32 nonblack = 0, samples = 0;
        u32 first[3] = {0, 0, 0};
        for (UINT i = 0; i < px; i += 97) {
            u32 c = px32[(size_t)(i / w) * row_words + (i % w)];
            if (c & 0x00FFFFFFu) ++nonblack;
            ++samples;
            if (samples <= 3) first[samples - 1] = c;
        }
        LOG_INFO("capture", "wrote %s %ux%u: nonblack=%u/%u sampled (%.1f%%), px=%08x %08x %08x", path, w, h, nonblack,
                 samples, 100.0 * nonblack / (double)(samples ? samples : 1), first[0], first[1], first[2]);
    } else {
        LOG_ERROR("capture", "cannot open %s for writing", path);
    }
    src->UnlockRect();
    if (owned_by_us) src->Release();
    return ok;
}

}  // namespace os::render
