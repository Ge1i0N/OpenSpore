#include "engine/spui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <functional>
#include <stdexcept>

namespace os::spui {
namespace {

// GA oracle: 0x82ce10 (file/object tables), 0x82b3f0 (property dispatch).
// In particular, code 19 reads count u16 object indexes, NOT an inline array length.
class BinaryReader {
public:
    BinaryReader(const std::vector<u8>& data, Layout& layout) : d(data), out(layout) {}

    void parse() {
        if (u32le() != 0xe3fe3fb8u) fail("wrong SPUI magic");
        out.version = u16le();
        // Version 1 has a separate legacy payload-length path (0x82b240); not implemented here.
        if (out.version != 2 && out.version != 3) fail("unsupported SPUI version");
        u32 counts[4];
        for (u32& count : counts) count = u16le();
        total = counts[0] + counts[1] + counts[2] + counts[3];
        if (total > 6000) fail("resource/object count exceeds original limit");
        out.external_resources.clear();
        out.external_resources.reserve(counts[0] + counts[1]);
        // GA reader evidence: external triples are instance, type, group. E3LaunchScreen's group-1
        // records are PNG type 0x2F7D0004, and code-19 Image refs index these triples.
        for (u32 n = 0; n < counts[0] + counts[1]; ++n) {
            ExternalResource ext;
            ext.instance = u32le();
            ext.type = u32le();
            ext.group = u32le();
            out.external_resources.push_back(ext);
        }
        for (u32 n = 0; n < counts[2]; ++n) {
            const u8 kind = out.version >= 3 ? byte() : 0;
            if (kind == 0) skip(12);
            else if (kind == 1) {
                skip(8);
                const u32 runs = u32le();
                if (runs > (d.size() - pos) / 2) fail("hitmask length exceeds input");
                skip(2 * runs);
            } else fail("unsupported hitmask kind");
        }
        const u32 first = total - counts[3];
        std::vector<u32> classes;
        for (u32 n = 0; n < counts[3]; ++n) classes.push_back(u32le());
        std::vector<int> object_slots(total, -1);
        while (true) {
            const size_t off = pos;
            if (u16le() != 0x5ff5) fail("expected object marker 0x5FF5");
            const u16 index = u16le();
            if (index == 0xffff) {
                if (u32le() != 0x1c01c047u) fail("wrong SPUI end magic");
                if (pos != d.size()) fail("trailing SPUI bytes");
                break;
            }
            if (index < first || index >= total || object_slots[index] >= 0)
                fail("invalid/repeated object index");
            const u16 flags = u16le();
            const u16 count = flags & 0x7fff;
            if (count > 200) fail("object property count exceeds original limit");
            Object obj;
            obj.index = index;
            obj.class_id = classes[index - first];
            obj.offset = (u32)off;
            obj.root = (flags & 0x8000) != 0;
            Window win;
            win.object_index = index;
            win.class_id = obj.class_id;
            win.offset = (u32)off;
            bool is_window = false;
            for (u32 n = 0; n < count; ++n) property(obj, &win, &is_window, 0);
            obj.end = (u32)pos;
            object_slots[index] = (int)out.objects.size();
            if (is_window) {
                obj.window = (int)out.windows.size();
                out.windows.push_back(std::move(win));
            }
            out.objects.push_back(std::move(obj));
        }
        for (u16 ref : references)
            if (ref != 0xffff && ref >= total) fail("object reference out of range");
        for (const Object& obj : out.objects) {
            out.drawable_links += obj.external_refs.size();
            for (u16 ref : obj.external_refs)
                if (ref >= out.external_resources.size()) fail("Image external index out of range");
        }
        std::vector<int> windows(total, -1);
        for (size_t n = 0; n < out.windows.size(); ++n)
            windows[out.windows[n].object_index] = (int)n;
        for (size_t n = 0; n < out.windows.size(); ++n) {
            Window& win = out.windows[n];
            for (u16 ref : win.child_objects) {
                if (ref == 0xffff) continue;
                if (ref >= total || windows[ref] < 0) fail("Children references non-window object");
                const size_t child = (size_t)windows[ref];
                if (out.windows[child].parent >= 0) fail("window has multiple parents");
                out.windows[child].parent = (int)n;
                win.children.push_back(child);
                ++out.child_links;
            }
        }
        // Detect cycles before publishing any geometry. Parent-relative placement is kept separate
        // from the serialized Area so the original data remains inspectable.
        std::vector<u8> visits(out.windows.size(), 0);
        std::vector<size_t> levels(out.windows.size(), 0);
        std::function<void(size_t, size_t)> place = [&](size_t n, size_t depth) {
            if (depth > 256) fail("window parent depth exceeds 256");
            if (visits[n] == 1) fail("cyclic Children graph");
            if (visits[n] == 2) return;
            visits[n] = 1;
            Window& win = out.windows[n];
            win.placed_area = win.area;
            win.visible = (win.flags & 1) != 0;
            win.enabled = (win.flags & 2) != 0;
            if (win.parent >= 0) {
                const size_t parent = (size_t)win.parent;
                place(parent, depth + 1);
                const Window& p = out.windows[parent];
                levels[n] = levels[parent] + 1;
                if (levels[n] > 256) fail("window parent depth exceeds 256");
                win.placed_area.left += p.placed_area.left;
                win.placed_area.right += p.placed_area.left;
                win.placed_area.top += p.placed_area.top;
                win.placed_area.bottom += p.placed_area.top;
                win.visible = win.visible && p.visible;
                win.enabled = win.enabled && p.enabled;
            }
            visits[n] = 2;
        };
        for (size_t n = 0; n < out.windows.size(); ++n) {
            place(n, 0);
            if (out.windows[n].parent < 0) out.roots.push_back(n);
        }
        out.consumed = (u32)pos;
        out.bytes = (u32)d.size();
        out.sequential = true;
    }

private:
    const std::vector<u8>& d;
    Layout& out;
    size_t pos = 0;
    u32 total = 0;
    std::vector<u16> references;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(message) + " at byte " + std::to_string(pos));
    }
    void skip(size_t n) {
        if (n > d.size() - pos) fail("truncated SPUI");
        pos += n;
    }
    u8 byte() { const size_t off = pos; skip(1); return d[off]; }
    u16 u16le() {
        const size_t off = pos; skip(2);
        return (u16)(u32(d[off]) | (u32(d[off + 1]) << 8));
    }
    u32 u32le() {
        const size_t off = pos; skip(4);
        return u32(d[off]) | (u32(d[off + 1]) << 8) | (u32(d[off + 2]) << 16) |
               (u32(d[off + 3]) << 24);
    }
    static size_t width(u16 code) {
        switch (code) {
            case 2: case 3: case 7: return 1;
            case 4: case 8: return 2;
            case 5: case 9: case 11: return 4;
            case 6: case 10: case 12: case 15: case 17: return 8;
            case 14: case 16: return 16;
            default: return 0;
        }
    }
    std::string string_value(Window* win) {
        const u16 chars = u16le();
        if (chars == 0xffff) {
            const u32 table = u32le(), key = u32le();
            if (win) {
                win->caption_is_ref = win->caption_key_seen = true;
                win->caption_table = table;
                win->caption_key = key;
            }
            return {};
        }
        std::string utf8;
        for (u32 n = 0; n < chars; ++n) {
            u32 cp = u16le();
            if (cp >= 0xd800 && cp <= 0xdbff && n + 1 < chars) {
                const u32 low = u16le();
                ++n;
                if (low < 0xdc00 || low > 0xdfff) fail("invalid UTF-16 surrogate pair");
                cp = 0x10000 + ((cp - 0xd800) << 10) + low - 0xdc00;
            } else if (cp >= 0xd800 && cp <= 0xdfff) fail("unpaired UTF-16 surrogate");
            if (cp < 0x80) utf8.push_back((char)cp);
            else if (cp < 0x800) {
                utf8.push_back((char)(0xc0 | (cp >> 6)));
                utf8.push_back((char)(0x80 | (cp & 63)));
            } else if (cp < 0x10000) {
                utf8.push_back((char)(0xe0 | (cp >> 12)));
                utf8.push_back((char)(0x80 | ((cp >> 6) & 63)));
                utf8.push_back((char)(0x80 | (cp & 63)));
            } else {
                utf8.push_back((char)(0xf0 | (cp >> 18)));
                utf8.push_back((char)(0x80 | ((cp >> 12) & 63)));
                utf8.push_back((char)(0x80 | ((cp >> 6) & 63)));
                utf8.push_back((char)(0x80 | (cp & 63)));
            }
        }
        return utf8;
    }
    void property(Object& obj, Window* win, bool* is_window, u32 depth) {
        if (depth > 64) fail("nested struct depth exceeds 64");
        Property p;
        p.offset = (u32)pos;
        p.id = u32le();
        p.code = u16le();
        p.count = u16le();
        p.depth = (u16)depth;
        const size_t value = pos;
        ++out.anchors; // now exact property count, including nested struct members
        if (p.count != 1) ++out.multi;
        const bool top = depth == 0;
        if (top && (p.id == 0xeec1b000 || p.id == 0xeec1b001 || p.id == 0xeec1b005 ||
                    p.id == 0xeec1b00b)) *is_window = true;
        if (const size_t elem = width(p.code)) {
            skip(elem * p.count);
            p.raw.assign(d.begin() + value, d.begin() + pos);
            if (top && p.count == 1) {
                if (p.code == 9) {
                    const size_t end = pos;
                    pos = value;
                    const u32 v = u32le();
                    pos = end;
                    switch (p.id) {
                        case 0xeec1b000: win->flags = v; break;
                        case 0xeec1b001: win->control_id = v; win->offset = p.offset; break;
                        case 0xeec1b002: win->command_id = v; break;
                        case 0xeec1b004: win->shade_color = v; break;
                        case 0xeec1b006: win->bg_color = v; break;
                        case 0xeec1b00e: win->text_font = v; break;
                        default: break;
                    }
                } else if (p.id == 0xeec1b005 && p.code == 16) {
                    f32 coords[4];
                    std::memcpy(coords, d.data() + value, sizeof(coords));
                    win->area = {coords[0], coords[1], coords[2], coords[3]};
                    win->has_area = std::all_of(coords, coords + 4, [](f32 x) { return std::isfinite(x); }) &&
                                    win->area.valid() && win->area.w() < 100000.f && win->area.h() < 100000.f;
                }
            }
        } else if (p.code == 19) {
            for (u32 n = 0; n < p.count; ++n) {
                const u16 ref = u16le();
                references.push_back(ref);
                p.refs.push_back(ref);
                if (top && p.id == 0xeec1b00b) win->child_objects.push_back(ref);
                if (top && p.id == 0x01be0001u && obj.class_id == 0x01be6b15u)
                    obj.external_refs.push_back(ref);
                if (top && p.id == 0xeec1b007u && n == 0) win->drawable_object = ref;
            }
        } else if (p.code == 18) {
            for (u32 n = 0; n < p.count; ++n) {
                const bool caption = top && p.id == 0xeec1b00a && n == 0;
                std::string s = string_value(caption ? win : nullptr);
                if (caption) win->caption = std::move(s);
            }
        } else if (p.code == 20) {
            u32le(); // struct metadata/tag; not a byte length
            for (u32 n = 0; n < p.count; ++n) {
                const u16 count = u16le();
                if (count > 200) fail("nested property count exceeds original limit");
                for (u32 k = 0; k < count; ++k) property(obj, win, is_window, depth + 1);
            }
        } else fail("unsupported property type");
        p.end = (u32)pos;
        obj.properties.push_back(p);
    }
};

}  // namespace

bool parse_binary(const std::vector<u8>& bytes, Layout& layout, std::string& error) {
    Layout result;
    result.package = layout.package;
    result.instance = layout.instance;
    result.record = layout.record;
    try {
        BinaryReader(bytes, result).parse();
    } catch (const std::runtime_error& ex) {
        error = ex.what();
        return false;
    }
    layout = std::move(result);
    error.clear();
    return true;
}

}  // namespace os::spui
