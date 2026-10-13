#include "states/statemgr.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

#include "engine/console.h"
#include "engine/diag.h"
#include "engine/log.h"

namespace os::states {
namespace {

Manager g_mgr;

std::string fmt_current(const State* s) {
    if (!s) return "<none>";
    return s->name;
}

}  // namespace

bool Manager::reg(State st) {
    // An explicit `-id` from the game's script wins verbatim; only auto states are hashed. Recomputing
    // an explicit id would silently renumber the states the UI addresses by number.
    st.id = st.has_explicit_id ? st.explicit_id : fnv1_lower(st.name.c_str());
    for (auto* e : m_all) {
        if (e->id == st.id) {
            // Original: push "A state with this ID already exists" (0x01413600) and assert.
            LOG_ERROR("state", "A state with this ID already exists: %s [0x%08x]", st.name.c_str(), st.id);
            OS_ASSERT_MSG(e->id != st.id, "duplicate state id");
            return false;
        }
    }
    State* p = new State(std::move(st));
    m_owned.push_back(p);
    m_all.push_back(p);
    LOG_DEBUG("state", "registered '%s' id=0x%08x (%zu states)", p->name.c_str(), p->id, m_all.size());
    return true;
}

State* Manager::find(const std::string& name) {
    // DISASM-SETTLED (session 13, the state-add at 0x7e3fb0):
    //   7e40c5  push 1 / push "id" (0x013fc8f0) / call 0x838410   ; the ArgScript arg lookup
    //   7e40d6  je  0x7e40ea                                      ; `id` ABSENT -> hash the name
    //   7e40e8  jmp 0x7e4100                                      ; `id` PRESENT -> skip the hash
    //   7e4103  mov [ecx+0x28], eax                               ; BOTH are stored in [state+0x28]
    // So an explicit `-id` REPLACES the identity, and 19 of the game's 60 states carry one (`Main`
    // is -id 0). Hashing the name here would therefore make every explicit-id state unreachable by
    // name - which contradicts the shipped script's own header ("you can run with -state:<stateName>").
    // The original must resolve names as STRINGS, so we do too. The tolower folding matches the
    // engine's own case rule (the hash's mode arg 1), so `-state:creatureeditor` behaves as in-game.
    for (auto* e : m_all) {
        if (e->name.size() != name.size()) continue;
        bool same = true;
        for (size_t i = 0; i < name.size(); ++i) {
            if (std::tolower((unsigned char)e->name[i]) != std::tolower((unsigned char)name[i])) {
                same = false;
                break;
            }
        }
        if (same) return e;
    }
    return nullptr;
}

State* Manager::find_by_id(u32 id) {
    // The [state+0xb8] map the original inserts into is keyed by THIS id, which is what messages and
    // the UI layouts address a state by - hence the script's "assign manual IDs just so I can fill in
    // the UI layout stuff more easily".
    for (auto* e : m_all) {
        if (e->id == id) return e;
    }
    return nullptr;
}

bool Manager::goto_name(const std::string& name) {
    State* target = find(name);
    if (!target) {
        // Original strings: 'unknown to state' (0x014135b4) / 'no such state' (0x01413804)
        LOG_ERROR("state", "unknown to state: %s", name.c_str());
        return false;
    }
    if (m_in_transition) {
        // Original: "can't nest states or transitions" (0x014135dc)
        LOG_ERROR("state", "can't nest states or transitions (tried %s while switching)", name.c_str());
        OS_ASSERT_MSG(!m_in_transition, "nested state transition");
        return false;
    }
    if (target == m_current) {
        LOG_DEBUG("state", "already in state %s", name.c_str());
        return true;
    }
    m_in_transition = true;
    if (m_current && m_current->on_exit) {
        LOG_DEBUG("state", "onExit '%s'", m_current->name.c_str());
        m_current->on_exit();
    }
    m_current = target;
    LOG_INFO("state", "-> %s [0x%08x]", target->name.c_str(), target->id);
    if (target->on_enter) target->on_enter();
    m_in_transition = false;
    // Fire the chain this transition queued. The original's transitions are a CHAIN, not a stack
    // (`endChain` 0x014130f8 sits beside `entry`/`exit`/`onExit` in the same .rdata cluster), so a
    // state's entry block CAN name the next state - it just must not re-enter goto_name directly,
    // which is what the assert above guards. request_goto() is the chain link.
    while (!m_pending.empty()) {
        std::string nxt = m_pending.front();
        m_pending.erase(m_pending.begin());
        if (++m_chain_depth > 32) {
            // A script cycle (state A -> B -> A). Refuse rather than hang: the shipped table has no
            // such cycle today (verified by walking all 60 states), so this only fires on bad data.
            LOG_ERROR("state", "state chain deeper than 32 hops, stopping at '%s'", nxt.c_str());
            m_pending.clear();
            break;
        }
        goto_name(nxt);
    }
    m_chain_depth = 0;
    return true;
}

void Manager::request_goto(const std::string& name) {
    // The chain link: queue a transition to fire after the current one completes. Queued, not
    // executed, so it can never trip the no-nesting assert.
    LOG_DEBUG("state", "chain: queued gotoState '%s' (depth %d)", name.c_str(), m_chain_depth + 1);
    m_pending.push_back(name);
}

bool Manager::next() {
    if (m_all.empty()) return false;
    size_t idx = 0;
    for (size_t i = 0; i < m_all.size(); ++i) {
        if (m_all[i] == m_current) {
            idx = (i + 1) % m_all.size();
            break;
        }
    }
    return goto_name(m_all[idx]->name);
}

bool Manager::prev() {
    if (m_all.empty()) return false;
    size_t idx = 0;
    for (size_t i = 0; i < m_all.size(); ++i) {
        if (m_all[i] == m_current) {
            idx = (i + m_all.size() - 1) % m_all.size();
            break;
        }
    }
    return goto_name(m_all[idx]->name);
}

void Manager::reload() {
    if (!m_current) return;
    LOG_INFO("state", "reload '%s'", m_current->name.c_str());
    if (m_current->on_reload) m_current->on_reload(nullptr);
}

std::string Manager::list() const {
    // Format lifted from the original: "  %-40.40s [0x%08x]" (0x014137ec)
    std::string out;
    char line[128];
    for (auto* s : m_all) {
        _snprintf_s(line, sizeof(line), _TRUNCATE, "  %-40.40s [0x%08x]%s", s->name.c_str(), s->id,
                    (s == m_current) ? "   <- current" : "");
        out += line;
        out += "\n";
    }
    char cur[128];
    _snprintf_s(cur, sizeof(cur), _TRUNCATE, "Current state: '%s'", fmt_current(m_current).c_str());
    out += cur;
    return out;
}

void Manager::tick(f32 dt) {
    if (m_current && m_current->on_tick) m_current->on_tick(dt);
}

void Manager::render() {
    if (m_current && m_current->on_render) m_current->on_render();
}

Manager& manager() { return g_mgr; }

void install_console_command(Manager& mgr) {
    auto state_cmd = [&mgr](const console::CmdContext& c) -> std::string {
        if (c.args.size() == 1) return mgr.list();  // no args -> list current state
        const std::string& a = c.args[1];
        if (a == "-list") return mgr.list();
        if (a == "-next") return mgr.next() ? ("next -> " + mgr.current_name()) : std::string("no such state");
        if (a == "-prev") return mgr.prev() ? ("prev -> " + mgr.current_name()) : std::string("no such state");
        if (a == "-reload") {
            mgr.reload();
            return "reloaded";
        }
        return mgr.goto_name(a) ? ("switched to " + mgr.current_name()) : std::string("no such state");
    };
    console::reg({"state",
                  "State manager control. With no arguments lists current state.",
                  "-list^-next^-prev^-reload^<state:cstring>",
                  state_cmd});
    LOG_INFO("state", "console command 'state' installed (AppStateManager analog)");
}

}  // namespace os::states
