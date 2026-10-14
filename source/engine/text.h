#pragma once
// The game's localized text tables (`locale` resources) - OUR reader.
//
// Authority #1 (what the shipped data actually is), measured over all 15 locale directories:
//   * Type id 0x02FAC0B6 is `locale` (analysis/typeids from reg_type.txt). 2,944 locale resources
//     exist game-wide; 200 of them are in en-us, spread over Text.package plus others.
//   * Payload = UTF-8 WITH A BOM (EF BB BF), records separated by CRLF. Verified on the raw first
//     bytes of a record: `ef bb bf 30 78 30 30 30 30 30 42 32 31 20 7e 67 75 69 64 65 3a`
//     == BOM + "0x00000B21 ~guide:".
//   * Grammar, one record per line:
//         0x<8 hex digits> [<markup>] <text>
//     where <markup> is an OPTIONAL `~...~` token immediately after the key (e.g. `~guide: -h2~`,
//     `~mission_target_planet~`, `~TOKEN1~`). 0 of 18,439 key lines had an unterminated `~`.
//   * Lines starting with `#` are comments (5,434 of them). One record (instance 0x04DC2F94,
//     1,644 keys) begins with `# WARNING: This is an assembled content string table. It is
//     auto-generated.` - the game's own tooling says so in prose.
//   * Continuation lines exist but are rare: 17 of 31,209 lines are neither key, comment, nor blank.
//
// Authority #1b: THE KEY SPACE IS PER-TABLE, NOT GLOBAL. This is the fact that decides the API:
//   * Within any single locale resource, a key appears AT MOST ONCE (measured: 0 tables out of 200
//     contain a repeated key).
//   * Across tables, low keys collide heavily and high keys almost not at all:
//        0x00000000..0x00000FFF  1,752 keys   36.2% ambiguous
//        0x00001000..0x0000FFFF   105 keys     1.9%
//        0x00010000..0x000FFFFF   180 keys     0.0%
//        0x01000000..0xFFFFFFFF 8,187 keys     0.5-0.7%
//     So `0x00000011` means "Build Mode" in one table and something else entirely in another, while
//     `0x05B08767` means "New Game" everywhere. Hence get() refuses to answer an ambiguous key
//     instead of returning whichever table happened to be parsed first - a silent wrong string in a
//     menu is worse than no string.
//
// Real labels recovered (en-us), which is the point of all this - a menu needs these, not our prose:
//   0x05B08767 "New Game"   0x05B08733 "Play"      0x00010009 "Options"   0x00001611 "Settings"
//   0x045BEDD8 "Exit"       0x045BEDD9 "Quit"      0x045BEDD4 "Cancel"    0x045BEDD1 "Save"
//   0x058C8702 "Load"       0x05B08747 "Back"      0x06162C25 "Ok"        0x0473EF30 "Credits"
//   0x0549656C "Continue"   0x058B5C13 "Creator"   0x05B08735 "Creature"  0x0630D565 "Spaceship"
//   0x0549366D "Upload"     0x045FFE89 "Help"
// Note 0x045BEDD1/D4/D8/D9 are a CONTIGUOUS cluster of four button labels (Save/Cancel/Exit/Quit),
// which is evidence that UI text keys are allocated in per-widget-group runs - a hint for reading
// the `spui` layouts once we get there.
//
// NOT reversed here: which key belongs to which widget. That mapping lives in the `spui` layout,
// whose records are BYTE-PACKED (id | code:u16 | marker:u16 | payload) - the earlier "bit-packed"
// reading was retracted the day the designer schema was found; see analysis/spui-schema.md. So this
// table supplies the STRINGS for a real menu, and spui supplies the geometry and the (table,key)
// references. Display is ASCII-only for now (our overlay font is a 32..126 GDI atlas), so non-ASCII
// bytes are reported, not silently mangled.
#include <map>
#include <string>
#include <vector>

#include "common.h"

namespace os::pkg {
class Package;
}  // namespace os::pkg

namespace os::text {

struct Entry {
    u32 key = 0;
    u32 table = 0;         // the locale resource's instance id
    std::string markup;    // the `~...~` token without the tildes, or ""
    std::string text;      // UTF-8
    // Which container produced this entry (an index into Tables::sources()). A (table,key) pair can
    // legitimately arrive from two containers - Data/PatchData.package and the EP1 locale packages
    // both ship table 0xADE7FFAA - so the source is what turns "which string won" from an accident
    // of traversal order into something we can state, log, and later match against the original's
    // own resource-cache precedence.
    u32 src = 0;
};

struct Hit {
    u32 key = 0;
    u32 table = 0;
    std::string text;
    std::string markup;
};

class Tables {
public:
    // Parses every `locale` resource under the Locale/<lang> search paths. Never throws; a
    // missing locale directory yields an empty table set and a log line.
    bool load(const std::string& lang);

    bool loaded() const { return m_loaded; }
    const std::string& lang() const { return m_lang; }
    size_t table_count() const { return m_tables.size(); }
    size_t entry_count() const { return m_entries.size(); }
    size_t distinct_keys() const { return m_index.size(); }
    size_t ambiguous_keys() const { return m_ambiguous; }
    size_t comment_lines() const { return m_comments; }
    size_t continuation_lines() const { return m_continuations; }
    size_t non_ascii() const { return m_non_ascii; }
    // Containers that contributed text, in the order they were parsed. See Entry::src.
    const std::vector<std::string>& sources() const { return m_sources; }
    // How many (table,key) pairs were defined more than once, split by whether the two definitions
    // AGREE. The disagreeing ones are the real precedence question; the agreeing ones are harmless
    // duplication (e.g. en-us is shipped byte-identically in two EP1 containers).
    size_t def_duplicates() const { return m_dup_same + m_dup_diff; }
    size_t def_duplicates_same() const { return m_dup_same; }
    size_t def_duplicates_diff() const { return m_dup_diff; }

    // The globally-unique answer for a key, or "" when the key is absent or ambiguous.
    std::string get(u32 key) const;
    std::string markup(u32 key) const;
    // The exact answer from one table. NOTE the historical trap here: this map was once declared
    // std::map<u32,...> while callers packed (table<<32)|key into a u64, so the implicit narrowing
    // truncated the TABLE away and get(table,key) silently became get(key) - returning another
    // table's string for a menu caption, which is exactly the failure this overload exists to
    // prevent. The giveaway was the load() log line: "distinct (table,key) texts" equal to
    // "distinct keys" can only happen if the table half is not participating in the lookup.
    std::string get(u32 table, u32 key) const;
    bool has(u32 key) const { return m_index.count(key) != 0; }
    bool ambiguous(u32 key) const;
    // Is this TABLE instance in the corpus at all? A caption reference that fails to resolve is a
    // COVERAGE gap when the answer is no (the text is not in the tree) and a SEMANTIC gap when the
    // table is present but the key is not. Conflating the two hid real progress: for two sessions
    // we recorded "the mvj and legal tables are missing", and `legal` does ship - inside the nested
    // locale packages of Data/PatchData.package.
    bool has_table(u32 table) const;
    // Is this exact (table,key) pair defined? Distinct from get() != "" only in intent, but the
    // distinction matters for an empty string that is a legitimate translation.
    bool has_in(u32 table, u32 key) const;

    // Case-insensitive reverse lookup: which keys carry this label. This is how a menu asks for
    // "New Game" without knowing its key.
    std::vector<Hit> find_label(const std::string& label, size_t limit = 32) const;

    const std::vector<Entry>& entries() const { return m_entries; }
    std::string stats() const;

private:
    struct Index {
        std::vector<size_t> entry_ids;  // into m_entries
        bool agrees = true;             // all texts identical
    };
    // Parse every `locale` resource of ONE package into the shared tables, and return how many tables
    // it yielded. Shared by the flat and the NESTED pass on purpose: the EP1 text lives in nested
    // packages, and a second copy of this grammar would be exactly the drift that produces two
    // disagreeing views of one dataset.
    size_t parse_package(pkg::Package& pk);
    // Interns a container label and returns its id, so Entry::src stays a u32 instead of a string
    // copy per entry (32,038 entries x a path each would cost more than the text itself).
    u32 source_id(const std::string& label);
    bool m_loaded = false;
    std::string m_lang;
    std::vector<u32> m_tables;                  // locale resource instance ids
    std::vector<Entry> m_entries;
    std::vector<std::string> m_sources;         // container labels, indexed by Entry::src
    std::map<u32, Index> m_index;
    std::map<u64, std::string> m_by_table;  // (u64(table)<<32)|key - u64 ON PURPOSE, see get()'s note
    size_t m_dup_same = 0;                      // (table,key) redefined with IDENTICAL text
    size_t m_dup_diff = 0;                      // (table,key) redefined with DIFFERENT text
    size_t m_ambiguous = 0;
    size_t m_comments = 0;
    size_t m_continuations = 0;
    size_t m_non_ascii = 0;
};

Tables& instance();

// Strip everything outside printable ASCII so a string can go through the GDI font atlas.
// Returns the input unchanged when it is already pure ASCII.
std::string to_display_ascii(const std::string& utf8);

}  // namespace os::text
