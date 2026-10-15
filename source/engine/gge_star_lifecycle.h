#pragma once
#include "engine/gge_image_provider.h"
#include <memory>

namespace os::gge {
enum class StarHostActionKind { Phase1, ConfigurationFlag, StageFrame, AnimationPixels, StageCaption };
enum class StarHostActionStatus {
    Pending, Applied, ReferenceBound, Observed, Unchanged, IgnoredOutOfRange,
    Unknown, Missing, Ambiguous, Unsupported, Failed
};
struct StarHostAction {
    StarHostActionKind kind=StarHostActionKind::Phase1;
    Phase1Request request;
    std::optional<i32> frame;
    StarHostActionStatus status=StarHostActionStatus::Pending;
    u32 attempts=0;
    std::string detail;
};
struct StarHostReport {
    u64 ticket=0;
    u32 passes=0;
    std::string phase_blocked_on,configuration_blocked_on;
    std::vector<StarHostAction> actions;
};
struct StarLifecycleInputs {
    StarPhase1Inputs phase1;
    std::optional<bool> suppress_stage_text;
    // Separate choices: DF6D80 image requests and DF9200 frame requests have distinct interfaces.
    std::vector<StarControlChoice> configuration_choices;
};
// Synchronous owned HOST presentation lifecycle, NOT native mount/phase replay. Immutable input
// snapshot per open. Each pass tries independent actions in recorded order; failures stay explicit
// and are retried on a later pass; applied work/observations/known no-ops are not replayed.
// No native phase latch; this independent-action policy is not original dependency/skip ordering.
class StarUiLifecycle {
public:
    StarUiLifecycle()=default;
    StarUiLifecycle(const StarUiLifecycle&)=delete;
    StarUiLifecycle& operator=(const StarUiLifecycle&)=delete;
    bool open(u64 id,const spui::Layout& source,const GameEntry& entry,const StarLifecycleInputs& inputs,std::string& error);
    bool close(std::string& error);
    // A true return means the pass was accepted, NOT that all actions/assets/services succeeded.
    // nullptr provider yields Unknown asset results, not fake Missing/Found. Provider exceptions
    // become per-action failures. Reentrant open/close/apply/place rejected before mutation.
    bool apply_available(u64 ticket,ImageProvider* provider,std::string& error);
    // Scene-owned pixel placement only; no camera acquisition, native phase or move notifications.
    bool place(u64 ticket,const StarUiPlacement& placement,std::string& error);
    // Borrowed view: invalidated by successful open/close or destruction of this owner.
    const StarUiInstance* instance() const { return instance_.get(); }
    const StarHostReport& report() const { return report_; }
    bool native_mount_complete() const { return false; }
    bool native_services_executed() const { return false; }
private:
    std::unique_ptr<StarUiInstance> instance_;
    StarHostReport report_;
    u64 generation_=0;
    bool busy_=false;
};
}  // namespace os::gge
