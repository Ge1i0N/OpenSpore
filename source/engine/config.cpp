#include "engine/config.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "engine/fs.h"
#include "engine/log.h"

namespace os::config {
namespace {

Config g_config;

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

// Find `<root>/Config/<name>` across the fs search paths (Data, DataEP1, ...).
std::filesystem::path find_config(const std::string& name, std::string& where) {
    for (const auto& sp : fs::search_paths()) {
        std::filesystem::path p = sp.dir / "Config" / name;
        std::error_code ec;
        if (std::filesystem::exists(p, ec)) {
            where = p.string();
            return p;
        }
    }
    return {};
}

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream is(line);
    std::string t;
    while (is >> t) out.push_back(t);
    return out;
}

std::string strip_comment(const std::string& line) {
    auto h = line.find('#');
    return h == std::string::npos ? line : line.substr(0, h);
}

// A property id comes in three forms in Properties.txt:
//   0x04754439            explicit hex
//   41                    decimal
//   (hash(mastervolume))  FNV-1-lower of the INNER name (which may differ from the property name,
//                         e.g. `property AudioMasterVolume (hash(mastervolume))`)
// We resolve all three; the `(hash(...))` form is why we must not just hash the property name.
u32 parse_id(const std::string& tok) {
    if (tok.rfind("(hash(", 0) == 0) {
        auto end = tok.find(')');
        if (end == std::string::npos) end = tok.size();
        std::string inner = tok.substr(6, end - 6);  // between "(hash(" and the first ')'
        return fnv1_lower(inner.c_str());
    }
    return (u32)std::strtoul(tok.c_str(), nullptr, 0);
}

}  // namespace

std::string Config::get(const std::string& name) const {
    auto it = m_vars.find(lower(name));
    return it == m_vars.end() ? std::string() : it->second;
}

int Config::get_int(const std::string& name, int def) const {
    std::string v = get(name);
    if (v.empty()) return def;
    try {
        return std::stoi(v, nullptr, 0);  // base 0: accepts 0x.. too
    } catch (...) {
        return def;
    }
}

std::string Config::stats() const {
    char b[192];
    _snprintf_s(b, sizeof(b), _TRUNCATE,
                "config: %zu properties, %zu variables from Config/*.txt (%s)",
                m_props.size(), m_vars.size(),
                m_src_mgr.empty() ? (m_src_props.empty() ? "none found" : "Properties.txt only")
                                  : "Properties.txt + ConfigManager.txt");
    return b;
}

void Config::load() {
    m_loaded = true;

    // 1. Properties.txt -> the option table
    std::string wp;
    auto pp = find_config("Properties.txt", wp);
    if (!pp.empty()) {
        m_src_props = wp;
        std::ifstream f(pp);
        std::string line;
        while (std::getline(f, line)) {
            auto toks = tokenize(strip_comment(line));
            if (toks.size() >= 3 && toks[0] == "property") {
                Property pr;
                pr.name = toks[1];
                pr.id = parse_id(toks[2]);
                pr.type = toks.size() >= 4 ? toks[3] : "";
                m_props.push_back(pr);
            }
        }
        LOG_INFO("config", "Properties.txt: %zu options (%s)", m_props.size(), wp.c_str());
    } else {
        LOG_WARN("config", "Config/Properties.txt not found in the search paths");
    }

    // 2. ConfigManager.txt -> variables / options (conditionals recorded but not evaluated)
    std::string wm;
    auto mp = find_config("ConfigManager.txt", wm);
    if (!mp.empty()) {
        m_src_mgr = wm;
        std::ifstream f(mp);
        std::string line;
        int assigns = 0;
        while (std::getline(f, line)) {
            auto toks = tokenize(strip_comment(line));
            if (toks.empty()) continue;
            const std::string& kw = toks[0];
            if (kw == "if" || kw == "elseif" || kw == "else" || kw == "endif") {
                ++m_cond_skipped;
                continue;
            }
            if ((kw == "set" || kw == "seti" || kw == "setb" || kw == "setOption") && toks.size() >= 3) {
                // value = everything after the name, joined (e.g. "Low", "1", "1920 1080")
                std::string val = toks[2];
                for (size_t i = 3; i < toks.size(); ++i) val += " " + toks[i];
                m_vars[lower(toks[1])] = val;
                ++assigns;
            } else if (kw == "setResolution" && toks.size() >= 3) {
                m_vars["setresolution"] = toks[1] + " " + toks[2];
                ++assigns;
            }
        }
        LOG_INFO("config", "ConfigManager.txt: %d assignments, %d conditional lines skipped (%s)",
                 assigns, m_cond_skipped, wm.c_str());
    } else {
        LOG_WARN("config", "Config/ConfigManager.txt not found in the search paths");
    }

    LOG_INFO("config", "%s", stats().c_str());
}

Config& instance() { return g_config; }

}  // namespace os::config
