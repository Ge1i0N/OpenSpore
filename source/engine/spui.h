#pragma once
// The `spui` UI layout reader - OUR implementation, from MEASURED facts.
//
// What makes this possible is that the format's dictionary is SHIPPED IN THE GAME: the 14 `xml`
// resources (type 0xCF6C21B8) are EA's own Spore UI Designer project files, and their <Class>/
// <Property> `proxy` attributes give 306 ids. 227 of them appear in the `spui` binaries and 748/748
// layouts carry them. See analysis/spui-schema.md for every measurement; the table itself is
// generated into source/engine/spui_schema.inc by tools/gen_spui_schema.py.
//
// Sequential framing from GA 0x82ce10 / 0x82b3f0, verified through EOF on 748/748 resources:
//   header + external-resource tables + class ids;
//   0x5FF5:u16 | object_index:u16 | root_flag/property_count:u16 | properties;
//   property = id:u32 | type:u16 | count:u16 | typed values.
// Code 19 is count u16 object references (0xFFFF=null); code 20 is nested structs, NOT 6-byte values.
// Unknown ids keep their framing and offsets. The designer dictionary supplies optional names only.
// Historical anchor measurements and the inline-Children hypothesis are superseded; see
// analysis/spui-binary.md. Reading all structure does not implement drawables, WinProcs or commands.
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
// first trailing id is a root ControlID; the second remains unresolved. These are distinct from
// the serialized object indexes used by Children and drawable references.
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
    u16 object_index = 0;
    u32 class_id = 0;
    int parent = -1;                  // index into Layout::windows; identity is NOT ControlID
    std::vector<u16> child_objects;    // serialized code-19 references, including null
    std::vector<size_t> children;      // resolved indexes into Layout::windows, in file order
    u16 drawable_object = 0xffff;      // object referenced by WindowBase::FillDrawable
    Rect placed_area;                 // Area translated by ancestor origins
    bool visible = false;             // includes ancestor visibility
    bool enabled = false;             // includes ancestor enabled state
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

struct Property {
    u32 offset = 0, end = 0;
    u32 id = 0;
    u16 code = 0, count = 0;
    u16 depth = 0;                  // 0=object property; >0=member inside code-20 structure
    std::vector<u16> refs;          // code-19 object/external references
    std::vector<u8> raw;            // scalar/tuple bytes, retained for drawable properties
};

struct ExternalResource {
    u32 instance = 0;
    u32 type = 0;
    u32 group = 0;
};

struct Object {
    u16 index = 0;
    u32 class_id = 0, offset = 0, end = 0;
    bool root = false;               // object property-count word's high bit (GA 0x82ce10)
    int window = -1;
    std::vector<Property> properties; // includes nested struct fields, preserving their offsets
    std::vector<u16> external_refs;   // Image (0x01BE6B15), property 0x01BE0001 -> external indexes
};

struct Layout {
    std::string package;
    u32 record = 0;
    u32 instance = 0;
    u32 bytes = 0;
    std::vector<Window> windows;
    std::vector<Object> objects;
    std::vector<ExternalResource> external_resources;
    std::vector<size_t> roots;        // window indexes with no parent
    u16 version = 0;
    u32 consumed = 0;
    size_t child_links = 0;
    size_t drawable_links = 0;
    bool sequential = false;
    size_t anchors = 0;              // compatibility name: exact properties, including nested members
    size_t multi = 0;                // properties whose count != 1
    size_t recognized = 0;           // how many of those had a name in the shipped schema
    std::string name() const;
    // The layout's DESIGN RESOLUTION: the widest right/bottom among its usable Areas. Both the
    // presenter (to scale down to the viewport) and the hit-test (to scale a pointer back up into
    // design coordinates) call THIS, so the two can never disagree about where a widget is. A
    // duplicated max-right/max-bottom scan is exactly the drift that makes a click land one pixel
    // off a button while the screen looks perfect.
    void design_resolution(float& w, float& h) const;
};

// Transactional, bounded reader from GA 0x82ce10 / 0x82b3f0. Unknown ids do not prevent decoding.
// On failure layout is unchanged and error includes the byte offset; there is no heuristic fallback.
bool parse_binary(const std::vector<u8>& bytes, Layout& layout, std::string& error);

// Parent-before-Children presentation traversal using the serialized graph, including visibility.
// Sibling forward order is our current policy; original dynamic Z order/WinProcs remain open.
std::vector<size_t> paint_order(const Layout& lay);

// Intersect viewport with Clip-marked ancestors (not the window's own Area). Empty on invalid
// indices/cycles/intersections. Shared host presentation policy for renderer and pointer picking.
Rect inherited_clip(const Layout& lay, size_t window, const Rect& viewport);

// Which window of `lay` is under the VIEWPORT point (vx,vy), when the viewport is (vw,vh)?
// Returns null when the point hits nothing usable.
//
// Reverse presentation order, honoring ancestor visibility/enabled, IgnoreMouse and parent Clip.
//
// A window with no usable Area cannot be hit: it has no geometry, and inventing one would let a
// click land on a widget that was never drawn.
const Window* hit_test(const Layout& lay, float vx, float vy, float vw, float vh);

class Reader {
public:
    // Find and parse every `spui` resource (type 0x250FE9A2) on the package search paths.
    // Never throws; a missing/undecodable resource is logged and skipped.
    void load(size_t limit = 0);

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

// Compatibility verb: dump exact properties, with bounded text output. Does not search for anchors.
std::string dump_anchors(u32 instance, size_t maxn);
std::string describe_tree(const Layout& layout);

// The schema lookup, for logging and for the `spui` console verb.
const char* schema_name(u32 id);
const char* schema_type(u32 id);
u16 schema_code(u32 id);

}  // namespace os::spui
