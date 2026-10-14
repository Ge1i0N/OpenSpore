#pragma once
// The `spui` UI layout reader - OUR implementation, from MEASURED facts.
//
// What makes this possible is that the format's dictionary is SHIPPED IN THE GAME: the 14 `xml`
// resources (type 0xCF6C21B8) are EA's own Spore UI Designer project files, and their <Class>/
// <Property> `proxy` attributes give 306 ids. 227 of them appear in the `spui` binaries and 748/748
// layouts carry them. See analysis/spui-schema.md for every measurement; the table itself is
// generated into source/engine/spui_schema.inc by tools/gen_spui_schema.py.
//
// THE RECORD, measured over 1,098,129 anchors in 748 layouts:
//
//     id:u32 | code:u16 | marker:u16(==1) | payload[code]
//
//   * `code` is a TYPE CODE, stable per id - 178 of the 182 high ids (>=0x1000) have exactly ONE code
//     across the whole corpus with 100% agreement (tools/spui_typecode.py). It is NOT a length, which
//     is what the first attempt got wrong.
//   * payload sizes are measured by majority vote (tools/spui_read.py): 9/11 -> 4 bytes, 16 -> 16
//     (a `rectf`), 17 -> 8 (`pointf`), 20 -> 6, 19 -> a u16 element count, 18 -> a u16 CHARACTER count
//     then UTF-16.
//   * the stream is BYTE-packed, not bit-packed. Record sizes are simply not multiples of 4 (code 20
//     gives a 14-byte record), so the id cursor drifts across all four alignments. That drift, and
//     nothing else, is why the mod-4 histogram is uniform - see the CORRECTION section in
//     analysis/spui-schema.md, because this file's predecessor misread that histogram as bit packing.
//
// WHAT IS NOT SOLVED, stated rather than hidden: a sequential walk reaches EOF cleanly in 0/748
// layouts and derails in 637 with "id not in schema", because the designer XML lists only 227 of the
// ids the binaries use. So this reader is ANCHOR-DRIVEN, not sequential: it locates every (id, marker==1)
// pair and reads each record's own value, which is exact for what it covers and does not depend on the
// bytes in between. The remaining work is extending the vocabulary, not understanding the structure.
//
// THE MENU PAYLOAD: code 18 is overloaded, split by a leading u16 of 0xFFFF - 4,207 inline UTF-16
// strings and 3,107 references, of which 21 of 36 distinct values are real en-us `locale` TABLE
// INSTANCES (accidental rate 4.66e-08). So a layout names the TEXT TABLE it draws from, not a
// per-string key, and engine/text.cpp already resolves those tables. That is the link between the UI
// layout, the state machine and the localized text.
#include <string>
#include <vector>

#include "common.h"

namespace os::spui {

struct Rect {
    // MEASURED, not assumed: the four floats of `Area` are LEFT, TOP, RIGHT, BOTTOM, not x/y/w/h.
    // The values settled it - (168, -10, 192, 14) is a 24x24 icon and (-14, -9, 10, 15) is another
    // 24x24, while (0, 3, 181, 5) is a 181x2 bar. Read as x/y/w/h they are nonsense (a height of 14
    // at y=-10, a width of 181 with a height of 5). EA's `rectf` is a corner pair, like a Win32 RECT.
    f32 left = 0, top = 0, right = 0, bottom = 0;
    f32 w() const { return right - left; }
    f32 h() const { return bottom - top; }
    bool valid() const { return w() > 0.f && h() > 0.f; }
};

struct Window {
    u32 offset = 0;            // byte offset of its ControlID record, for provenance
    u32 control_id = 0;
    u32 command_id = 0;
    u32 flags = 0;
    u32 bg_color = 0;          // ARGB as stored
    u32 shade_color = 0;
    u32 text_font = 0;
    Rect area;
    bool has_area = false;
    // Code-18 (Caption) content, in whichever of the two forms it came in.
    std::string caption;             // decoded inline text (UTF-16 -> code points)
    u32 caption_table = 0;           // the referenced locale TABLE instance, 0 when inline
    u32 caption_key = 0;             // ...and the KEY within it: 0xFFFF|table|key, measured 1097/1426
    bool caption_is_ref = false;
    bool caption_key_seen = false;   // false = the record carried a table but no key (reclen 14)
    std::string caption_raw;         // the raw bytes, so a non-ASCII caption stays inspectable
};

struct Layout {
    std::string package;
    u32 record = 0;
    u32 instance = 0;
    u32 bytes = 0;
    std::vector<Window> windows;
    size_t anchors = 0;              // how many (id, marker==1) records were read
    size_t recognized = 0;           // how many of those had a name in the shipped schema
    std::string name() const;
};

class Reader {
public:
    // Find and parse every `spui` resource (type 0x250FE9A2) on the package search paths.
    // Never throws; a missing/undecodable resource is logged and skipped.
    void load(size_t limit = 400);

    bool loaded() const { return m_loaded; }
    const std::vector<Layout>& layouts() const { return m_layouts; }
    const Layout* find(u32 instance) const;
    // The layout with the most windows that have a usable Area, which is what a screen needs.
    const Layout* best_screen() const;
    std::string stats() const;

private:
    bool m_loaded = false;
    std::vector<Layout> m_layouts;
};

Reader& instance();

// The schema lookup, for logging and for the `spui` console verb.
const char* schema_name(u32 id);
const char* schema_type(u32 id);
u16 schema_code(u32 id);

}  // namespace os::spui
