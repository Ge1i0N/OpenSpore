#pragma once
#include "common.h"

namespace os::gge {
constexpr u32 kDbCheckNotification = 0x08085c4cu;
enum class MovieObservation { Unknown, Idle, Playing };
enum class IntroAction { None, StartDbCheck, RunPostIntro };
enum class IntroWait { Inactive, IntroPhase, MovieService, MoviePlaying, DbNotification, PostIntroWork };

// Post-title decision subset of GA 0xDEAF70. Guards/one-shot latches only: no title animation,
// MoviePlayer, DBCheck, message bus or actual transition. Notifications may arrive before update.
class IntroGate {
public:
    void enter();
    void leave();
    bool notify(u32 id);
    IntroAction update(u32 controller_phase, MovieObservation movie);
    IntroWait waiting() const { return m_wait; }
    bool check_requested() const { return m_check_requested; }
    bool notification_seen() const { return m_notified; }
    bool post_intro_requested() const { return m_post_requested; }
private:
    bool m_active = false, m_check_requested = false, m_notified = false, m_post_requested = false;
    IntroWait m_wait = IntroWait::Inactive;
};
const char* wait_description(IntroWait wait);
IntroGate& intro_subset();
}  // namespace os::gge
