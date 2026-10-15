#pragma once
#include "engine/gge_star_instance.h"

namespace os::gge {
enum class StarBindingStatus { Unknown, Found, Missing, Ambiguous };
struct StarControlBinding {
    u32 control=0;
    StarBindingStatus status=StarBindingStatus::Unknown;
    std::vector<u16> candidates;
    std::optional<u16> object;
};
struct StarControlChoice { u32 control=0; u16 object=0xffff; };
enum class Phase1Op {
    Flag, CenterInRoot, ImageResource, TooltipUserData, StageDescription,
    ProfileName, LayerReference, SavedTerrainLayer, StageImageSource, StageImageTarget,
    EntryName, SavedTimestamp, TimestampColour
};
struct Phase1Request {
    Phase1Op operation=Phase1Op::Flag;
    StarControlBinding target;
    u32 required_drawable_interface=0; // ImageResource/StageImage paths: 0xEF3C47CF, NOT Animated Icon
    u32 mask=0; bool flag_value=false;
    std::optional<ImageKey> resource; // nullopt means key unobserved, NOT observed zero key
    StarBindingStatus resource_status=StarBindingStatus::Unknown; // no resource lookup performed by planner
    std::optional<StarLocaleCaption> locale;
    // Relationship control (source for LayerReference/StageImageTarget); no native pointer stored.
    std::optional<StarControlBinding> source;
};
struct StarPhase1Inputs {
    std::optional<ImageKey> thumbnail; // original record+0x3C; zero instance selects original absent branch
    std::optional<bool> has_profile;  // original record+0x48 pointer, NOT native profile success
    std::optional<bool> selected_star_saved; // DE45C0 selected StarRecord+0xAC, not entry.saved
    std::optional<bool> selected_star_present; // later timestamp branch; independent observation
    std::vector<StarControlChoice> choices; // explicitly resolve duplicate ControlIDs, never automatic first match
};
struct StarPhase1Plan {
    u64 instance_id=0;
    std::vector<Phase1Request> requests; // original inspected order; truncated at unobserved branch
    std::string blocked_on; // missing dynamic branch observation, not a native return/error code
    bool inspected_branches_observed=false; // NOT execution, native mount success or service readiness
    bool native_services_executed=false;
    bool native_mount_complete=false;
};
// Static binding subset of DF6D80 phase1 CASE BODY, source order and explicit object selections.
// Caller/native current-phase guard, action execution, optional-control skips and result latches
// are NOT implemented here. Missing/Ambiguous requests stay visible for a future service executor.
// Found means one structural host target, NOT native interface/asset/plugin availability.
// Unknown dynamic inputs produce a bounded prefix + blocked_on (not silent absence). Invalid
// identity/choice/status inputs return false and leave output unchanged. No mutation or native call.
bool plan_star_phase1(const StarUiInstance& instance,const StarPhase1Inputs& observed,
                      StarPhase1Plan& out,std::string& error);
}  // namespace os::gge
