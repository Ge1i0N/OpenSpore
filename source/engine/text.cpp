#include "engine/text.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <set>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/naming.h"
#include "engine/pkg.h"

namespace os::text {
namespace {

// Type id of a localized string table, from reg_type.txt via typeids.inc. The game ASSIGNS type ids
// (they are not FNV of the name - MEMORY.md), so this comes from the registry, not a hash. The
// confirmation that the id is right is that all 2,944 resources carrying it parse with the grammar
// documented in text.h.
constexpr u32 kLocaleType = 0x02FAC0B6u;

// Type id of a resource that IS itself a DBPF - a NESTED package. Measured (tools/nested_locale.py):
// DataEP1/Spore_EP1_Locale_0{1,2}.package hold 21 of them, one per language, carrying 1,364 `locale`
// resources between them. The en-us one has instance 0x7080DB55 == fnv1_lower("en-us"), i.e. a nested
// package is named by the SAME hash rule that names a locale table - which is what makes picking the
// right one a lookup rather than a guess.
constexpr u32 kNestedType = 0x06EFC6AAu;

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

bool hex8(const std::string& s, size_t i, u32& out) {
    u32 v = 0;
    for (size_t k = 0; k < 8; ++k) {
        char c = s[i + k];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        v = (v << 4) | (u32)d;
    }
    out = v;
    return true;
}

// Recognise `0x<8 hex>` at the start of a record line. The character after the eight digits must be
// end-of-line or blank, which is what separates a key from a continuation line that merely quotes a
// hex id inside its text.
bool parse_key(const std::string& line, u32& key, size_t& after) {
    if (line.size() < 10) return false;
    if (line[0] != '0' || (line[1] != 'x' && line[1] != 'X')) return false;
    if (!hex8(line, 2, key)) return false;
    if (line.size() > 10 && line[10] != ' ' && line[10] != '\t') return false;
    after = 10;
    while (after < line.size() && (line[after] == ' ' || line[after] == '\t')) ++after;
    return true;
}

bool has_non_ascii(const std::string& s) {
    for (unsigned char c : s) {
        if (c >= 0x80) return true;
    }
    return false;
}

std::vector<std::string> split_crlf(const std::string& body) {
    // Records are separated by CRLF (measured on the raw bytes). Splitting on '\n' and dropping a
    // trailing '\r' also tolerates a lone-LF file, which costs nothing and cannot mis-split, because
    // a record's text never contains a bare CR.
    std::vector<std::string> out;
    size_t i = 0;
    while (true) {
        size_t nl = body.find('\n', i);
        std::string line = nl == std::string::npos ? body.substr(i) : body.substr(i, nl - i);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
        if (nl == std::string::npos) break;
        i = nl + 1;
    }
    return out;
}

}  // namespace

std::string to_display_ascii(const std::string& utf8) {
    if (!has_non_ascii(utf8)) return utf8;
    // Our overlay font is a GDI raster atlas of ASCII 32..126 (render/overlay.h), so it cannot draw
    // the accented Latin of de/fr or the Cyrillic of ru yet. Fold to a near ASCII rendering rather
    // than dropping the string: a menu that reads "Crédits" as "Credits" is still the game's own
    // text. Proper diacritics need a real font atlas (backlog), and Cyrillic/CJK need a wider atlas -
    // this function is explicitly a display shim, not a transliteration.
    std::string out;
    out.reserve(utf8.size());
    size_t i = 0;
    while (i < utf8.size()) {
        unsigned char c = (unsigned char)utf8[i];
        u32 cp = 0;
        int extra = 0;
        if (c < 0x80) {
            cp = c;
        } else if ((c & 0xE0u) == 0xC0u) {
            cp = c & 0x1Fu;
            extra = 1;
        } else if ((c & 0xF0u) == 0xE0u) {
            cp = c & 0x0Fu;
            extra = 2;
        } else if ((c & 0xF8u) == 0xF0u) {
            cp = c & 0x07u;
            extra = 3;
        } else {
            out.push_back('?');  // a stray continuation byte or an invalid lead byte
            ++i;
            continue;
        }
        if (i + (size_t)extra >= utf8.size()) {
            out.push_back('?');
            break;
        }
        bool bad = false;
        for (int k = 1; k <= extra; ++k) {
            unsigned char cc = (unsigned char)utf8[i + (size_t)k];
            if ((cc & 0xC0u) != 0x80u) {
                bad = true;
                break;
            }
            cp = (cp << 6) | (u32)(cc & 0x3Fu);
        }
        if (bad) {
            out.push_back('?');
            ++i;
            continue;
        }
        i += (size_t)(extra + 1);
        if (cp < 0x80) {
            out.push_back((char)cp);
            continue;
        }
        std::string fold;
        switch (cp) {
            // Latin-1 supplement: the letters the western/central European locales actually use.
            case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: fold = "A"; break;
            case 0xC6: fold = "AE"; break;             // AE ligature
            case 0xC7: fold = "C"; break;
            case 0xC8: case 0xC9: case 0xCA: case 0xCB: fold = "E"; break;
            case 0xCC: case 0xCD: case 0xCE: case 0xCF: fold = "I"; break;
            case 0xD0: fold = "D"; break;              // Eth
            case 0xD1: fold = "N"; break;
            case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD8: fold = "O"; break;
            case 0xD7: fold = "x"; break;              // multiplication sign
            case 0xD9: case 0xDA: case 0xDB: case 0xDC: fold = "U"; break;
            case 0xDD: fold = "Y"; break;
            case 0xDE: fold = "TH"; break;             // Thorn
            case 0xDF: fold = "ss"; break;             // sharp s
            case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: fold = "a"; break;
            case 0xE6: fold = "ae"; break;
            case 0xE7: fold = "c"; break;
            case 0xE8: case 0xE9: case 0xEA: case 0xEB: fold = "e"; break;
            case 0xEC: case 0xED: case 0xEE: case 0xEF: fold = "i"; break;
            case 0xF0: fold = "d"; break;              // eth
            case 0xF1: fold = "n"; break;
            case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF8: fold = "o"; break;
            case 0xF9: case 0xFA: case 0xFB: case 0xFC: fold = "u"; break;
            case 0xFD: fold = "y"; break;
            case 0xFE: fold = "th"; break;
            case 0xFF: fold = "y"; break;
            // Latin Extended-A: the Czech/Slovak/Polish/Hungarian/Romanian letters in our locales.
            case 0x0102: case 0x0104: fold = "A"; break;
            case 0x0103: case 0x0105: fold = "a"; break;
            case 0x0106: case 0x0108: case 0x010A: case 0x010C: fold = "C"; break;
            case 0x0107: case 0x0109: case 0x010B: case 0x010D: fold = "c"; break;
            case 0x010E: case 0x0110: fold = "D"; break;
            case 0x010F: case 0x0111: fold = "d"; break;
            case 0x0112: case 0x0118: case 0x011A: case 0x011C: case 0x011E: case 0x0120: fold = "E"; break;
            case 0x0113: case 0x0119: case 0x011B: case 0x011D: case 0x011F: case 0x0121: fold = "e"; break;
            case 0x0122: case 0x0124: fold = "G"; break;
            case 0x0123: case 0x0125: fold = "g"; break;
            case 0x0126: fold = "H"; break;
            case 0x0127: fold = "h"; break;
            case 0x0128: case 0x012A: case 0x012C: case 0x012E: case 0x0130: fold = "I"; break;
            case 0x0129: case 0x012B: case 0x012D: case 0x012F: case 0x0131: fold = "i"; break;
            case 0x0134: fold = "J"; break;
            case 0x0135: fold = "j"; break;
            case 0x0136: fold = "K"; break;
            case 0x0137: case 0x0138: fold = "k"; break;
            case 0x0139: case 0x013B: case 0x013D: case 0x0141: fold = "L"; break;
            case 0x013A: case 0x013C: case 0x013E: case 0x0142: fold = "l"; break;
            case 0x0143: case 0x0145: case 0x0147: fold = "N"; break;
            case 0x0144: case 0x0146: case 0x0148: case 0x0149: fold = "n"; break;
            case 0x014C: case 0x0150: case 0x0152: fold = "O"; break;
            case 0x014D: case 0x0151: case 0x0153: fold = "o"; break;
            case 0x0154: case 0x0156: case 0x0158: fold = "R"; break;
            case 0x0155: case 0x0157: case 0x0159: fold = "r"; break;
            case 0x015A: case 0x015C: case 0x015E: case 0x0160: fold = "S"; break;
            case 0x015B: case 0x015D: case 0x015F: case 0x0161: fold = "s"; break;
            case 0x0162: case 0x0164: case 0x0166: fold = "T"; break;
            case 0x0163: case 0x0165: case 0x0167: fold = "t"; break;
            case 0x0168: case 0x016A: case 0x016C: case 0x016E: case 0x0170: case 0x0172: fold = "U"; break;
            case 0x0169: case 0x016B: case 0x016D: case 0x016F: case 0x0171: case 0x0173: fold = "u"; break;
            case 0x0174: fold = "W"; break;
            case 0x0175: fold = "w"; break;
            case 0x0176: case 0x0178: fold = "Y"; break;
            case 0x0177: fold = "y"; break;
            case 0x0179: case 0x017B: case 0x017D: fold = "Z"; break;
            case 0x017A: case 0x017C: case 0x017E: fold = "z"; break;
            // General punctuation the string tables actually contain.
            case 0x2018: case 0x2019: case 0x2032: fold = "'"; break;
            case 0x201A: fold = ","; break;
            case 0x201B: fold = "`"; break;
            case 0x201C: case 0x201D: case 0x2033: fold = "\""; break;
            case 0x201E: fold = "\""; break;
            case 0x2013: fold = "-"; break;            // en dash
            case 0x2014: fold = "--"; break;           // em dash
            case 0x2026: fold = "..."; break;          // ellipsis
            case 0x2022: fold = "*"; break;            // bullet
            case 0x20AC: fold = "EUR"; break;          // euro sign
            case 0x00A0: fold = " "; break;            // no-break space
            case 0x00A9: fold = "(c)"; break;
            case 0x00AE: fold = "(R)"; break;
            case 0x00B0: fold = " deg"; break;
            case 0x00B1: fold = "+-"; break;
            case 0x00BD: fold = "1/2"; break;
            case 0x00BC: fold = "1/4"; break;
            case 0x00BE: fold = "3/4"; break;
            default: break;
        }
        if (fold.empty()) {
            // Unknown code point: one '?' per code point, never a per-byte burst, so a Cyrillic
            // sentence degrades to a readable-length placeholder.
            out.push_back('?');
        } else {
            out += fold;
        }
    }
    return out;
}

// Parse every `locale` resource of ONE package (flat or nested) into the shared tables, and
// return how many tables it yielded. Shared by both passes on purpose: the EP1 text lives in
// nested packages, and a second copy of this grammar would be exactly the drift that produces
// two disagreeing views of one dataset.
size_t Tables::parse_package(pkg::Package& pk) {
    size_t tables_parsed = 0, decode_failed = 0, no_grammar = 0;
    const auto& recs = pk.records();
    for (u32 n = 0; n < recs.size(); ++n) {
        const pkg::Record& r = recs[n];
        if (r.type() != kLocaleType) continue;
        std::vector<u8> bytes;
        if (!pk.read_resource_decoded(n, bytes)) {
            ++decode_failed;
            continue;
        }
        if (bytes.size() < 4) continue;

        size_t pos = 0;
        if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) pos = 3;
        std::string body((const char*)bytes.data() + pos, bytes.size() - pos);

        const u32 table_id = r.instance();
        m_tables.push_back(table_id);
        size_t before = m_entries.size();
        u32 cur_key = 0;
        bool in_record = false;
        for (const std::string& raw_line : split_crlf(body)) {
            size_t s = raw_line.find_first_not_of(" \t");
            if (s == std::string::npos) {
                in_record = false;  // a blank line closes the record
                continue;
            }
            const std::string t = raw_line.substr(s);
            if (t[0] == '#') {
                ++m_comments;  // 5,434 of these, including the assembled-table warning
                in_record = false;
                continue;
            }
            u32 key;
            size_t after;
            if (parse_key(t, key, after)) {
                std::string rest = t.substr(after);
                Entry e;
                e.key = key;
                e.table = table_id;
                if (!rest.empty() && rest[0] == '~') {
                    // The optional `~markup~` token. Measured: 0 unterminated tokens in 18,439
                    // key lines, so a missing close is NOT swallowed as markup - it stays text.
                    size_t close = rest.find('~', 1);
                    if (close != std::string::npos) {
                        e.markup = rest.substr(1, close - 1);
                        rest.erase(0, close + 1);
                        size_t k = rest.find_first_not_of(" \t");
                        rest = k == std::string::npos ? std::string() : rest.substr(k);
                    }
                }
                e.text = rest;
                if (has_non_ascii(e.text)) ++m_non_ascii;
                cur_key = key;
                in_record = true;
                m_entries.push_back(e);
                continue;
            }
            if (in_record) {
                // A continuation of the previous record's text (measured: 17 of 31,209 lines).
                ++m_continuations;
                Entry& e = m_entries.back();
                e.text.push_back('\n');
                e.text += t;
                if (has_non_ascii(t)) ++m_non_ascii;
                continue;
            }
            ++m_continuations;  // an orphan line before any key: counted, never guessed at
        }
        if (m_entries.size() == before) {
            ++no_grammar;
            m_tables.pop_back();
        } else {
            ++tables_parsed;
        }
    }
    (void)decode_failed;
    return tables_parsed;
}

bool Tables::load(const std::string& lang) {
    m_loaded = false;
    m_lang = lang;
    m_tables.clear();
    m_entries.clear();
    m_index.clear();
    m_by_table.clear();
    m_ambiguous = m_comments = m_continuations = m_non_ascii = 0;

    // fs.cpp already puts exe_dir/Locale/<lang> on the search path with tag "Locale", so we walk the
    // search paths and keep the Locale ones whose directory name matches the requested language.
    std::vector<std::string> locale_pkgs;
    for (const auto& sp : fs::search_paths()) {
        if (lower(sp.tag).rfind("locale", 0) != 0) continue;
        std::string leaf = lower(sp.dir.filename().string());
        if (!lang.empty() && leaf != lower(lang)) continue;
        for (const auto& p : pkg::find_packages(sp.dir.string())) locale_pkgs.push_back(p);
    }
    if (locale_pkgs.empty()) {
        LOG_WARN("text", "no Locale/%s package on the search paths - no localized text available",
                 lang.c_str());
        return false;
    }

    size_t tables_parsed = 0, flat_pkgs = 0, nested_pkgs = 0, nested_tables = 0;
    for (const auto& path : locale_pkgs) {
        pkg::Package pk;
        if (!pk.open(path)) continue;
        ++flat_pkgs;
        tables_parsed += parse_package(pk);
    }

    // NESTED packages: the EP1 locale text is not a file on disk at all. DataEP1/
    // Spore_EP1_Locale_0{1,2}.package hold 21 resources of type `package` (0x06EFC6AA), each of
    // which IS a whole DBPF, together carrying 1,364 `locale` resources (tools/nested_locale.py).
    // Without this pass the EP1 text tables (EP1_Buttons, EP1_Headers, EP1_Tooltips, GGETextEP1,
    // ScenarioBuildMode, ...) are absent, and ~285 caption references in the shipped UI layouts
    // point at exactly those. This is the THIRD shape of this session's recurring error: not a
    // wrong directory (exe_dir/Locale vs Data/Locale), not a wrong id field (group vs instance),
    // but a wrong CONTAINER DEPTH - the data was in the tree, the tree was deeper than we looked.
    {
        const u32 want = naming::hash_lower(lang.c_str());
        std::vector<std::string> all;
        std::set<std::string> seen;
        for (const auto& sp : fs::search_paths()) {
            for (const auto& p : pkg::find_packages(sp.dir.string())) {
                std::error_code ec;
                std::string key = std::filesystem::weakly_canonical(p, ec).string();
                if (ec) key = p;
                if (seen.insert(key).second) all.push_back(p);
            }
        }
        for (const auto& path : all) {
            pkg::Package outer;
            if (!outer.open(path)) continue;
            const auto& recs = outer.records();
            for (u32 n = 0; n < recs.size(); ++n) {
                if (recs[n].type() != kNestedType || recs[n].instance() != want) continue;
                std::vector<u8> bytes;
                if (!outer.read_resource_decoded(n, bytes)) continue;
                if (bytes.size() < 0x60 || std::memcmp(bytes.data(), "DBPF", 4) != 0) continue;
                pkg::Package inner;
                if (!inner.open_from_memory(bytes.data(), bytes.size(),
                        std::filesystem::path(path).filename().string() + "#nested")) continue;
                ++nested_pkgs;
                size_t got = parse_package(inner);
                nested_tables += got;
                tables_parsed += got;
            }
        }
    }

    // Index by key and classify ambiguity. This is the per-table-key-space finding: a low key means
    // different things in different tables, so get() must be able to refuse to answer it.
    std::stable_sort(m_entries.begin(), m_entries.end(),
                     [](const Entry& a, const Entry& b) {
                         if (a.key != b.key) return a.key < b.key;
                         return a.table < b.table;
                     });
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        m_by_table[((u64)e.table << 32) | e.key] = e.text;
        Index& ix = m_index[e.key];
        ix.entry_ids.push_back(i);
    }
    for (auto& kv : m_index) {
        Index& ix = kv.second;
        if (ix.entry_ids.size() <= 1) continue;
        const std::string& first = m_entries[ix.entry_ids.front()].text;
        for (size_t id : ix.entry_ids) {
            if (m_entries[id].text != first) {
                ix.agrees = false;
                break;
            }
        }
        if (!ix.agrees) ++m_ambiguous;
    }
    std::sort(m_tables.begin(), m_tables.end());
    m_tables.erase(std::unique(m_tables.begin(), m_tables.end()), m_tables.end());

    m_loaded = true;
    LOG_INFO("text", "locale '%s': %zu tables (%zu flat packages, %zu nested containers, %zu nested "
                     "tables), %zu entries, %zu distinct keys, %zu ambiguous, %zu comments, %zu "
                     "continuation lines, %zu non-ASCII texts",
             lang.c_str(), tables_parsed, flat_pkgs, nested_pkgs, nested_tables, m_entries.size(),
             m_index.size(), m_ambiguous, m_comments, m_continuations, m_non_ascii);
    return true;
}

std::string Tables::get(u32 key) const {
    auto it = m_index.find(key);
    if (it == m_index.end()) return std::string();
    const Index& ix = it->second;
    // Refuse an ambiguous key rather than guess: the key space is per-table, so returning whichever
    // table was parsed first would put someone else's string into our menu.
    if (!ix.agrees) return std::string();
    return m_entries[ix.entry_ids.front()].text;
}

std::string Tables::markup(u32 key) const {
    auto it = m_index.find(key);
    if (it == m_index.end() || !it->second.agrees) return std::string();
    return m_entries[it->second.entry_ids.front()].markup;
}

std::string Tables::get(u32 table, u32 key) const {
    auto it = m_by_table.find(((u64)table << 32) | key);
    return it == m_by_table.end() ? std::string() : it->second;
}

bool Tables::ambiguous(u32 key) const {
    auto it = m_index.find(key);
    return it != m_index.end() && !it->second.agrees;
}

std::vector<Hit> Tables::find_label(const std::string& label, size_t limit) const {
    // The reverse lookup a menu needs: it knows it wants "New Game" and not which key carries it.
    std::vector<Hit> out;
    const std::string want = lower(label);
    for (const auto& e : m_entries) {
        if (lower(e.text) != want) continue;
        Hit h;
        h.key = e.key;
        h.table = e.table;
        h.text = e.text;
        h.markup = e.markup;
        out.push_back(h);
        if (out.size() >= limit) break;
    }
    return out;
}

std::string Tables::stats() const {
    char buf[512];
    std::snprintf(buf, sizeof(buf),
                  "locale '%s': %zu tables, %zu entries, %zu keys, %zu ambiguous, %zu non-ascii",
                  m_lang.c_str(), m_tables.size(), m_entries.size(), m_index.size(), m_ambiguous,
                  m_non_ascii);
    return buf;
}

Tables& instance() {
    static Tables t;
    return t;
}

}  // namespace os::text
