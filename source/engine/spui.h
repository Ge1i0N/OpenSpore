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
// THE MENU PAYLOAD: code 18 is overloaded, split by a leading u16 of 0xFFFF. A reference is
// `0xFFFF | table:u32 | key:u32`, so a layout names BOTH the locale TABLE and the KEY within it -
// 1,384 of 1,426 references resolve to real en-us strings through engine/text.cpp (the remaining 42
// are classified by `spui -gaps`: 15 keys whose table is not in our corpus at all, 4 keys a present
// table does not define). The earlier reading that a layout names only a TABLE was superseded by that
// measurement. This is the link between the UI layout, the state machine and the localized text.
//
// WHAT THE STATE SCRIPT GIVES THE UI: `uilayout -load <name> <id> <id>` (the only UI verb in the
// whole recovered script, in state DevLauncher). The NAME hashes to the layout's instance -
// E3LaunchScreen -> 0xE932BD44, a real `spui` record - so a state can select its screen by name. The
// two trailing ids are NOT resource ids (they appear in no type/group/instance field of any of the
// 150,945 records) and no registry name hashes to them; the first appears twice inside the spui byte
// corpus and the second not at all, so they are internal to the layout or runtime handles. Unresolved
// and currently unnecessary: `spui::Reader::find_by_name()` does not need them.
#include <cstdio>
#include <map>
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
    bool contains(f32 px, f32 py) const {
        // Half-open on the far edges, so two adjacent windows sharing an edge cannot both claim a
        // click on it.
        return px >= left && px < right && py >= top && py < bottom;
    }
    std::string describe() const {
        char b[80];
        std::snprintf(b, sizeof(b), "Area (%g,%g)-(%g,%g) %gx%g", left, top, right, bottom,
                      (double)w(), (double)h());
        return b;
    }
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
    size_t anchors = 0;              // how many (id, count) records were read
    size_t multi = 0;                // of those, how many had count != 1 (the old scan skipped them)
    size_t recognized = 0;           // how many of those had a name in the shipped schema
    std::string name() const;
    // The layout's DESIGN RESOLUTION: the widest right/bottom among its usable Areas. Both the
    // presenter (to scale down to the viewport) and the hit-test (to scale a pointer back up into
    // design coordinates) call THIS, so the two can never disagree about where a widget is. A
    // duplicated max-right/max-bottom scan is exactly the drift that makes a click land one pixel
    // off a button while the screen looks perfect.
    void design_resolution(float& w, float& h) const;
};

// The order to PAINT a layout in: largest Area first, so a container is painted behind everything it
// holds. ONE RULE, SHARED BY DRAWING AND HIT-TESTING.
//
// Why this is not cosmetic: the record order in the data is not a z-order (measured - the full-screen
// root of E3LaunchScreen is the LAST record in the vector, so painting in record order paints the
// root OVER the panels it contains). hit_test() resolves an overlapping click to the SMALLEST
// containing window, on the grounds that a user aims at the most specific box. For the two to agree,
// the painter must put the largest first. If they disagree, the engine would hit-test a widget the
// user cannot see, which is worse than no clicks at all.
//
// Returns indices into lay.windows. Ties keep record order. When the layout's real parent/child
// hierarchy is reversed from the data, this is the single function to replace - and hit_test needs no
// change, because "smallest wins" is exactly the inverse of "largest painted first".
std::vector<size_t> paint_order(const Layout& lay);

// Which window of `lay` is under the VIEWPORT point (vx,vy), when the viewport is (vw,vh)?
// Returns null when the point hits nothing usable.
//
// TOPMOST = the LAST window in draw order that contains the point, so the answer is what the user
// SEES rather than what the designer listed first. Walks the vector in reverse for that reason.
//
// A window with no usable Area cannot be hit: it has no geometry, and inventing one would let a
// click land on a widget that was never drawn.
const Window* hit_test(const Layout& lay, float vx, float vy, float vw, float vh);

class Reader {
public:
    // Find and parse every `spui` resource (type 0x250FE9A2) on the package search paths.
    // Never throws; a missing/undecodable resource is logged and skipped.
    void load(size_t limit = 400);

    bool loaded() const { return m_loaded; }
    const std::vector<Layout>& layouts() const { return m_layouts; }
    // The layout whose INSTANCE is `inst`, or null. PRECEDENCE IS EXPLICIT, not an accident of
    // traversal order: the corpus carries 748 `spui` RESOURCES but only 524 distinct INSTANCES, so
    // 224 layouts are shipped more than once (E3LaunchScreen is in both Data/PatchData.package,
    // 4,486 B, and Data/Spore_Graphics.package, 4,480 B). The container that comes FIRST on the
    // engine's search path wins, because that is what mounting a patch ahead of the content
    // packages means - and it makes PatchData override base content, which is its job.
    // XXX ASSUMED, NOT REVERSED: the original's own resource-cache resolution order is not yet read
    // off the disassembly. text.cpp currently uses the OPPOSITE rule (last parsed wins), so the two
    // subsystems can disagree for a resource in both. The affected surface is measured and small -
    // 3 locale strings out of 23,933 - and it is recorded as an open question rather than hidden.
    const Layout* find(u32 instance) const;
    // The layout a STATE SCRIPT names, e.g. `uilayout -load E3LaunchScreen`. This is the seam
    // between "we can render a layout" and "the state machine drives the UI": the script gives a
    // NAME, and fnv1_lower(name) is the resource INSTANCE (measured: E3LaunchScreen -> 0xE932BD44,
    // which is a real `spui` record; LaunchScreen -> 0x562AB2CC, likewise). Names are never hashed
    // by guesswork here - see the note on ids in analysis/states.md.
    const Layout* find_by_name(const std::string& name) const;
    // How many of the parsed resources were SHADOWED, i.e. their instance already came from an
    // earlier container. Reported so the 748-vs-524 gap is a number, not a footnote.
    size_t shadowed() const { return m_shadowed; }
    size_t distinct_instances() const { return m_distinct; }
    // Of the shadows, how many hide a DIFFERENT byte length - i.e. how much the precedence rule
    // could actually change what is drawn. Zero would make the whole question moot; it is not zero.
    size_t shadowed_differing() const { return m_shadowed_diff; }
    // The layout with the most windows that have a usable Area, which is what a screen needs.
    // This is OUR choice of what to show; prefer find_by_name() when a state names one.
    const Layout* best_screen() const;
    std::string stats() const;

private:
    bool m_loaded = false;
    std::vector<Layout> m_layouts;
    std::map<u32, size_t> m_by_instance;  // instance -> index into m_layouts, FIRST container wins
    size_t m_shadowed = 0;
    size_t m_distinct = 0;
    // Shadows whose byte size DIFFERS from the winner - the ones that could actually change what is
    // on screen, and therefore the measure of how much the (assumed) precedence rule matters.
    size_t m_shadowed_diff = 0;
};

Reader& instance();

// Dump one layout's ANCHOR LIST in file order (offset, id, schema name, code, element count). This is
// ground truth from the reader that already works, for questions the reader cannot answer alone - in
// particular what surrounds a `Children` record (0xEEC1B00B), and therefore what the u16 after it
// counts. Re-reads the resource rather than caching 748 blobs.
std::string dump_anchors(u32 instance, size_t maxn);

// The schema lookup, for logging and for the `spui` console verb.
const char* schema_name(u32 id);
const char* schema_type(u32 id);
u16 schema_code(u32 id);

}  // namespace os::spui
