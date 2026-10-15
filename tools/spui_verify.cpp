// Compare the C++ parser against independently decoded corpus fixtures; test rejection atomically.
#include "engine/spui.h"
#include "engine/spui_drawable.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <algorithm>

using namespace os;

static std::vector<u8> bytes(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary);
    return std::vector<u8>(std::istreambuf_iterator<char>(f), {});
}

static bool clipping_checks() {
    spui::Layout lay;
    lay.windows.resize(3);
    lay.roots = {0};
    for (auto& win : lay.windows) win.has_area = win.visible = win.enabled = true;
    lay.windows[0].area = lay.windows[0].placed_area = {0, 0, 100, 100};
    lay.windows[0].flags = 0x400;
    lay.windows[0].children = {1};
    lay.windows[1].parent = 0;
    lay.windows[1].area = lay.windows[1].placed_area = {20, 20, 80, 80};
    lay.windows[1].flags = 0x400;
    lay.windows[1].children = {2};
    lay.windows[2].parent = 1;
    lay.windows[2].area = lay.windows[2].placed_area = {-10, -10, 120, 120};
    const spui::Rect viewport{0, 0, 100, 100};
    auto clip = spui::inherited_clip(lay, 2, viewport);
    if (clip.left != 20 || clip.top != 20 || clip.right != 80 || clip.bottom != 80) return false;
    // Own Clip flag limits children, not this window's own rendering.
    clip = spui::inherited_clip(lay, 1, viewport);
    if (clip.left != 0 || clip.right != 100) return false;
    if (spui::inherited_clip(lay, 2, {90, 90, 100, 100}).valid()) return false;
    if (spui::hit_test(lay, 30, 30, 100, 100) != &lay.windows[2] ||
        spui::hit_test(lay, 10, 10, 100, 100) != &lay.windows[0] ||
        spui::hit_test(lay, 80, 30, 100, 100) != &lay.windows[0]) return false;
    lay.windows[1].flags = 0;
    clip = spui::inherited_clip(lay, 2, viewport);
    if (clip.left != 0 || clip.right != 100) return false;
    lay.windows[0].parent = 2; // cycle: bounded even for a synthetic invalid Layout
    if (spui::inherited_clip(lay, 2, viewport).valid()) return false;
    lay.windows[0].parent = 99;
    if (spui::inherited_clip(lay, 2, viewport).valid() ||
        spui::inherited_clip(lay, 99, viewport).valid()) return false;
    return true;
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: spui_verify <fixture-directory>\n"; return 2; }
    if (!clipping_checks()) { std::cerr << "ancestor clipping/picking regression\n"; return 1; }
    const std::filesystem::path dir(argv[1]);
    std::ifstream manifest(dir / "manifest.txt");
    if (!manifest) { std::cerr << "missing manifest\n"; return 2; }
    size_t passed = 0, negative = 0;
    std::string file;
    size_t size, objects, windows, props, links, roots;
    bool negative_done = false;
    while (manifest >> file >> size >> objects >> windows >> props >> links >> roots) {
        auto data = bytes(dir / file);
        spui::Layout layout;
        std::string error;
        if (!spui::parse_binary(data, layout, error) || layout.consumed != size ||
            layout.objects.size() != objects || layout.windows.size() != windows ||
            layout.anchors != props || layout.child_links != links || layout.roots.size() != roots) {
            std::cerr << file << ": corpus mismatch " << error << '\n'; return 1;
        }
        ++passed;
        if (!negative_done && file.find("e932bd44") != std::string::npos) {
            negative_done = true;
            float dw = 0.f, dh = 0.f;
            layout.design_resolution(dw, dh);
            if (dw != 1024.f || dh != 768.f || objects != 227 || windows != 68 || links != 67)
                { std::cerr << "launch screen hierarchy mismatch\n"; return 1; }
            const spui::Window* hidden_gge = nullptr;
            for (const auto& win : layout.windows)
                if (win.control_id == 0x02ccda9c) hidden_gge = &win;
            if (!hidden_gge || hidden_gge->visible || hidden_gge->parent < 0)
                { std::cerr << "hidden GGE branch became visible\n"; return 1; }
            // An invisible parent must remove ALL descendants from both presentation and picking.
            const auto paint = spui::paint_order(layout);
            for (size_t idx : paint) {
                const auto& win = layout.windows[idx];
                if (!win.visible) { std::cerr << "hidden window in paint list\n"; return 1; }
                if (win.parent >= 0 &&
                    std::find(paint.begin(), paint.end(), (size_t)win.parent) >=
                    std::find(paint.begin(), paint.end(), idx))
                    { std::cerr << "parent painted after child\n"; return 1; }
            }
            const auto* selected = spui::hit_test(layout, 311.25f, 549.84f, 1280.f, 720.f);
            if (!selected || selected->control_id != 0x022d9e01)
                { std::cerr << "visible CreatureEditor tile not pickable\n"; return 1; }
            // Independent Python fixture: object 43 has logical 688x173 and the atlas rect below.
            // Button 44 selects 1/4 normal, 0/4 disabled, 2/4 hover, 3/4 pressed (GA 0x966a40).
            const spui::Rect bounds{10, 20, 210, 120};
            const float u0 = -0.022461000829935074f, u1 = 0.6494140028953552f;
            const auto near = [](float a, float b) { return std::abs(a-b) < 0.00001f; };
            for (u32 state = 0; state < 4; ++state) {
                const u32 frame = state == 0 ? 1u : state == 1 ? 0u : state;
                spui::ImageQuad q;
                if (!spui::drawable_quad(layout, 44, bounds, state, q) ||
                    !near(q.uv.left, u0+(u1-u0)*(float)frame/4.f) ||
                    !near(q.uv.right, u0+(u1-u0)*(float)(frame+1)/4.f) ||
                    !near(q.uv.bottom, 0.6757810115814209f) || q.area.w() != 200.f)
                    { std::cerr << "button atlas state mismatch\n"; return 1; }
            }
            spui::ImageQuad plain;
            if (!spui::drawable_quad(layout, 41, bounds, 0, plain) ||
                !near(plain.uv.top, 0.24023400247097015f) || plain.area.w() != 200.f ||
                plain.area.h() != 100.f)
                { std::cerr << "plain atlas/stretch mismatch\n"; return 1; }
            auto fixed = layout;
            for (auto& o : fixed.objects) if (o.index == 41) {
                for (auto& p : o.properties) {
                    if (p.id == 0x4f3c4a27) p.raw = {3, 0, 0, 0};
                    if (p.id == 0x4f3c4a2b) p.raw = {0, 0, 0, 0x3f}; // 0.5f
                }
            }
            if (!spui::drawable_quad(fixed, 41, bounds, 0, plain) ||
                plain.area.w() != 176.5f || plain.area.h() != 49.f ||
                plain.area.left != 21.75f || plain.area.top != 45.5f)
                { std::cerr << "fixed scale/center mismatch\n"; return 1; }
            for (u8 alignment : {u8(1), u8(2), u8(3)}) {
                for (auto& o : fixed.objects) if (o.index == 41)
                    for (auto& p : o.properties)
                        if (p.id == 0x4f3c4a29 || p.id == 0x4f3c4a28)
                            p.raw = {alignment, 0, 0, 0};
                const float left = alignment == 1 ? 10.f : alignment == 2 ? 33.5f : 21.75f;
                const float top = alignment == 1 ? 20.f : alignment == 2 ? 71.f : 45.5f;
                if (!spui::drawable_quad(fixed, 41, bounds, 0, plain) ||
                    plain.area.left != left || plain.area.top != top)
                    { std::cerr << "fixed alignment mismatch\n"; return 1; }
            }
            // Radio uses six horizontal cells, selection adds three (0x966ad0).
            spui::Rect radio_uv;
            for (const auto& o : layout.objects) if (o.index == 240)
                for (const auto& p : o.properties) if (p.id == 0x01be0002)
                    std::memcpy(&radio_uv, p.raw.data(), sizeof(radio_uv));
            for (u32 state : {0u, 1u, 2u, 3u, 8u, 9u, 10u, 11u}) {
                const u32 base = state & 7;
                const u32 frame = (base >= 2 ? base-1 : 0) + ((state & 8) ? 3 : 0);
                spui::ImageQuad q;
                if (!spui::drawable_quad(layout, 241, bounds, state, q) ||
                    !near(q.uv.left, radio_uv.left+radio_uv.w()*(float)frame/6.f) ||
                    !near(q.uv.w(), radio_uv.w()/6.f))
                    { std::cerr << "radio atlas state mismatch\n"; return 1; }
            }
            for (auto& o : fixed.objects) if (o.index == 41)
                for (auto& p : o.properties) if (p.id == 0x4f3c4a2a) p.raw = {1, 0, 0, 0};
            plain.external = 0x1234;
            if (spui::drawable_quad(fixed, 41, bounds, 0, plain) || plain.external != 0x1234)
                { std::cerr << "unsupported tiling accepted or changed output\n"; return 1; }
            if (spui::drawable_quad(layout, 0xffff, bounds, 0, plain) ||
                spui::drawable_quad(layout, 41, {0,0,0,1}, 0, plain) || plain.external != 0x1234)
                { std::cerr << "missing drawable/invalid bounds accepted\n"; return 1; }
            if (spui::hit_test(layout, 0, 0, 0, 720) ||
                spui::hit_test(layout, -1, 0, 1280, 720))
                { std::cerr << "invalid viewport accepted\n"; return 1; }
            auto reject = [&](const std::vector<u8>& broken) {
                spui::Layout sentinel;
                sentinel.instance = 0x12345678;
                sentinel.windows.emplace_back();
                sentinel.windows[0].control_id = 0x76543210;
                std::string why;
                if (spui::parse_binary(broken, sentinel, why) || why.empty() ||
                    sentinel.instance != 0x12345678 || sentinel.windows.size() != 1 ||
                    sentinel.windows[0].control_id != 0x76543210) {
                    std::cerr << "accepted corruption or changed output on failure\n";
                    return false;
                }
                ++negative;
                return true;
            };
            // Truncate at every object start and end, plus the header and final magic.
            for (const auto& obj : layout.objects) {
                if (!reject(std::vector<u8>(data.begin(), data.begin()+obj.offset)) ||
                    !reject(std::vector<u8>(data.begin(), data.begin()+obj.end))) return 1;
            }
            for (size_t n : {size_t(0), size_t(4), size_t(13), data.size()-1})
                if (!reject(std::vector<u8>(data.begin(), data.begin()+n))) return 1;
            auto broken = data;
            broken[0] ^= 1;
            if (!reject(broken)) return 1;
            broken = data;
            broken.push_back(0);
            if (!reject(broken)) return 1;
            for (const auto& obj : layout.objects) {
                for (const auto& p : obj.properties) {
                    if (p.id != 0xeec1b00b || !p.count) continue;
                    broken = data;
                    // Valid 16-bit integer, but out of bounds for this object's resource table.
                    broken[p.offset+8] = 0xfe;
                    broken[p.offset+9] = 0xff;
                    if (!reject(broken)) return 1;
                    broken = data;
                    broken[p.offset+8] = (u8)(obj.index & 255);
                    broken[p.offset+9] = (u8)(obj.index >> 8);
                    if (!reject(broken)) return 1; // self-parent cycle
                }
            }
            // A missing NAME in the designer vocabulary must never change binary framing.
            auto unknown = data;
            const auto& first_property = layout.objects.front().properties.back();
            for (size_t k = 0; k < 4; ++k)
                unknown[first_property.offset+k] = (u8)(0xdeadc0deu >> (8*k));
            spui::Layout renamed;
            if (!spui::parse_binary(unknown, renamed, error) || renamed.anchors != props ||
                renamed.consumed != data.size())
                { std::cerr << "unknown property id broke framing\n"; return 1; }
        }
    }
    if (!passed || !negative_done) { std::cerr << "incomplete fixture corpus\n"; return 1; }
    std::cout << passed << " corpus layouts match; " << negative
              << " malformed inputs rejected with output unchanged\n";
    std::cout << "drawable atlas/state/fixed-scale/alignment/unsupported-mode checks passed\n";
    std::cout << "ancestor clipping / empty intersection / boundary picking checks passed\n";
    return 0;
}
