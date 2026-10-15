#include "engine/gge_star_phase1.h"
#include <unordered_set>
#include <utility>

namespace os::gge {
namespace {
bool valid_choice_control(u32 id,u32 stage) {
    if (id==stage+0x03744ff0u) return true;
    switch (id) {
    case kStarUiRoot: case 0x05b31710: case 0x03753f40: case 0x04656452: case 0x046a9ca7:
    case 0x02e71340: case 0x046ea568: case 0x068dac20: case 0x068dac21: case 0x046a9a21:
    case 0x046ea4b6: case 0x0594a640: case 0x05ff9352: case 0x05ff9ad2: case 0x05ff9d44:
    case 0x02e713a8: case 0x046ea523: case 0x026e0001: case 0x026e0002: case 0x02e712e4:
    case 0x046ea550: case 0x046a9a9b: case 0x046ea507: case 0x047e3cd3: return true;
    default: return false;
    }
}
}
bool plan_star_phase1(const StarUiInstance& instance,const StarPhase1Inputs& observed,
                      StarPhase1Plan& out,std::string& error) {
    if (!instance.active()) { error="phase1 host instance unavailable"; return false; }
    if (observed.choices.size()>64) { error="phase1 choice list exceeds host limit"; return false; }
    const auto& entry=instance.entry();
    std::unordered_set<u32> chosen;
    for (const auto& choice:observed.choices) {
        const auto matches=instance.control_objects(choice.control);
        bool exists=false; for (u16 object:matches) if (object==choice.object) exists=true;
        if (!valid_choice_control(choice.control,entry.stage)||!chosen.insert(choice.control).second||!exists) {
            error="invalid/duplicate phase1 control-object choice"; return false;
        }
    }
    const auto binding=[&](u32 control) {
        StarControlBinding result; result.control=control; result.candidates=instance.control_objects(control);
        for (const auto& choice:observed.choices) if (choice.control==control) {
            result.object=choice.object; result.status=StarBindingStatus::Found; return result;
        }
        result.status=result.candidates.empty()?StarBindingStatus::Missing:
            result.candidates.size()==1?StarBindingStatus::Found:StarBindingStatus::Ambiguous;
        if (result.status==StarBindingStatus::Found) result.object=result.candidates[0];
        return result;
    };
    StarPhase1Plan result; result.instance_id=instance.id();
    const auto emit=[&](Phase1Op op,u32 control)->Phase1Request& {
        Phase1Request request; request.operation=op; request.target=binding(control);
        if (op==Phase1Op::ImageResource||op==Phase1Op::StageImageSource||op==Phase1Op::StageImageTarget)
            request.required_drawable_interface=0xef3c47cfu;
        result.requests.push_back(std::move(request)); return result.requests.back();
    };
    const auto flag=[&](u32 control,u32 mask,bool value) {
        auto& request=emit(Phase1Op::Flag,control); request.mask=mask; request.flag_value=value;
    };
    const auto finish=[&](const char* blocked) {
        result.blocked_on=blocked; out=std::move(result); error.clear(); return true;
    };
    // Native missing root skips the phase body and still returns 1. Host records a missing target,
    // never claims failure was signalled by the original return or initializes a phase latch.
    flag(kStarUiRoot,1,true);
    if (result.requests.back().target.status!=StarBindingStatus::Found) return finish("positioning root binding unavailable");
    flag(0x05b31710u,1,false);
    for (u32 control:{0x03753f40u,0x04656452u,0x046a9ca7u}) {
        flag(control,1,false); emit(Phase1Op::CenterInRoot,control).source=binding(kStarUiRoot);
    }
    if (!observed.thumbnail) return finish("thumbnail key record+0x3C unobserved");
    if (observed.thumbnail->instance) {
        for (u32 control:{0x02e71340u,0x046ea568u}) emit(Phase1Op::ImageResource,control).resource=*observed.thumbnail;
    } else {
        flag(0x02e71340u,1,false); flag(0x046ea568u,1,false);
        flag(0x068dac20u,1,true); flag(0x068dac21u,1,true);
    }
    for (u32 control:{0x046a9a21u,0x046ea4b6u}) emit(Phase1Op::ImageResource,control).resource=entry.image;
    emit(Phase1Op::TooltipUserData,0x046ea4b6u).mask=0x037ac490u;
    std::optional<StarLocaleCaption> description;
    switch (entry.stage) {
    case 0: description=StarLocaleCaption{0x2b7a0311u,0x05b08702u}; break;
    case 1: description=StarLocaleCaption{0x2b7a0311u,0x05b08703u}; break;
    case 2: description=StarLocaleCaption{0x2b7a0311u,0x05b08704u}; break;
    case 4: description=StarLocaleCaption{0x2b7a0311u,0x05b08705u}; break;
    case 5: description=StarLocaleCaption{0x2b7a0311u,0x05b08706u}; break;
    default: break; // native string-builder default, not a guessed locale key or stage name
    }
    for (u32 control:{0x0594a640u,0x05ff9352u}) emit(Phase1Op::StageDescription,control).locale=description;
    if (!observed.has_profile) return finish("profile-pointer presence record+0x48 unobserved");
    if (*observed.has_profile) for (u32 control:{0x05ff9ad2u,0x05ff9d44u}) emit(Phase1Op::ProfileName,control);
    // Native 80DE40 layered-object operation is not approximated as a window flag change.
    auto& layer=emit(Phase1Op::LayerReference,0x05b31710u);
    layer.source=binding(entry.saved?0x05b347bcu:0x04656452u);
    if (!observed.selected_star_saved) return finish("selected StarRecord saved byte unobserved");
    if (*observed.selected_star_saved) emit(Phase1Op::SavedTerrainLayer,0x05b31710u);
    const auto stage_source=binding(entry.stage+0x03744ff0u);
    emit(Phase1Op::StageImageSource,entry.stage+0x03744ff0u);
    for (u32 control:{0x02e713a8u,0x046ea523u,0x026e0001u,0x026e0002u}) {
        auto& request=emit(Phase1Op::StageImageTarget,control);
        request.source=stage_source;
        request.resource=ImageKey{}; // native explicit zero triple if shared getter image is null
    }
    for (u32 control:{0x02e712e4u,0x046ea550u}) emit(Phase1Op::EntryName,control);
    if (!observed.selected_star_present) return finish("selected StarRecord presence for timestamp unobserved");
    if (*observed.selected_star_present) {
        for (u32 control:{0x046a9a9bu,0x046ea507u}) emit(Phase1Op::SavedTimestamp,control);
        // Native also sets a colour on 0x047E3CD3 here; keep unsupported service requirement visible.
        emit(Phase1Op::TimestampColour,0x047e3cd3u);
    }
    result.inspected_branches_observed=true;
    return finish("");
}
} // namespace os::gge
