#pragma once
// D3D9 device + swap chain, mirroring the original's import surface.
//
// Evidence (level 1): the target imports exactly ONE d3d9 function -
// d3d9.dll!Direct3DCreate9 - and d3dx9_27.dll for matrix/shader helpers. All RenderWare
// lives inside .text. Our first milestone does not need RW at all: a windowed clear +
// overlay quad stream is enough to prove the boot chain, and the device creation path
// reproduces the original's error contract:
//   'Could not start the renderer.'          0x014139e0 (ref 0x7e6ff7)
//   'Could not create the Graphics Device.'  0x01413ac8 (ref 0x7e748f)
//   'Please ensure your display is set to 32 bits.' 0x014139fe
//   'Direct X is unavailable.'               0x01413aee
// and the default mode seen at 0x7e74f1: width 0x258=600? no - [esp+0x30]=0x258(600) and
// [esp+0x34]=0x320(800) are passed to 0x410330 as a (w,h) pair: 800x600 was the original's
// fallback resolution. We default to the args (1280x720) but keep 800x600 as the honest
// fallback when the desktop cannot satisfy the request.
#include <d3d9.h>

#include <string>

#include "common.h"

namespace os::render {

struct Config {
    bool windowed = true;
    int width = 1280;
    int height = 720;
    bool vsync = true;
    u32 adapter = D3DADAPTER_DEFAULT;
};

class Device {
public:
    ~Device();

    // Returns false with `err` set to the SAME user-facing strings the original used
    // (see header). Never shows a dialog: PROMPT.md + field guide §4.
    bool init(HWND hwnd, const Config& cfg, std::string& err);
    void shutdown();

    bool resize(int w, int h);

    IDirect3DDevice9* d3d() { return m_device; }
    IDirect3D9* d3d9() { return m_d3d9; }
    const D3DDISPLAYMODE& desktop_mode() const { return m_desktop; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    bool windowed() const { return m_cfg.windowed; }
    bool vsync() const { return m_present_interval_one; }

    // True when a scene was started (caller must pair with end_frame()). False means the
    // device is lost/not-reset and the frame should be skipped entirely.
    bool begin_frame();
    void end_frame();
    void clear(u32 argb);

    // In-engine frame capture. PrintWindow cannot grab D3D9 content (it returns an
    // all-black client area for a window that is verifiably rendering), so the only
    // machine-checkable visual evidence is a capture from inside the device:
    // GetFrontBufferData -> SYSTEMMEM surface -> LockRect -> 32bpp BMP.
    // Signatures verified against SDK 10.0.26100 shared\d3d9.h:466-467:
    //   GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface)
    //   GetFrontBufferData (UINT iSwapChain,               IDirect3DSurface9* pDestSurface)
    // Both take a READY destination surface, not a **out** parameter - the first version of
    // this function passed &front and did not compile.
    bool capture_frame(const char* path);

    // Upload tightly-packed 32-bit BGRA (top-down) as an A8R8G8B8 texture. Used to put a real
    // decoded game asset on screen. Returns nullptr on failure (and logs).
    IDirect3DTexture9* create_texture_bgra(const u8* bgra, int w, int h);

    // Defer a capture to the next end_frame(), which is the only point where a scene is
    // guaranteed open (GetFrontBufferData requires one). Returns the path for logging.
    void request_capture(const char* path);
    bool capture_pending() const { return m_capture_request[0] != '\0'; }

private:
    void fill_pp(D3DPRESENT_PARAMETERS& pp, int w, int h, bool windowed) const;

    IDirect3D9* m_d3d9 = nullptr;
    IDirect3DDevice9* m_device = nullptr;
    HWND m_hwnd = nullptr;
    Config m_cfg{};
    int m_width = 0, m_height = 0;
    D3DDISPLAYMODE m_desktop{};
    D3DPRESENT_PARAMETERS m_pp{};   // remembered: GetPresentationParameters is NOT in this
                                    // SDK's IDirect3DDevice9 (verified: 0 hits in shared\d3d9.h
                                    // and um\d3d9helper.h of SDK 10.0.26100; it is 9Ex-only)
    bool m_present_interval_one = false;
    bool m_lost = false;
    bool m_in_scene = false;
    // Deferred capture: GetFrontBufferData only works inside an open scene, so the request
    // is recorded here and performed by the state's render callback while the scene is live.
    char m_capture_request[MAX_PATH] = {};
};

// One device for the whole app (the original has exactly one Direct3DCreate9 import).
Device& device();

}  // namespace os::render
