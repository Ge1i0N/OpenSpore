#include "states/statemgr.h"

#include <algorithm>
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
    st.id = (u32)fnv1a_upper(st.name.c_str());
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
    u32 id = (u32)fnv1a_upper(name.c_str());
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
    return true;
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
