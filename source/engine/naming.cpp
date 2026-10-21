#include "engine/naming.h"

#include <cstdio>
#include <filesystem>
#include <unordered_map>
#include <vector>

#include "engine/log.h"

namespace os::naming {
namespace {

std::unordered_map<u32, std::string> g_instances;
std::unordered_map<u32, std::string> g_types;
std::unordered_map<u32, std::string> g_properties;
bool g_loaded = false;

bool read_lines(const std::filesystem::path& p, std::vector<std::string>& out) {
    FILE* f = nullptr;
    if (fopen_s(&f, p.string().c_str(), "rb") != 0 || !f) return false;
    std::string cur;
    int ch;
    while ((ch = fgetc(f)) != EOF) {
        if (ch == '\n') {
            out.push_back(std::move(cur));
            cur.clear();
        } else if (ch != '\r') {
            cur.push_back((char)ch);
        }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    fclose(f);
    return true;
}

std::filesystem::path find_file(const char* name) {
    namespace fs = std::filesystem;
    std::vector<fs::path> cands = {
        fs::current_path() / "upstream" / "SporeMaster3" / "data" / name,
        fs::current_path() / "data" / name,
        fs::current_path() / name,
    };
    // also walk up a few levels, so running from build/bin/Debug finds the repo copy
    fs::path up = fs::current_path();
    for (int i = 0; i < 4 && up.has_parent_path(); ++i) {
        up = up.parent_path();
        cands.push_back(up / "upstream" / "SporeMaster3" / "data" / name);
        cands.push_back(up / "data" / name);
    }
    std::error_code ec;
    for (auto& c : cands) {
        if (fs::is_regular_file(c, ec)) return c;
    }
    return {};
}

void parse_registry(const std::filesystem::path& p, std::unordered_map<u32, std::string>& dst) {
    std::vector<std::string> lines;
    if (!read_lines(p, lines)) return;
    for (auto& ln : lines) {
        if (ln.empty()) continue;
        const size_t tab = ln.find('\t');
        if (tab != std::string::npos) {
            std::string h = ln.substr(tab + 1);
            while (!h.empty() && (h.back() == ' ' || h.back() == '\t')) h.pop_back();
            if (h.size() > 2 && (h[0] == '0') && (h[1] == 'x' || h[1] == 'X')) {
                try {
                    u32 id = (u32)std::stoul(h, nullptr, 16);
                    dst.emplace(id, ln.substr(0, tab));
                } catch (...) {
                }
            }
            continue;
        }
        dst.emplace(hash_lower(ln.c_str()), ln);
    }
}

}  // namespace

u32 hash_lower(const char* s) {
    // FNV-1, tolower: h = (h * prime) ^ c, per 0x00932e50 (imul before xor, mode 1).
    u32 h = 0x811c9dc5u;
    for (const unsigned char* p = (const unsigned char*)s; *p; ++p) {
        u32 c = *p;
        if (c >= 'A' && c <= 'Z') c += 32;
        h = ((h * 0x01000193u) & 0xFFFFFFFFu) ^ c;
    }
    return h;
}

u32 load() {
    if (g_loaded) return (u32)(g_instances.size() + g_types.size());
    g_loaded = true;
    auto rf = find_file("reg_file.txt");
    auto rt = find_file("reg_type.txt");
    auto rp = find_file("reg_property.txt");
    if (!rf.empty()) parse_registry(rf, g_instances);
    if (!rt.empty()) parse_registry(rt, g_types);
    if (!rp.empty()) parse_registry(rp, g_properties);
    LOG_INFO("naming", "registry: %zu instance/group names, %zu type names, %zu property names (source: %s)",
             g_instances.size(), g_types.size(), g_properties.size(),
             rf.empty() ? "<none>" : rf.string().c_str());
    return (u32)(g_instances.size() + g_types.size() + g_properties.size());
}

bool loaded() { return g_loaded; }

const char* instance_name(u32 id) {
    auto it = g_instances.find(id);
    return it == g_instances.end() ? nullptr : it->second.c_str();
}

const char* type_name(u32 id) {
    auto it = g_types.find(id);
    return it == g_types.end() ? nullptr : it->second.c_str();
}

const char* property_name(u32 id) {
    auto it = g_properties.find(id);
    return it == g_properties.end() ? nullptr : it->second.c_str();
}

std::string resource_path(u32 type, u32 group, u32 instance) {
    char b[256];
    const char* tn = type_name(type);
    const char* in = instance_name(instance);
    const char* gn = instance_name(group);
    if (gn && in && tn) {
        _snprintf_s(b, sizeof(b), _TRUNCATE, "%s/%s.%s", gn, in, tn);
    } else if (in && tn) {
        _snprintf_s(b, sizeof(b), _TRUNCATE, "%s.%s", in, tn);
    } else {
        _snprintf_s(b, sizeof(b), _TRUNCATE, "0x%08x/0x%08x.%s", group, instance, tn ? tn : "");
    }
    return b;
}

std::string stats() {
    char b[160];
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%zu instance/group names, %zu type names",
                g_instances.size(), g_types.size());
    return b;
}

}  // namespace os::naming
