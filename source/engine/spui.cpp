#include "engine/spui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/naming.h"  // hash_lower, so a state script's layout NAME becomes an INSTANCE
#include "engine/pkg.h"
#include "engine/spui_schema.inc"

namespace os::spui {
namespace {

constexpr u32 kSpuiType = 0x250FE9A2u;   // confirmed in code: loader 0x810940 does
                                          // `cmp dword [eax+4], 0x250FE9A2`
constexpr u16 kMarker = 0x0001;          // 98.4% of 1,098,129 anchors
constexpr u16 kCodeString = 18;          // inline UTF-16, or a locale TABLE ref when len == 0xFFFF
constexpr u16 kCodeArray = 19;
constexpr u16 kInlineSentinel = 0xFFFF;  // "this code-18 value is a reference, not text"

struct SchemaLookup {
    const SchemaEntry* by_id(u32 id) const {
        auto it = std::lower_bound(m_sorted.begin(), m_sorted.end(), id,
                                   [](const std::pair<u32, const SchemaEntry*>& a, u32 b) {
                                       return a.first < b;
                                   });
        if (it != m_sorted.end() && it->first == id) return it->second;
        return nullptr;
    }
    u16 code_or(u32 id, u16 def) const {
        const SchemaEntry* e = by_id(id);
        return (e && e->code != 0xFFFF) ? e->code : def;
    }
    std::vector<std::pair<u32, const SchemaEntry*>> m_sorted;
};

SchemaLookup g_schema;
bool g_schema_ready = false;

void ensure_schema() {
    if (g_schema_ready) return;
    g_schema.m_sorted.clear();
    for (const SchemaEntry& e : kSchema) g_schema.m_sorted.push_back({e.id, &e});
    std::sort(g_schema.m_sorted.begin(), g_schema.m_sorted.end(),
              [](const std::pair<u32, const SchemaEntry*>& a, const std::pair<u32, const SchemaEntry*>& b) {
                  return a.first < b.first;
              });
    g_schema_ready = true;
}

// The payload size a code implies, measured by majority vote over 748 layouts.
u32 payload_size(u16 code) {
    switch (code) {
        case 3:  return 1;
        case 5:  return 4;
        case 7:  return 7;
        case 9:  return 4;
        case 11: return 4;
        case 16: return 16;   // rectf
        case 17: return 8;    // pointf
        case 19: return 2;    // a u16 element count (the elements themselves follow; we record the count)
        case 20: return 6;
        case 18: return 0;    // variable, handled by the caller
        default: return 0;
    }
}

u32 rd32(const u8* p) {
    return u32(p[0]) | (u32(p[1]) << 8) | (u32(p[2]) << 16) | (u32(p[3]) << 24);
}
u16 rd16(const u8* p) { return u16(u32(p[0]) | (u32(p[1]) << 8)); }
f32 rdf(const u8* p) {
    u32 v = rd32(p);
    f32 f;
    std::memcpy(&f, &v, 4);
    return f;
}

// UTF-16LE -> UTF-8, bounded. The length comes from the file, so a mis-aligned anchor can make it
// huge; cap it before slicing rather than trusting it (this bit the python probe).
std::string utf16_to_utf8(const u8* p, size_t chars) {
    std::string out;
    if (chars > 512) chars = 512;
    out.reserve(chars * 2);
    for (size_t i = 0; i < chars; ++i) {
        u32 c = u32(rd16(p + i * 2));
        if (c == 0) break;
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < chars) {
            u32 lo = u32(rd16(p + (i + 1) * 2));
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                ++i;
            }
        }
        if (c < 0x80) {
            out.push_back((char)c);
        } else if (c < 0x800) {
            out.push_back((char)(0xC0 | (c >> 6)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        } else if (c < 0x10000) {
            out.push_back((char)(0xE0 | (c >> 12)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        } else {
            out.push_back((char)(0xF0 | (c >> 18)));
            out.push_back((char)(0x80 | ((c >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((c >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

struct Anchor {
    u32 off;
    u32 id;
    u16 code;
};

// All (id, marker==1) pairs in `d`. Anchor-driven on purpose: a sequential walk derails in 637/748
// layouts on ids the shipped schema does not list, but every anchor whose id IS known can be read
// exactly, independent of whatever sits between two anchors.
std::vector<Anchor> find_anchors(const std::vector<u8>& d) {
    std::vector<Anchor> out;
    const size_t n = d.size();
    if (n < 8) return out;
    for (const SchemaEntry& e : kSchema) {
        if (e.code == 0xFFFF) continue;              // no measured code -> cannot read it
        const u8 pat[4] = {(u8)(e.id & 0xFF), (u8)((e.id >> 8) & 0xFF), (u8)((e.id >> 16) & 0xFF),
                           (u8)((e.id >> 24) & 0xFF)};
        for (size_t k = 0; k + 8 <= n; ++k) {
            if (d[k] != pat[0] || d[k + 1] != pat[1] || d[k + 2] != pat[2] || d[k + 3] != pat[3]) {
                continue;
            }
            if (rd16(&d[k + 6]) != kMarker) continue;
            u16 code = rd16(&d[k + 4]);
            if (code != e.code) continue;            // the id's STABLE code must match
            out.push_back(Anchor{(u32)k, e.id, code});
        }
    }
    std::sort(out.begin(), out.end(), [](const Anchor& a, const Anchor& b) { return a.off < b.off; });
    // Drop overlaps: one record cannot start inside another's id.
    std::vector<Anchor> keep;
    long long last = -100;
    for (const Anchor& a : out) {
        if ((long long)a.off < last + 4) continue;
        keep.push_back(a);
        last = a.off;
    }
    return keep;
}

void parse_layout(const std::vector<u8>& d, Layout& L) {
    const size_t n = d.size();
    L.bytes = (u32)n;
    std::vector<Anchor> an = find_anchors(d);
    L.anchors = an.size();
    Window* cur = nullptr;
    for (size_t ai = 0; ai < an.size(); ++ai) {
        const Anchor& a = an[ai];
        const SchemaEntry* e = g_schema.by_id(a.id);
        if (!e) continue;
        ++L.recognized;
        // The record's payload ends where the NEXT anchor begins, not where the file does. Reading
        // past that boundary is what made every reference look like it carried a key (the stat said
        // "0 table-only" where the python measurement found records of reclen 14), so the extent is
        // taken from the anchor list and every `need()` below is honest against it.
        const size_t bound = (ai + 1 < an.size()) ? an[ai + 1].off : n;
        const u8* p = d.data() + a.off + 8;         // payload start
        const size_t avail = bound > a.off + 8 ? bound - (a.off + 8) : 0;
        auto need = [&](size_t k) { return avail >= k; };
        if (a.id == kWinControlID) {
            L.windows.emplace_back();
            cur = &L.windows.back();
            cur->offset = a.off;
            if (need(4)) cur->control_id = rd32(p);
            continue;
        }
        if (!cur) continue;                         // properties before the first window: no owner
        switch (a.id) {
            case kWinCommandID:
                if (need(4)) cur->command_id = rd32(p);
                break;
            case kWinFlags:
                if (need(4)) cur->flags = rd32(p);
                break;
            case kWinBgColor:
                if (need(4)) cur->bg_color = rd32(p);
                break;
            case kWinShadeColor:
                if (need(4)) cur->shade_color = rd32(p);
                break;
            case kWinTextFont:
                if (need(4)) cur->text_font = rd32(p);
                break;
            case kWinArea:
                if (need(16)) {
                    cur->area.left = rdf(p);
                    cur->area.top = rdf(p + 4);
                    cur->area.right = rdf(p + 8);
                    cur->area.bottom = rdf(p + 12);
                    // A rect is only usable if the floats are finite and the extent is sane; a
                    // mis-read would put a window off-screen forever.
                    if (std::isfinite(cur->area.left) && std::isfinite(cur->area.top) &&
                        std::isfinite(cur->area.right) && std::isfinite(cur->area.bottom) &&
                        cur->area.w() > 0.f && cur->area.h() > 0.f && cur->area.w() < 100000.f &&
                        cur->area.h() < 100000.f) {
                        cur->has_area = true;
                    }
                }
                break;
            case kWinCaption: {
                if (!need(2)) break;
                u16 len = rd16(p);
                if (len == kInlineSentinel) {
                    // The reference form is  0xFFFF | table:u32 | key:u32  (MEASURED:
                    // tools/spui_caption_ref.py - 1,097 of 1,426 reference records carry a key that is
                    // a real key OF THE REFERENCED TABLE, resolving to the game's own UI text, e.g.
                    // table 0xAD7B2086 + key 0x0589E51B -> "640x480"). So a widget says which TEXT
                    // TABLE it draws from and WHICH ENTRY in it, and engine/text.cpp resolves the
                    // pair. Some records stop after the table (reclen 14, e.g. table 0x40D7E685), so
                    // the key is optional and its absence is recorded rather than guessed at.
                    if (need(6)) {
                        cur->caption_is_ref = true;
                        cur->caption_table = rd32(p + 2);
                    }
                    if (need(10)) {
                        cur->caption_key = rd32(p + 6);
                        cur->caption_key_seen = true;
                    }
                } else if (len <= 512 && need((size_t)2 + 2 * (size_t)len)) {
                    cur->caption = utf16_to_utf8(p + 2, len);
                    cur->caption_raw.assign((const char*)p, 2 + 2 * (size_t)len);
                }
                break;
            }
            default:
                break;
        }
    }
}

}  // namespace

std::string Layout::name() const {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s rec[%u] inst=0x%08X %uB %zu windows",
                  package.c_str(), record, instance, bytes, windows.size());
    return buf;
}

void Reader::load(size_t limit) {
    m_loaded = false;
    m_layouts.clear();
    m_by_instance.clear();
    m_shadowed = m_distinct = 0;
    ensure_schema();
    size_t scanned = 0;
    if (limit == 0) limit = SIZE_MAX;  // 0 means "all of them", not "none"
    // Deduplicate by CANONICAL path before scanning. The dev search list contains both
    // Data=<exe>\Data and cwd=<exe>, so a recursive scan visits every package TWICE - this is a
    // trap already documented for the `pkg` verb (109 vs 57 packages), and I walked straight back
    // into it here: 1,349 layouts parsed where the corpus has 748. Two views of one dataset must
    // not disagree by 2x.
    std::vector<std::string> pkgs;
    std::set<std::string> seen;
    for (const auto& sp : fs::search_paths()) {
        for (const auto& path : pkg::find_packages(sp.dir.string())) {
            std::error_code ec;
            std::string key = std::filesystem::weakly_canonical(path, ec).string();
            if (ec) key = path;
            if (seen.insert(key).second) pkgs.push_back(path);
        }
    }
    for (const auto& path : pkgs) {
        if (m_layouts.size() >= limit) break;
        pkg::Package pk;
        if (!pk.open(path)) continue;
        const auto& recs = pk.records();
        std::string pname = std::filesystem::path(path).filename().string();
        for (u32 n = 0; n < recs.size() && m_layouts.size() < limit; ++n) {
            const pkg::Record& r = recs[n];
            if (r.type() != kSpuiType) continue;
            std::vector<u8> bytes;
            if (!pk.read_resource_decoded(n, bytes)) continue;
            ++scanned;
            Layout L;
            L.package = pname;
            L.record = n;
            L.instance = r.instance();
            parse_layout(bytes, L);
            if (L.windows.empty()) continue;   // nothing readable: do not pretend it is a screen
            // PRECEDENCE, EXPLICIT. The corpus holds 748 `spui` RESOURCES but only 524 distinct
            // INSTANCES, so 224 layouts ship more than once - E3LaunchScreen is in Data/PatchData at
            // 4,486 B and in Data/Spore_Graphics at 4,480 B. The FIRST container on the search path
            // wins, because that is what mounting a patch ahead of the content packages MEANS:
            // PatchData's job is to override base content. Before this, find() returned whichever
            // copy the traversal happened to reach first - the same accident of traversal order that
            // I just removed from text.cpp's locale precedence, re-introduced in a different
            // subsystem. A rule has to be stated once, in one place, or it drifts.
            const size_t idx = m_layouts.size();
            auto ins = m_by_instance.emplace(L.instance, idx);
            if (!ins.second) {
                ++m_shadowed;
                const Layout& keeper = m_layouts[ins.first->second];
                if (keeper.bytes != L.bytes) {
                    if (m_shadowed_diff < 6) {
                        LOG_INFO("spui", "layout 0x%08X: %s (%u B) shadowed by %s (%u B) - the "
                                         "earlier container wins", L.instance, pname.c_str(),
                                 L.bytes, keeper.package.c_str(), keeper.bytes);
                    }
                    ++m_shadowed_diff;
                }
            } else {
                ++m_distinct;
            }
            m_layouts.push_back(std::move(L));
        }
    }
    m_loaded = true;
    size_t wins = 0, areas = 0, anchors = 0;
    size_t caps_inline = 0, caps_ref_key = 0, caps_ref_nokey = 0;
    for (const Layout& L : m_layouts) {
        wins += L.windows.size();
        anchors += L.anchors;
        for (const Window& w : L.windows) {
            areas += w.has_area ? 1 : 0;
            // Count the THREE caption shapes, because the coverage of the layout->text chain is the
            // number that matters here: an inline string is already on screen, a (table,key) pair
            // resolves through engine/text.cpp, and a table-only record cannot be resolved at all
            // without the game's own selection rule. Reporting them together would hide the gap.
            if (!w.caption.empty()) ++caps_inline;
            else if (w.caption_is_ref && w.caption_key_seen) ++caps_ref_key;
            else if (w.caption_is_ref) ++caps_ref_nokey;
        }
    }
    LOG_INFO("spui", "parsed %zu layouts from %zu %s (%zu distinct instances, %zu shadowed by an "
                     "earlier container, %zu of those with a different size): %zu windows, %zu with "
                     "a usable Area (%zu anchors read, %d schema ids); captions: %zu inline, "
                     "%zu (table,key), %zu table-only",
             m_layouts.size(), scanned, scanned == 1 ? "resource" : "resources", m_distinct,
             m_shadowed, m_shadowed_diff, wins, areas, anchors, kSchemaCount,
             caps_inline, caps_ref_key, caps_ref_nokey);
}

const Layout* Reader::find(u32 inst) const {
    auto it = m_by_instance.find(inst);
    return it == m_by_instance.end() ? nullptr : &m_layouts[it->second];
}

const Layout* Reader::find_by_name(const std::string& name) const {
    // A state script names a layout in plain text, and the resource INSTANCE is that name run
    // through the engine's own hash - the same rule that names a locale table and a nested package.
    // Measured: E3LaunchScreen -> 0xE932BD44 (a real spui record, in two containers),
    // LaunchScreen -> 0x562AB2CC (one record). Nothing is guessed here: if the hash does not land on
    // an instance, the answer is null, and the caller says so rather than inventing a screen.
    if (name.empty()) return nullptr;
    return find(naming::hash_lower(name.c_str()));
}

const Layout* Reader::best_screen() const {
    const Layout* best = nullptr;
    size_t best_areas = 0;
    for (const Layout& L : m_layouts) {
        size_t a = 0;
        for (const Window& w : L.windows) a += w.has_area ? 1 : 0;
        if (a > best_areas) { best_areas = a; best = &L; }
    }
    return best;
}

std::string Reader::stats() const {
    char buf[256];
    const Layout* b = best_screen();
    std::snprintf(buf, sizeof(buf), "spui: %zu layouts, best = %s (%zu with Area)", m_layouts.size(),
                  b ? b->name().c_str() : "none",
                  b ? [&] { size_t n = 0; for (const Window& w : b->windows) n += w.has_area; return n; }()
                    : 0);
    return buf;
}

Reader& instance() {
    static Reader r;
    return r;
}

const char* schema_name(u32 id) {
    ensure_schema();
    const SchemaEntry* e = g_schema.by_id(id);
    return e ? e->name : nullptr;
}
const char* schema_type(u32 id) {
    ensure_schema();
    const SchemaEntry* e = g_schema.by_id(id);
    return e ? e->type : nullptr;
}
u16 schema_code(u32 id) {
    ensure_schema();
    const SchemaEntry* e = g_schema.by_id(id);
    return e ? e->code : 0xFFFF;
}

}  // namespace os::spui
