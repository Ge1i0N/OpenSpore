#include "engine/appmodes.h"  // the recovered mode table, so the log can name what a mode IS
#include "engine/appstates.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>

#include "engine/fs.h"
#include "engine/log.h"
#include "engine/pkg.h"
#include "states/statemgr.h"
#include "states/uipresent.h"  // present_ui_screen: the layout a state names, drawn

namespace os::appstates {
namespace {

// `trigger` (assigned type id, from reg_type.txt via typeids.inc - type ids are NOT hashes).
constexpr u32 kTriggerType = 0x024A0E52u;
// SporeAppStates, the instance id the run path asks the resource manager for at 0xf48505.
constexpr u32 kAppStatesInstance = 0xD6396913u;
// The CSA (Cast Away / bonus-pack) variant of the same table, for the record.
constexpr u32 kCSAInstance = 0x094EFEF3u;

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::vector<std::string> split_lines(const std::string& body) {
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

std::string first_token(const std::string& s) {
    size_t sp = s.find_first_of(" \t");
    return sp == std::string::npos ? s : s.substr(0, sp);
}

// Strip a trailing comment. `#` starts one; the script has no string escapes that could contain one
// (measured: every `#` in the file begins a whole-line comment), so a plain scan is safe here.
std::string strip_comment(const std::string& line) {
    size_t h = line.find('#');
    return h == std::string::npos ? line : line.substr(0, h);
}

bool parse_u32(const std::string& tok, u32& out) {
    if (tok.empty()) return false;
    char* end = nullptr;
    unsigned long v = std::strtoul(tok.c_str(), &end, 0);
    if (end == tok.c_str()) return false;
    while (end && (*end == ' ' || *end == '\t')) ++end;
    out = (u32)v;
    return true;
}

// Parses the ArgScript state table. Grammar (analysis/states.md):
//   state <Name> [-id <0xHEX|dec>]
//     description "<text>" | mode <M> | gotoState <S> | prop ... | message ...
//     entry  <actions...>  exit  <actions...>
//   end
// `seti kName <id>` defines module-level id constants referenced as $kName.
void parse(const std::string& body, std::vector<GameState>& out, std::map<std::string, u32>& seti) {
    GameState* cur = nullptr;
    bool in_exit = false;
    for (const std::string& raw : split_lines(body)) {
        std::string line = strip_comment(raw);
        size_t s = line.find_first_not_of(" \t");
        if (s == std::string::npos) continue;
        line = line.substr(s);
        const std::string verb = first_token(line);
        std::string rest = line.size() > verb.size() ? line.substr(verb.size() + 1) : std::string();
        // Trim the rest.
        size_t rs = rest.find_first_not_of(" \t");
        rest = rs == std::string::npos ? std::string() : rest.substr(rs);

        if (verb == "state") {
            GameState st;
            st.name = first_token(rest);
            // -id may appear after the name: `state Main -id 0`
            size_t pos = rest.find("-id");
            if (pos != std::string::npos) {
                std::string tail = rest.substr(pos + 3);
                // Trim BEFORE taking the token: `first_token(" 0")` returns "" because the very first
                // character is the delimiter, which silently dropped every explicit -id in the first
                // run (all 60 states came out as hashed id 0).
                size_t ts = tail.find_first_not_of(" \t");
                tail = ts == std::string::npos ? std::string() : tail.substr(ts);
                std::string idtok = first_token(tail);
                u32 v;
                if (parse_u32(idtok, v)) {
                    st.has_explicit_id = true;
                    st.id = v;
                }
            }
            out.push_back(st);
            cur = &out.back();
            in_exit = false;
            continue;
        }
        if (verb == "end") {
            cur = nullptr;
            in_exit = false;
            continue;
        }
        if (verb == "seti") {
            std::istringstream is(rest);
            std::string k, v;
            if (is >> k >> v) {
                u32 id;
                if (parse_u32(v, id)) seti[k] = id;
            }
            continue;
        }
        if (!cur) continue;

        if (verb == "entry") { in_exit = false; continue; }
        if (verb == "exit") { in_exit = true; continue; }
        if (verb == "description") {
            size_t q1 = rest.find('"'), q2 = rest.rfind('"');
            if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                cur->description = rest.substr(q1 + 1, q2 - q1 - 1);
            }
            continue;
        }
        Action a;
        a.verb = verb;
        a.raw = rest;
        a.a1 = first_token(rest);
        if (verb == "mode") {
            if (in_exit) {
                cur->exit.push_back(a);
            } else {
                cur->mode = a.a1;
                cur->entry.push_back(a);
            }
            continue;
        }
        if (verb == "gotoState") {
            if (!in_exit) cur->goto_state.push_back(a.a1);
            (in_exit ? cur->exit : cur->entry).push_back(a);
            continue;
        }
        if (verb == "message" && !in_exit) {
            cur->messages.push_back(a);
            cur->entry.push_back(a);
            continue;
        }
        (in_exit ? cur->exit : cur->entry).push_back(a);
    }
}

// Reads the trigger resource out of the packages. Returns the decoded text and names the winning
// package; `all` collects every copy so the log can show the mount-order ambiguity honestly.
bool read_resource(std::string& text, std::string& source, std::vector<std::string>& all) {
    // Deduplicate by canonical path: the dev search list has both Data=<exe>\Data and cwd=<exe>, so a
    // recursive scan visits every package TWICE. Without this the "copies seen" log listed 5 copies
    // of a script that exists 3 times, which is exactly the trap already documented for the `pkg`
    // verb and re-entered in spui.cpp the same day.
    std::vector<std::string> pkgs;
    std::set<std::string> seen_path;
    for (const auto& sp : fs::search_paths()) {
        for (const auto& path : pkg::find_packages(sp.dir.string())) {
            std::error_code ec;
            std::string key = std::filesystem::weakly_canonical(path, ec).string();
            if (ec) key = path;
            if (seen_path.insert(key).second) pkgs.push_back(path);
        }
    }
    for (const auto& path : pkgs) {
            pkg::Package pk;
            if (!pk.open(path)) continue;
            const auto& recs = pk.records();
            for (u32 n = 0; n < recs.size(); ++n) {
                const pkg::Record& r = recs[n];
                if (r.type() != kTriggerType) continue;
                if (r.instance() != kAppStatesInstance && r.instance() != kCSAInstance) continue;
                std::vector<u8> bytes;
                if (!pk.read_resource_decoded(n, bytes)) continue;
                std::string body((const char*)bytes.data(), bytes.size());
                if (body.size() > 8 && body[0] == 0xEF) body = body.substr(3);  // tolerate a BOM
                const bool is_main = r.instance() == kAppStatesInstance;
                char tag[160];
                std::snprintf(tag, sizeof(tag), "%s rec[%u] %s %zu B",
                              std::filesystem::path(path).filename().string().c_str(), n,
                              is_main ? "SporeAppStates" : "CSAAppStates", bytes.size());
                all.push_back(tag);
                if (!is_main) continue;  // the CSA table describes a different product; do not use it
                // Largest copy wins, which is how the shipped tree resolves the three duplicates
                // (Spore_Game 8,202 B / PatchData 8,202 B / EP1 8,499 B) in practice.
                if (text.empty() || bytes.size() > text.size()) {
                    text = std::move(body);
                    source = tag;
                }
            }
        }
    return !text.empty();
}

}  // namespace

bool Table::load() {
    m_loaded = false;
    m_states.clear();
    m_source.clear();
    std::string text;
    std::vector<std::string> all;
    if (!read_resource(text, m_source, all)) {
        LOG_ERROR("appstates",
                  "no SporeAppStates trigger (instance 0x%08X) found on the search paths - the game's "
                  "state table is unavailable. Searched %zu path(s); if this line ever appears, check "
                  "WHERE you looked before concluding the data is missing (session 13: two 'no data' "
                  "conclusions were search-path bugs).",
                  kAppStatesInstance, fs::search_paths().size());
        return false;
    }
    std::map<std::string, u32> seti;
    parse(text, m_states, seti);
    m_loaded = !m_states.empty();
    LOG_INFO("appstates", "loaded %zu states from %s (%zu seti vars); copies seen: %s",
             m_states.size(), m_source.c_str(), seti.size(),
             all.empty() ? "-" : [&] {
                 std::string j;
                 for (size_t i = 0; i < all.size(); ++i) { if (i) j += " | "; j += all[i]; }
                 return j;
             }().c_str());
    return m_loaded;
}

const GameState* Table::find(const std::string& name) const {
    for (const auto& s : m_states) {
        if (s.name == name) return &s;
    }
    return nullptr;
}

const GameState* Table::find_by_id(u32 id) const {
    for (const auto& s : m_states) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

size_t Table::mode_count() const {
    std::vector<std::string> modes;
    for (const auto& s : m_states) {
        if (s.mode.empty()) continue;
        if (std::find(modes.begin(), modes.end(), s.mode) == modes.end()) modes.push_back(s.mode);
    }
    return modes.size();
}

std::string Table::entry_state() const {
    // `state Main -id 0` is the documented default entry. Prefer the explicit id 0, then the name.
    for (const auto& s : m_states) {
        if (s.has_explicit_id && s.id == 0) return s.name;
    }
    if (find("Main")) return "Main";
    return m_states.empty() ? std::string() : m_states.front().name;
}

std::string Table::stats() const {
    char buf[320];
    std::snprintf(buf, sizeof(buf), "SporeAppStates: %zu states, %zu modes, entry '%s' (from %s)",
                  m_states.size(), mode_count(), entry_state().c_str(),
                  m_source.empty() ? "not loaded" : m_source.c_str());
    return buf;
}

std::vector<UiCommand> ui_commands(const GameState& gs) {
    std::vector<UiCommand> out;
    // Both blocks in file order: DevLauncher loads on `entry` and unloads on `exit`, and the pair is
    // what tells the two apart. Walking only `entry` would miss the teardown half of the only UI
    // command in the entire script.
    for (int block = 0; block < 2; ++block) {
        const std::vector<Action>& acts = block == 0 ? gs.entry : gs.exit;
        for (const Action& a : acts) {
            if (a.verb != "cheat") continue;
            // Action::raw still carries the script's quotes:
            //     "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"
            size_t q1 = a.raw.find('"'), q2 = a.raw.rfind('"');
            const std::string body = (q1 != std::string::npos && q2 != std::string::npos && q2 > q1)
                                         ? a.raw.substr(q1 + 1, q2 - q1 - 1)
                                         : a.raw;
            std::istringstream is(body);
            std::string cmd, flag, name, s1, s2;
            if (!(is >> cmd >> flag >> name)) continue;
            cmd = lower(cmd);
            flag = lower(flag);
            if (cmd != "uilayout") continue;
            const bool load = (flag == "-load"), unload = (flag == "-unload");
            if (!load && !unload) continue;   // an undocumented subcommand: counted by stats(), not guessed
            UiCommand u;
            u.name = name;
            u.unload = unload;
            if (is >> s1) parse_u32(s1, u.id1);
            if (is >> s2) parse_u32(s2, u.id2);
            out.push_back(u);
        }
    }
    return out;
}

Table& instance() {
    static Table t;
    return t;
}

void install(os::states::Manager& mgr) {
    if (!instance().loaded()) {
        LOG_WARN("appstates", "not installing game states (table not loaded)");
        return;
    }
    size_t added = 0, skipped = 0;
    for (const GameState& gs : instance().states()) {
        if (mgr.find(gs.name)) {
            ++skipped;  // a state we own (Loader/Title) - never shadow ours silently
            continue;
        }
        os::states::State st;
        st.name = gs.name;
        st.has_explicit_id = gs.has_explicit_id;
        st.explicit_id = gs.id;
        // A game state has no C++ callbacks of its own: its behaviour is the ACTION LIST. What we CAN
        // execute faithfully today is the transition chain, because `gotoState` is pure state machine.
        // Everything else (mode/message/cheat/effect) names an engine subsystem we have not built, so
        // entering such a state logs exactly what it could not do, rather than pretending.
        std::vector<std::string> chain = gs.goto_state;
        std::string mode = gs.mode;
        size_t n_msg = gs.messages.size();
        size_t n_cheat = 0, n_effect = 0;
        for (const Action& a : gs.entry) {
            if (a.verb == "cheat") ++n_cheat;
            if (a.verb == "effect") ++n_effect;
        }
        // THE STATE'S OWN UI INSTRUCTION. `cheat "uilayout -load <name>"` used to be reported as a
        // cheat we could not run. But the name hashes to a `spui` resource instance and we can READ
        // those bytes, so it is now an instruction we EXECUTE: this state gets a renderer that draws
        // the screen the script asked for. Resolution is deferred to render time on purpose - the
        // spui table may not be loaded yet at install() time, and a map lookup per frame is cheaper
        // than an init-order constraint that would silently break.
        std::vector<UiCommand> uis = ui_commands(gs);
        size_t n_ui_load = 0;
        for (const UiCommand& u : uis) {
            if (!u.unload) ++n_ui_load;
        }
        // ONLY a state that names a screen gets a renderer. Setting on_render unconditionally would
        // swallow the app's renderer-less placeholder, and a state that presents no frame means
        // `-capture` is never serviced - a hang masquerading as a blank screen.
        if (n_ui_load) {
            st.on_render = [uis = std::move(uis)]() {
                os::states::present_ui_screen(false);
            };
        }
        const size_t n_cheat_unrun = n_cheat > n_ui_load ? n_cheat - n_ui_load : 0;
        st.on_enter = [chain = std::move(chain), mode = std::move(mode), n_msg, n_cheat_unrun,
                       n_ui_load, n_effect, name = gs.name]() {
            if (!mode.empty()) {
                // `mode <M>` selects one of the original's ten named driver CLASSES, which we do not
                // have. But we now KNOW the table (analysis/modes.md), so the log names the tag, the
                // registering function and the state the mode drives, instead of a bare "not
                // implemented" that tells the reader nothing they can act on.
                if (const auto* m = appmodes::find_by_name(mode.c_str())) {
                    LOG_WARN("appstates", "%s: `%s` - %s", name.c_str(), mode.c_str(),
                             appmodes::describe(*m).c_str());
                } else {
                    LOG_WARN("appstates", "%s: `mode %s` not executed - this build of the game has no "
                                          "such mode class (searched the whole oracle)",
                             name.c_str(), mode.c_str());
                }
            }
            if (n_msg) {
                LOG_WARN("appstates", "%s: %zu message(s) not sent (no message bus)", name.c_str(), n_msg);
            }
            if (n_ui_load) {
                // Not a gap: this is the state machine driving the UI, executed every frame by
                // present_ui_screen() (states/uipresent.cpp).
                LOG_INFO("appstates", "%s: %zu `uilayout -load` EXECUTED by the UI layer (the screen "
                                      "this state names)", name.c_str(), n_ui_load);
            }
            if (n_cheat_unrun) {
                LOG_WARN("appstates", "%s: %zu cheat(s) not run (no console-command backend)",
                         name.c_str(), n_cheat_unrun);
            }
            if (n_effect) {
                LOG_WARN("appstates", "%s: %zu effect(s) not run (no FX system)", name.c_str(), n_effect);
            }
            for (const std::string& nxt : chain) {
                LOG_INFO("appstates", "%s: gotoState %s", name.c_str(), nxt.c_str());
                os::states::manager().request_goto(nxt);
            }
        };
        if (!mgr.reg(std::move(st))) {
            ++skipped;
            continue;
        }
        ++added;
    }
    LOG_INFO("appstates", "installed %zu game states into the state manager (%zu skipped: ours or "
                          "duplicate id)", added, skipped);
}

}  // namespace os::appstates
