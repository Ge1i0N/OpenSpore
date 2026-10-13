#pragma once
// The game's own config scripts (Config/*.txt) - OUR reader.
//
// Authority: the shipped files `Data/Config/{Properties.txt,ConfigManager.txt,VideoCards.txt}` and
// `DataEP1/Config/...` (loose text), which the disassembly loads via the resource manager as the
// `properties` / `ConfigManager` groups (see analysis/content-loading.md). This is a small
// imperative scripting language:
//   property <Name> <0xid> <type>      (Properties.txt: the game's option table)
//   set/seti/setb <Name> <value>       (ConfigManager.txt: variables)
//   setOption <Name> <value>           (ConfigManager.txt: options)
//   setResolution <w> <h>
//   if / elseif / else / endif, trace, include, alert, boolProp, intProp, setVariables
//
// IMPORTANT: the option ids in Properties.txt are a MIX - only some are FNV-1-lower of the name,
// the rest are explicitly assigned. So this reader takes the id FROM THE FILE; it never hashes.
//
// Conditionals are hardware-dependent (video card, CPU, sound). We do NOT evaluate them; we record
// every assignment (last one wins) and log that conditionals were skipped, so nothing is guessed.
#include <map>
#include <string>
#include <vector>

#include "common.h"

namespace os::config {

struct Property {
    std::string name;
    u32 id = 0;
    std::string type;
};

class Config {
public:
    // Reads Config/Properties.txt and Config/ConfigManager.txt through the fs search paths.
    // Never throws; missing files are logged and simply produce an empty result.
    void load();

    bool loaded() const { return m_loaded; }
    const std::vector<Property>& properties() const { return m_props; }
    const std::map<std::string, std::string>& vars() const { return m_vars; }
    const std::string& source_properties() const { return m_src_props; }
    const std::string& source_manager() const { return m_src_mgr; }
    int conditional_lines_skipped() const { return m_cond_skipped; }
    // VideoCards.txt: vendor id -> name, card id -> name.
    size_t vendor_count() const { return m_vendors.size(); }
    size_t card_count() const { return m_cards.size(); }
    std::string card_name(u32 id) const;
    std::string vendor_name(u32 id) const;

    // Look up a variable (case-insensitive name), or "" if absent.
    std::string get(const std::string& name) const;
    int get_int(const std::string& name, int def) const;

    std::string stats() const;

private:
    bool m_loaded = false;
    std::vector<Property> m_props;
    std::map<std::string, std::string> m_vars;  // lowercased name -> value
    std::map<u32, std::string> m_vendors;       // VideoCards.txt
    std::map<u32, std::string> m_cards;
    std::string m_src_props, m_src_mgr;
    int m_cond_skipped = 0;
};

Config& instance();

}  // namespace os::config
