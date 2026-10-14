#include "engine/spui.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <functional>
#include <set>
#include <sstream>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"
#include "engine/spui_schema.inc"

namespace os::spui {
namespace {
constexpr u32 kSpuiType = 0x250fe9a2;

const SchemaEntry* schema(u32 id) {
    const auto* begin = std::begin(kSchema);
    const auto* end = std::end(kSchema);
    const auto* it = std::lower_bound(begin, end, id,
        [](const SchemaEntry& entry, u32 key) { return entry.id < key; });
    return it != end && it->id == id ? it : nullptr;
}

std::vector<std::string> packages() {
    std::vector<std::string> paths;
    std::set<std::string> seen;
    for (const auto& sp : fs::search_paths()) {
        for (const auto& path : pkg::find_packages(sp.dir.string())) {
            std::error_code ec;
            std::string key = std::filesystem::weakly_canonical(path, ec).string();
            if (ec) key = path;
            if (seen.insert(key).second) paths.push_back(path);
        }
    }
    return paths;
}
}  // namespace

std::string Layout::name() const {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s rec[%u] inst=0x%08X %uB %zu windows",
                  package.c_str(), record, instance, bytes, windows.size());
    return buf;
}

void Reader::load(size_t limit) {
    m_loaded = false;
    m_layouts.clear();
    m_by_instance.clear();
    m_shadowed = m_shadowed_diff = m_distinct = 0;
    if (limit == 0) limit = SIZE_MAX;
    size_t scanned = 0, failed = 0;
    for (const auto& path : packages()) {
        if (scanned >= limit) break;
        pkg::Package pk;
        if (!pk.open(path)) continue;
        const auto& recs = pk.records();
        const std::string pname = std::filesystem::path(path).filename().string();
        for (u32 n = 0; n < recs.size() && scanned < limit; ++n) {
            const pkg::Record& r = recs[n];
            if (r.type() != kSpuiType) continue;
            ++scanned;
            std::vector<u8> bytes;
            if (!pk.read_resource_decoded(n, bytes)) { ++failed; continue; }
            Layout layout;
            layout.package = pname;
            layout.record = n;
            layout.instance = r.instance();
            std::string error;
            if (!parse_binary(bytes, layout, error)) {
                ++failed;
                LOG_ERROR("spui", "%s rec[%u]: %s", pname.c_str(), n, error.c_str());
                continue;
            }
            for (const Object& obj : layout.objects)
                for (const Property& p : obj.properties) if (schema(p.id)) ++layout.recognized;
            const auto ins = m_by_instance.emplace(layout.instance, m_layouts.size());
            if (ins.second) ++m_distinct;
            else {
                ++m_shadowed;
                if (m_layouts[ins.first->second].bytes != layout.bytes) ++m_shadowed_diff;
            }
            m_layouts.push_back(std::move(layout));
        }
    }
    size_t objects = 0, windows = 0, areas = 0, props = 0, multi = 0, links = 0;
    size_t inline_caps = 0, refs = 0;
    for (const Layout& layout : m_layouts) {
        objects += layout.objects.size();
        windows += layout.windows.size();
        props += layout.anchors;
        multi += layout.multi;
        links += layout.child_links;
        for (const Window& win : layout.windows) {
            areas += win.has_area ? 1 : 0;
            inline_caps += !win.caption.empty() ? 1 : 0;
            refs += win.caption_is_ref ? 1 : 0;
        }
    }
    m_loaded = failed == 0;
    LOG_ONCE_INFO("spui", "sequential parse %zu/%zu resources, %zu failed: %zu objects, %zu windows, "
                  "%zu Areas, %zu properties (%zu multi), %zu child links; %zu distinct instances, "
                  "%zu shadowed (%zu size differs); captions %zu inline / %zu references",
                  m_layouts.size(), scanned, failed, objects, windows, areas, props, multi, links,
                  m_distinct, m_shadowed, m_shadowed_diff, inline_caps, refs);
}

const Layout* Reader::find(u32 inst) const {
    const auto it = m_by_instance.find(inst);
    return it == m_by_instance.end() ? nullptr : &m_layouts[it->second];
}

const Layout* Reader::find_by_name(const std::string& name) const {
    return name.empty() ? nullptr : find(naming::hash_lower(name.c_str()));
}

const Layout* Reader::best_screen() const {
    const Layout* best = nullptr;
    size_t best_areas = 0;
    for (const Layout& layout : m_layouts) {
        size_t areas = 0;
        for (const Window& win : layout.windows) areas += win.has_area ? 1 : 0;
        if (areas > best_areas) { best_areas = areas; best = &layout; }
    }
    return best;
}

std::string Reader::stats() const {
    char buf[280];
    const Layout* best = best_screen();
    std::snprintf(buf, sizeof(buf), "spui: %zu sequential layouts, %zu distinct; best=%s",
                  m_layouts.size(), m_distinct, best ? best->name().c_str() : "none");
    return buf;
}

Reader& instance() {
    static Reader reader;
    return reader;
}

std::string dump_anchors(u32 inst, size_t maxn) {
    // Compatibility verb; now emits exact framed properties, not heuristic anchors. The limit keeps
    // output below the logger's message buffer; use --parse --output in spui_objects.py for a corpus.
    const Layout* layout = instance().find(inst);
    if (!layout) return "dump: layout not loaded";
    std::vector<Property> props;
    for (const Object& obj : layout->objects)
        props.insert(props.end(), obj.properties.begin(), obj.properties.end());
    std::sort(props.begin(), props.end(), [](const Property& a, const Property& b) {
        return a.offset < b.offset;
    });
    std::ostringstream out;
    out << props.size() << " exact properties of " << layout->name();
    for (size_t i = 0; i < std::min(maxn, props.size()); ++i) {
        const Property& p = props[i];
        char buf[160];
        std::snprintf(buf, sizeof(buf), "\n  off=%u end=%u depth=%u id=0x%08X code=%u count=%u %s",
                      p.offset, p.end, p.depth, p.id, p.code, p.count,
                      schema_name(p.id) ? schema_name(p.id) : "(unknown name)");
        if (out.str().size() + std::strlen(buf) > 1700) { out << "\n  ..."; break; }
        out << buf;
    }
    return out.str();
}

std::string describe_tree(const Layout& layout) {
    size_t visible = 0;
    for (const Window& win : layout.windows) visible += win.visible ? 1 : 0;
    float w = 0.f, h = 0.f;
    layout.design_resolution(w, h);
    std::ostringstream out;
    out << "tree: " << layout.objects.size() << " objects / " << layout.windows.size()
        << " windows / " << layout.child_links << " links / " << layout.roots.size()
        << " roots; visible=" << visible << "; design=" << w << 'x' << h
        << "; consumed=" << layout.consumed << '/' << layout.bytes;
    size_t shown = 0;
    std::function<void(size_t, size_t)> visit = [&](size_t n, size_t depth) {
        if (shown >= 12) return;
        const Window& win = layout.windows[n];
        char line[190];
        std::snprintf(line, sizeof(line), "\n  depth=%zu object=%u parent=%d control=0x%08X children=%zu visible=%d",
                      depth, win.object_index,
                      win.parent >= 0 ? (int)layout.windows[(size_t)win.parent].object_index : -1,
                      win.control_id, win.children.size(), win.visible ? 1 : 0);
        out << line;
        ++shown;
        for (size_t child : win.children) visit(child, depth + 1);
    };
    for (size_t root : layout.roots) visit(root, 0);
    return out.str();
}

const char* schema_name(u32 id) {
    const SchemaEntry* entry = schema(id);
    return entry ? entry->name : nullptr;
}
const char* schema_type(u32 id) {
    const SchemaEntry* entry = schema(id);
    return entry ? entry->type : nullptr;
}
u16 schema_code(u32 id) {
    const SchemaEntry* entry = schema(id);
    return entry ? entry->code : 0xffff;
}
}  // namespace os::spui
