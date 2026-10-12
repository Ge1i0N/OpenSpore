#include "render/device.h"

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

    // 3. Create the device. Software vertex processing keeps this scaffold independent of
    //    shader profiles; the RenderWare bridge will revisit this (field guide §3).
    m_width = cfg.width;
    m_height = cfg.height;
    fill_pp(m_pp, m_width, m_height, cfg.windowed);
    DWORD flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    hr = m_d3d9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, flags, &m_pp, &m_device);
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
    LOG_INFO("render", "device %s %dx%d vsync=%d (desktop %dx%d fmt=%d) flags=SWVP",
             cfg.windowed ? "windowed" : "fullscreen", m_width, m_height, (int)m_present_interval_one,
             (int)m_desktop.Width, (int)m_desktop.Height, (int)m_desktop.Format);
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
    if (!m_lost) m_device->Present(nullptr, nullptr, nullptr, nullptr);
}

void Device::clear(u32 argb) {
    if (!m_device || !m_in_scene) return;
    m_device->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, (D3DCOLOR)argb, 1.0f, 0);
}

Device& device() { return g_device; }

}  // namespace os::render
