#include "engine/gge_intro.h"

namespace os::gge {
void IntroGate::enter() {
    m_active = true;
    m_check_requested = m_notified = m_post_requested = false;
    m_wait = IntroWait::IntroPhase;
}
void IntroGate::leave() { *this = IntroGate{}; }
bool IntroGate::notify(u32 id) {
    if (!m_active || id != kDbCheckNotification) return false;
    m_notified = true; // GA 0xDEA820, +0x12; notification is not a DB success result.
    return true;
}
IntroAction IntroGate::update(u32 phase, MovieObservation movie) {
    if (!m_active) return IntroAction::None;
    // Unknown is a host guard: missing service is not an observed idle GA return.
    if (phase != 6) { m_wait = IntroWait::IntroPhase; return IntroAction::None; }
    if (movie == MovieObservation::Unknown) { m_wait = IntroWait::MovieService; return IntroAction::None; }
    if (movie == MovieObservation::Playing) { m_wait = IntroWait::MoviePlaying; return IntroAction::None; }
    if (!m_check_requested) {
        m_check_requested = true; // GA +0x11 is set after DBCheck +0x18.
        m_wait = IntroWait::DbNotification;
        return IntroAction::StartDbCheck;
    }
    if (m_notified && !m_post_requested) {
        m_post_requested = true; // GA +0x0D is set after 0xDE9550.
        m_wait = IntroWait::PostIntroWork;
        return IntroAction::RunPostIntro;
    }
    m_wait = m_post_requested ? IntroWait::PostIntroWork : IntroWait::DbNotification;
    return IntroAction::None;
}
const char* wait_description(IntroWait wait) {
    switch (wait) {
    case IntroWait::Inactive: return "inactive";
    case IntroWait::IntroPhase: return "waiting for post-title phase 6";
    case IntroWait::MovieService: return "blocked: MoviePlayer observation unavailable (not implemented)";
    case IntroWait::MoviePlaying: return "waiting for MoviePlayer idle";
    case IntroWait::DbNotification: return "waiting for DBCheck notification 0x08085C4C";
    case IntroWait::PostIntroWork: return "post-intro requested; original side effects/state 10 not implemented";
    }
    return "unknown gate state";
}
IntroGate& intro_subset() { static IntroGate gate; return gate; }
}  // namespace os::gge
