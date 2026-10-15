// Uses our real D3D9 overlay and a 64x64 readback; does not execute the original game.
#include "render/device.h"
#include "render/overlay.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <cstring>
#include <vector>

int main(int argc, char** argv) {
    using namespace os;
    if (argc != 2) { std::cerr << "usage: overlay_clip_verify <new-capture.bmp>\n"; return 2; }
    if (std::filesystem::exists(argv[1])) { std::cerr << "capture already exists\n"; return 2; }
    HWND window = CreateWindowExW(0, L"STATIC", L"OpenSpore scissor verification", WS_OVERLAPPEDWINDOW,
        0, 0, 128, 128, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 1;
    render::Device device;
    render::Overlay overlay;
    render::Config config;
    config.width = config.height = 64;
    config.vsync = false;
    std::string error;
    bool ok = device.init(window, config, error) && overlay.init(device.d3d()) && device.begin_frame();
    IDirect3DTexture9* texture = nullptr;
    if (ok) {
        device.clear(0xff000000);
        overlay.begin(64, 64);
        overlay.text(2, 2, 0xffffffff, "I"); // must flush BEFORE the first scissor
        ok = overlay.set_clip(16, 16, 32, 32);
        if (ok) overlay.rect(0, 0, 64, 64, 0xffff0000);
        const u32 blue[4]{0xff0000ff, 0xff0000ff, 0xff0000ff, 0xff0000ff};
        texture = device.create_texture_bgra((const u8*)blue, 2, 2);
        ok = ok && texture && overlay.set_clip(20, 20, 28, 28);
        if (ok) overlay.image_uv(0, 0, 64, 64, texture, 0, 0, 1, 1);
        overlay.text(40, 2, 0xffffffff, "I"); // must flush UNDER the old scissor, hence invisible
        overlay.reset_clip();
        overlay.rect(50, 50, 6, 6, 0xff00ff00); // proves no scissor leaks after reset
        ok = ok && !overlay.set_clip(2, 2, 2, 3) &&
            !overlay.set_clip(std::numeric_limits<float>::quiet_NaN(), 0, 10, 10);
        overlay.end();
        device.request_capture(argv[1]);
        device.end_frame();
    }
    if (texture) texture->Release();
    overlay.shutdown();
    device.shutdown();
    DestroyWindow(window);
    if (!ok) { std::cerr << "overlay setup/draw failed: " << error << '\n'; return 1; }
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<u8> bitmap(std::istreambuf_iterator<char>(input), {});
    if (bitmap.size() != 54+64*64*4 || bitmap[0] != 'B' || bitmap[1] != 'M') {
        std::cerr << "readback dimensions/format mismatch\n"; return 1;
    }
    const auto pixel = [&](int x, int y) {
        u32 value;
        std::memcpy(&value, bitmap.data()+54+((63-y)*64+x)*4, 4);
        return value & 0xffffff;
    };
    int initial_text_pixels = 0;
    for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
        initial_text_pixels += pixel(x, y) != 0;
    if (pixel(17,17) != 0xff0000 || pixel(23,23) != 0x0000ff || pixel(10,30) != 0 ||
        pixel(33,23) != 0 || pixel(52,52) != 0x00ff00 || pixel(45,5) != 0 || !initial_text_pixels) {
        std::cerr << "scissor pixels / image / text flush / reset mismatch\n"; return 1;
    }
    std::cout << "D3D9 scissor readback: flat quad, atlas quad, text flush, empty/NaN rejection and reset PASS\n";
    return 0;
}
