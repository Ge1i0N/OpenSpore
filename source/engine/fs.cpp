#include "engine/fs.h"

#include <windows.h>

#include <set>

#include "engine/log.h"

namespace os::fs {
namespace {

std::vector<DirEntry> g_paths;
std::filesystem::path g_user_data;
std::filesystem::path g_exe_dir;
std::set<std::string> g_missed;  // log each missing relative path once (consolidated)

// The original's own prefixes, verbatim from .rdata (see header for addresses).
constexpr const char* kUserData = "UserData";
constexpr const char* kBaseData = "BaseData";
constexpr const char* kLocale = "Locale";
constexpr const char* kConfig = "Config";
constexpr const char* kData = "Data";

bool ci_file_exists(const std::filesystem::path& p) {
    DWORD attr = GetFileAttributesW(p.wstring().c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::filesystem::path ci_find(const std::filesystem::path& dir, const std::string& name) {
    // Case-insensitive single-component lookup: game paths ship in mixed case and the
    // original's own resolver is case-insensitive (it lowercases via _strlwr imports).
    if (!std::filesystem::exists(dir)) return {};
    std::string want = name;
    for (auto& c : want) c = (char)tolower((unsigned char)c);
    std::error_code ec;
    for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (!e.is_regular_file()) continue;
        std::string cand = e.path().filename().string();
        for (auto& c : cand) c = (char)tolower((unsigned char)c);
        if (cand == want) return e.path();
    }
    return {};
}

}  // namespace

void init(const Args& args, const std::filesystem::path& exe_dir) {
    g_exe_dir = exe_dir;
    g_paths.clear();

    const bool dev = args.use_dev_dirs();  // [obj+0x159] analog: shipDirs wins

    // userDataDir / dataDir / dataBaseDir overrides (keys verified in arg_vocab.md)
    std::filesystem::path user_data = args.user_data_dir.empty() ? (exe_dir / kUserData)
                                                                 : std::filesystem::path(args.user_data_dir);
    g_user_data = user_data;
    std::error_code ec;
    std::filesystem::create_directories(user_data, ec);
    if (ec) LOG_WARN("fs", "cannot create UserData dir %s: %s", user_data.string().c_str(), ec.message().c_str());

    auto add = [&](const std::filesystem::path& p, const char* tag) {
        if (std::filesystem::is_directory(p, ec) || dev) g_paths.push_back({p, tag});
    };

    if (dev) {
        // Dev mode: loose dev directories take priority, exactly the point of -devDirs.
        add(exe_dir / kData, "Data");
        add(exe_dir / kBaseData, "BaseData");
        std::filesystem::path loc = exe_dir / kLocale / (args.locale.empty() ? "en-US" : args.locale);
        add(loc, "Locale");
        add(exe_dir / kConfig, "Config");
        add(user_data, "UserData");
        add(exe_dir, "cwd");
    } else {
        // Ship mode: UserData (writable overlays) first, then the packaged data roots.
        add(user_data, "UserData");
        add(exe_dir / kData, "Data");
        add(exe_dir / kBaseData, "BaseData");
        std::filesystem::path loc = exe_dir / kLocale / (args.locale.empty() ? "en-US" : args.locale);
        add(loc, "Locale");
        add(exe_dir / kConfig, "Config");
        add(exe_dir, "cwd");
    }

    LOG_INFO("fs", "search mode=%s (devDirs=%d shipDirs=%d), %zu paths", dev ? "DEV" : "SHIP", (int)args.dev_dirs,
             (int)args.ship_dirs, g_paths.size());
    LOG_DEBUG("fs", "%s", dump_search_list().c_str());
}

const std::vector<DirEntry>& search_paths() { return g_paths; }

std::filesystem::path resolve(const std::string& rel) {
    // rel may contain '/' separators; resolve component-wise case-insensitively.
    std::vector<std::string> comps;
    size_t start = 0;
    for (size_t i = 0; i <= rel.size(); ++i) {
        if (i == rel.size() || rel[i] == '/' || rel[i] == '\\') {
            if (i > start) comps.push_back(rel.substr(start, i - start));
            start = i + 1;
        }
    }
    if (comps.empty()) return {};
    const std::string& file = comps.back();
    for (auto& sp : g_paths) {
        std::filesystem::path dir = sp.dir;
        bool ok = true;
        for (size_t i = 0; i + 1 < comps.size(); ++i) {
            std::filesystem::path nxt = dir / comps[i];
            if (!std::filesystem::is_directory(nxt)) {
                // try case-insensitive directory match
                std::error_code ec;
                bool found = false;
                for (auto& e : std::filesystem::directory_iterator(dir, ec)) {
                    if (!e.is_directory()) continue;
                    std::string c = e.path().filename().string(), w = comps[i];
                    for (auto& x : c) x = (char)tolower((unsigned char)x);
                    for (auto& x : w) x = (char)tolower((unsigned char)x);
                    if (c == w) {
                        nxt = e.path();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    ok = false;
                    break;
                }
            }
            dir = nxt;
        }
        if (!ok) continue;
        std::filesystem::path hit = ci_find(dir, file);
        if (!hit.empty()) return hit;
        std::filesystem::path direct = dir / file;
        if (ci_file_exists(direct)) return direct;
    }
    if (g_missed.insert(rel).second) {
        LOG_WARN("fs", "MISS %s (searched %zu paths: %s)", rel.c_str(), g_paths.size(), dump_search_list().c_str());
    }
    return {};
}

bool exists(const std::string& rel) { return !resolve(rel).empty(); }

std::filesystem::path user_data_dir() { return g_user_data; }

std::filesystem::path log_dir() { return g_exe_dir; }

std::string dump_search_list() {
    std::string s;
    for (auto& sp : g_paths) {
        if (!s.empty()) s += "; ";
        s += sp.tag;
        s += "=";
        s += sp.dir.string();
    }
    if (s.empty()) s = "(empty)";
    return s;
}

}  // namespace os::fs
