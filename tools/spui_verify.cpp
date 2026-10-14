// Compare the C++ parser against independently decoded corpus fixtures; test rejection atomically.
#include "engine/spui.h"
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

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "usage: spui_verify <fixture-directory>\n"; return 2; }
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
    return 0;
}
