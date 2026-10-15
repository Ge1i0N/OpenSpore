#include "engine/gge_dbcheck.h"
#include "engine/gge_intro.h"

namespace os::gge {
bool DbCheckSession::start(ContentCheck observation, DbCheckBackend& backend) {
    if (m_state != DbCheckState::Inactive && m_state != DbCheckState::DependencyBlocked) return false;
    if (observation == ContentCheck::Unknown) {
        m_state = DbCheckState::DependencyBlocked;
        return false; // unimplemented observation cannot be interpreted as an absent original manager
    }
    if (observation == ContentCheck::Absent || observation == ContentCheck::Bypass) {
        m_state = DbCheckState::Continued; // host reentry guard, before external synchronous notification
        backend.notify(kDbCheckNotification); // 0x60B654; continuation, not a successful database result
        return true;
    }
    m_state = DbCheckState::Starting;
    const DbCheckUi ui;
    m_ui_owned = backend.load_ui(ui);
    if (!m_ui_owned) {
        m_state = DbCheckState::Continued;
        backend.notify(kDbCheckNotification); // original layout failure also posts the notification
        return true;
    }
    if (!backend.find_root(ui.root_control)) {
        backend.unload_ui();
        m_ui_owned = false;
        m_state = DbCheckState::Continued;
        backend.notify(kDbCheckNotification);
        return true;
    }
    m_state = DbCheckState::Dialog;
    m_callback_armed = false;
    backend.attach_dialog_proc(ui.root_control); // 0x60B340 -> root +0x104; allow a synchronous callback
    if (m_state != DbCheckState::Dialog) return true;
    if (backend.find_root(ui.root_control)) backend.attach_listener(ui.root_control); // 0x60B62D
    return true;
}

bool DbCheckSession::on_ui_callback(ContentCheck observation, u32 control, DbCheckBackend& backend) {
    if (m_state != DbCheckState::Dialog) return false;
    // GA 0x60B473..0x60B4F5: the first callback only sets dialog+0x0C, no completion side effects.
    if (!m_callback_armed) { m_callback_armed = true; return true; }
    if (observation == ContentCheck::Unknown || observation == ContentCheck::Absent) return false;
    m_state = DbCheckState::Continued; // guard duplicate callbacks during external side effects
    backend.manager_continue();
    backend.notify(kDbCheckNotification); // original order: manager operation, notify, cleanup
    const DbCheckUi ui;
    if (backend.find_root(ui.root_control)) {
        backend.dispatch_callback(ui.root_control, control);
        backend.detach_dialog_proc(ui.root_control);
    }
    backend.unload_ui();
    m_ui_owned = false;
    return true;
}

void DbCheckSession::cancel(DbCheckBackend& backend) {
    // Host ownership policy: release locally owned UI on cancellation; never emit continuation.
    const bool release_ui = m_ui_owned;
    m_state = DbCheckState::Inactive; // suppress teardown-triggered callbacks before detach/unload
    m_callback_armed = m_ui_owned = false;
    if (release_ui) {
        const DbCheckUi ui;
        if (backend.find_root(ui.root_control)) backend.detach_dialog_proc(ui.root_control);
        backend.unload_ui();
    }
}
}  // namespace os::gge
