#include "engine/gge_star_transition.h"
#include <unordered_set>
#include <utility>

namespace os::gge {
namespace {
bool relevant(u32 phase,u32 control) {
    if (control==0x05b31710u) return true;
    if (phase==2) return control==0x046a9ca7u||control==0x04656452u;
    if (phase==3) return control==0x046a9ca7u||control==0x02e712e4u||control==0x046ea550u;
    if (phase==4) return control==0x04656452u;
    return control==0x04656452u||control==0x046a9ca7u||control==0x03753f40u||control==0x05b34d93u;
}
}
bool plan_star_transition(const StarUiInstance& instance,const StarTransitionInputs& inputs,StarTransitionPlan& out,std::string& error) {
    if (!instance.active()||inputs.requested_case<2||inputs.requested_case>5||inputs.choices.size()>16) {
        error="inactive instance/unsupported transition case/host choice bound"; return false;
    }
    std::unordered_set<u32> seen;
    for (const auto& c:inputs.choices) {
        bool exists=false; for (u16 object:instance.control_objects(c.control)) if (object==c.object) exists=true;
        if (!relevant(inputs.requested_case,c.control)||!exists||!seen.insert(c.control).second) {
            error="invalid/unrelated/duplicate transition control-object choice"; return false;
        }
    }
    const auto binding=[&](u32 control) {
        StarControlBinding b; b.control=control; b.candidates=instance.control_objects(control);
        for (const auto& c:inputs.choices) if (c.control==control) { b.object=c.object; b.status=StarBindingStatus::Found; return b; }
        b.status=b.candidates.empty()?StarBindingStatus::Missing:b.candidates.size()==1?StarBindingStatus::Found:StarBindingStatus::Ambiguous;
        if (b.status==StarBindingStatus::Found) b.object=b.candidates[0];
        return b;
    };
    StarTransitionPlan plan; plan.instance_id=instance.id(); plan.requested_case=inputs.requested_case;
    plan.observed_current_case=inputs.observed_current_case;
    if (!inputs.observed_current_case) plan.blocked_on="native current phase UI-data+2C unobserved";
    else if (*inputs.observed_current_case==inputs.requested_case) plan.guard=StarTransitionGuard::AlreadyCurrent;
    else {
        plan.guard=StarTransitionGuard::CaseBody;
        const auto emit=[&](StarTransitionOp op,u32 control)->StarTransitionRequest& {
            StarTransitionRequest r; r.operation=op; if (op!=StarTransitionOp::ServiceEvent) r.target=binding(control);
            plan.requests.push_back(std::move(r)); return plan.requests.back();
        };
        const auto flag=[&](u32 control,bool value) { auto& r=emit(StarTransitionOp::Flag,control); r.mask=1; r.value=value; };
        emit(StarTransitionOp::ServiceEvent,0).event=inputs.requested_case<=3?0xd06d9182u:
            inputs.requested_case==4?0xd2fc3ad5u:0x6c6da889u;
        switch (inputs.requested_case) {
        case 2: flag(0x046a9ca7u,false); flag(0x04656452u,true); flag(0x05b31710u,true); break;
        case 3:
            flag(0x046a9ca7u,true); emit(StarTransitionOp::ServiceControlRequest,0x046a9ca7u).value=true;
            emit(StarTransitionOp::EntryName,0x02e712e4u); emit(StarTransitionOp::EntryName,0x046ea550u);
            flag(0x05b31710u,true); break;
        case 4: flag(0x04656452u,false); flag(0x05b31710u,false); break;
        case 5:
            flag(0x04656452u,false); flag(0x046a9ca7u,false); flag(0x03753f40u,true);
            emit(StarTransitionOp::CopyWindowRect,0x05b31710u).source=binding(0x05b34d93u);
            flag(0x05b31710u,true); break;
        }
    }
    out=std::move(plan); error.clear(); return true;
}
}  // namespace os::gge
