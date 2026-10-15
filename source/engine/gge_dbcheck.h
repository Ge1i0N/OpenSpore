#pragma once
#include "common.h"

namespace os::gge {

// Unknown is a host capability boundary, NOT the GA null-manager branch.
// Bypass/NeedsDialog correspond only to the observed manager vtable +0x1C result.
enum class ContentCheck { Unknown, Absent, Bypass, NeedsDialog };
enum class DbCheckState { Inactive, DependencyBlocked, Starting, Dialog, Continued };
struct DbCheckUi {
    u32 instance = 0x4b0ac091u;
    u32 request_tag = 0x0510a95bu;
    u32 group = 0x40464100u;
    u32 mount = 0x05b598f7u;
    u32 root_control = 0x0431e538u;
};

// Explicit side-effect port. No default methods that silently pretend a service succeeded.
// This subset has no global message bus, content manager or original WinProc callback pump.
class DbCheckBackend {
public:
    virtual ~DbCheckBackend() = default;
    virtual bool load_ui(const DbCheckUi& ui) = 0; // false must leave no UI owned by this session
    virtual bool find_root(u32 control) = 0;
    virtual void attach_dialog_proc(u32 root) = 0;
    virtual void attach_listener(u32 root) = 0;
    virtual void manager_continue() = 0; // manager vtable +0x20; semantics not assumed to be DB success
    virtual void notify(u32 message) = 0;
    virtual void dispatch_callback(u32 root, u32 callback_control) = 0;
    virtual void detach_dialog_proc(u32 root) = 0;
    virtual void unload_ui() = 0;
};

// GA 0x60B5D0 / 0x60B340 / 0x60B470 subset. One Intro-owned request per session.
// on_ui_callback is an original-callback-shaped event, NOT a per-frame completion timer.
class DbCheckSession {
public:
    DbCheckSession() = default;
    DbCheckSession(const DbCheckSession&) = delete;
    DbCheckSession& operator=(const DbCheckSession&) = delete;
    bool start(ContentCheck observation, DbCheckBackend& backend);
    bool on_ui_callback(ContentCheck observation, u32 callback_control, DbCheckBackend& backend);
    void cancel(DbCheckBackend& backend);
    DbCheckState state() const { return m_state; }
    bool callback_armed() const { return m_callback_armed; }
private:
    DbCheckState m_state = DbCheckState::Inactive;
    bool m_callback_armed = false;
    bool m_ui_owned = false;
};
}  // namespace os::gge
