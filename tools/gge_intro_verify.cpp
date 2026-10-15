#include "engine/gge_intro.h"
#include <iostream>
using namespace os;
using namespace os::gge;

int main() {
    IntroGate gate;
    unsigned checks = 0;
    const auto require = [&](bool ok, const char* why) {
        ++checks;
        if (!ok) std::cerr << "GGE intro gate: " << why << '\n';
        return ok;
    };
    if (!require(gate.update(6, MovieObservation::Idle) == IntroAction::None &&
                 !gate.notify(kDbCheckNotification), "inactive gate acts")) return 1;
    gate.enter();
    if (!require(!gate.notify(0x12345678) && !gate.notification_seen(), "unrelated event accepted")) return 1;
    for (u32 phase = 0; phase < 6; ++phase)
        if (!require(gate.update(phase, MovieObservation::Idle) == IntroAction::None &&
                     !gate.check_requested(), "early phase starts DBCheck")) return 1;
    for (u32 phase : {7u, 8u, 9u, 10u, 0xffffffffu})
        if (!require(gate.update(phase, MovieObservation::Idle) == IntroAction::None &&
                     !gate.check_requested(), "non-six phase starts DBCheck")) return 1;
    for (int frame = 0; frame < 1000; ++frame)
        if (!require(gate.update(6, MovieObservation::Unknown) == IntroAction::None &&
                     gate.waiting() == IntroWait::MovieService && !gate.check_requested(),
                     "missing service auto-completes")) return 1;
    if (!require(gate.update(6, MovieObservation::Playing) == IntroAction::None &&
                 gate.waiting() == IntroWait::MoviePlaying, "playing service bypassed")) return 1;
    if (!require(gate.update(6, MovieObservation::Idle) == IntroAction::StartDbCheck &&
                 gate.check_requested() && !gate.post_intro_requested(), "DBCheck not requested once")) return 1;
    for (int frame = 0; frame < 1000; ++frame)
        if (!require(gate.update(6, MovieObservation::Idle) == IntroAction::None &&
                     gate.waiting() == IntroWait::DbNotification && !gate.post_intro_requested(),
                     "DB notification fabricated")) return 1;
    if (!require(gate.notify(kDbCheckNotification) && gate.notification_seen(), "notification not latched")) return 1;
    if (!require(gate.update(6, MovieObservation::Playing) == IntroAction::None &&
                 !gate.post_intro_requested(), "movie guard skipped after notification")) return 1;
    if (!require(gate.update(6, MovieObservation::Idle) == IntroAction::RunPostIntro &&
                 gate.post_intro_requested(), "post-intro action not requested")) return 1;
    for (int n = 0; n < 20; ++n) {
        gate.notify(kDbCheckNotification);
        if (!require(gate.update(6, MovieObservation::Idle) == IntroAction::None,
                     "duplicate post-intro action")) return 1;
    }
    gate.leave();
    if (!require(gate.waiting() == IntroWait::Inactive && !gate.check_requested() &&
                 !gate.notification_seen() && !gate.post_intro_requested(), "exit retains state")) return 1;
    gate.enter();
    gate.notify(kDbCheckNotification);
    if (!require(gate.update(6, MovieObservation::Unknown) == IntroAction::None &&
                 gate.update(6, MovieObservation::Idle) == IntroAction::StartDbCheck &&
                 gate.update(6, MovieObservation::Idle) == IntroAction::RunPostIntro,
                 "early notification collapses start/advance into one update")) return 1;
    std::cout << checks << " GGE post-title guard/latch/reset checks PASS (controlled signals, not original runtime)\n";
    return 0;
}
