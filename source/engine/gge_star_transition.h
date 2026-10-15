#pragma once
#include "engine/gge_star_phase1.h"

namespace os::gge {
enum class StarTransitionOp { ServiceEvent, Flag, ServiceControlRequest, EntryName, CopyWindowRect };
enum class StarTransitionGuard { UnknownCurrent, AlreadyCurrent, CaseBody };
struct StarTransitionInputs {
    u32 requested_case=0; // only 2..5 in this subset; 1 remains the separate phase1 planner
    std::optional<u32> observed_current_case; // native UI-data+2C, not host action success
    std::vector<StarControlChoice> choices; // explicit per-transition choices, including duplicates
};
struct StarTransitionRequest {
    StarTransitionOp operation=StarTransitionOp::Flag;
    StarControlBinding target;
    std::optional<StarControlBinding> source;
    u32 event=0,mask=0;
    bool value=false;
};
struct StarTransitionPlan {
    u64 instance_id=0;
    u32 requested_case=0;
    std::optional<u32> observed_current_case;
    StarTransitionGuard guard=StarTransitionGuard::UnknownCurrent;
    std::vector<StarTransitionRequest> requests;
    std::string blocked_on;
};
// DF6D80 cases2..5 CASE BODY + equality guard; no choice of case from mouse/selection is inferred.
// Invalid input fails transactionally; Unknown current returns a blocked plan, not observed absence.
bool plan_star_transition(const StarUiInstance& instance,const StarTransitionInputs& inputs,
                          StarTransitionPlan& out,std::string& error);
}  // namespace os::gge
