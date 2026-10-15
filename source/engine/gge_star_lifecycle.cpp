#include "engine/gge_star_lifecycle.h"
#include <limits>
#include <map>
#include <tuple>
#include <unordered_set>
#include <utility>

namespace os::gge {
namespace {
using Status=StarHostActionStatus;
bool terminal(Status s) {
    return s==Status::Applied||s==Status::ReferenceBound||s==Status::Observed||s==Status::Unchanged||s==Status::IgnoredOutOfRange;
}
void increment(u32& n) { if (n!=std::numeric_limits<u32>::max()) ++n; }
StarControlBinding bind(const StarUiInstance& instance,u32 control,const std::vector<StarControlChoice>& choices) {
    StarControlBinding b; b.control=control; b.candidates=instance.control_objects(control);
    for (const auto& c:choices) if (c.control==control) { b.object=c.object; b.status=StarBindingStatus::Found; return b; }
    b.status=b.candidates.empty()?StarBindingStatus::Missing:b.candidates.size()==1?StarBindingStatus::Found:StarBindingStatus::Ambiguous;
    if (b.status==StarBindingStatus::Found) b.object=b.candidates[0];
    return b;
}
bool resolve(const StarUiInstance& instance,const StarControlBinding& b,StarHostAction& action) {
    switch (b.status) {
    case StarBindingStatus::Missing: action.status=Status::Missing; action.detail="target absent in explicit layout"; return false;
    case StarBindingStatus::Ambiguous: action.status=Status::Ambiguous; action.detail="explicit object choice required"; return false;
    case StarBindingStatus::Unknown: action.status=Status::Unknown; action.detail="target unobserved"; return false;
    default: break;
    }
    if (b.object) for (const auto& w:instance.layout().windows) if (w.object_index==*b.object&&w.control_id==b.control) return true;
    action.status=Status::Failed; action.detail="object/control binding no longer matches instance"; return false;
}
Status asset_status(ImageReadStatus s) {
    switch (s) {
    case ImageReadStatus::Found: return Status::Applied;
    case ImageReadStatus::Missing: return Status::Missing;
    case ImageReadStatus::Ambiguous: return Status::Ambiguous;
    case ImageReadStatus::Unsupported: return Status::Unsupported;
    case ImageReadStatus::ReadFailure: return Status::Failed;
    default: return Status::Unknown;
    }
}
Status applied_status(ImageApplyStatus s) {
    switch (s) {
    case ImageApplyStatus::Applied: return Status::Applied;
    case ImageApplyStatus::Missing: return Status::Missing;
    case ImageApplyStatus::Ambiguous: return Status::Ambiguous;
    case ImageApplyStatus::Unsupported: return Status::Unsupported;
    case ImageApplyStatus::ReadFailure: return Status::Failed;
    default: return Status::Unknown;
    }
}
// One bounded pass, one result per exact key, including failures. No cross-pass cache/precedence.
class PassImages final:public ImageProvider {
public:
    explicit PassImages(ImageProvider* source):source_(source) {}
    ImageRead read(const ImageKey& key) override {
        const auto id=std::make_tuple(key.instance,key.type,key.group);
        if (const auto it=reads_.find(id);it!=reads_.end()) return it->second;
        ImageRead result;
        if (!source_) result.error="image provider unobserved";
        else {
            try { result=source_->read(key); }
            catch (...) { result.status=ImageReadStatus::ReadFailure; result.error="image provider threw during host pass"; }
        }
        reads_.emplace(id,result); return result;
    }
private:
    ImageProvider* source_;
    std::map<std::tuple<u32,u32,u32>,ImageRead> reads_;
};
void apply_action(StarUiInstance& instance,StarHostAction& action,PassImages& images) {
    action.detail.clear(); increment(action.attempts);
    const auto& request=action.request;
    if (!resolve(instance,request.target,action)) return;
    const u16 object=*request.target.object;
    const auto done=[&](bool ok,Status success=Status::Applied) { action.status=ok?success:Status::Failed; };
    if (action.kind==StarHostActionKind::AnimationPixels) {
        spui::AnimationDescriptor descriptor;
        if (!instance.animation(object,descriptor,action.detail)) { action.status=Status::Unsupported; return; }
        const auto& r=descriptor.image.resource;
        auto result=images.read({r.instance,r.type,r.group}); action.status=asset_status(result.status); action.detail=result.error;
        if (result.status!=ImageReadStatus::Found) return;
        if (!result.image.pixels||!spui::valid_image_binding(result.image)) { action.status=Status::Failed; action.detail="provider Found lacks valid decoded image"; return; }
        done(instance.set_animation_pixels(object,result.image.pixels,action.detail)); return;
    }
    if (action.kind==StarHostActionKind::StageFrame) {
        StarUiInstance::FrameChange change;
        if (!action.frame||!instance.set_animation_frame(object,*action.frame,change,action.detail)) { action.status=Status::Unsupported; return; }
        action.status=change==StarUiInstance::FrameChange::Changed?Status::Applied:
            change==StarUiInstance::FrameChange::Unchanged?Status::Unchanged:Status::IgnoredOutOfRange;
        return;
    }
    if (action.kind==StarHostActionKind::StageCaption) {
        if (!request.locale) { action.status=Status::Unknown; action.detail="caption reference unobserved"; return; }
        done(instance.set_caption_reference(object,*request.locale,action.detail),Status::ReferenceBound); return;
    }
    if (action.kind==StarHostActionKind::ConfigurationFlag) {
        done(instance.set_window_flag(object,request.mask,request.flag_value,action.detail)); return;
    }
    switch (request.operation) {
    case Phase1Op::Flag: done(instance.set_window_flag(object,request.mask,request.flag_value,action.detail)); return;
    case Phase1Op::CenterInRoot:
        if (!request.source) { action.status=Status::Unknown; action.detail="centering source unobserved"; return; }
        if (!resolve(instance,*request.source,action)) return;
        if (request.source->object!=instance.root_object()) { action.status=Status::Unsupported; action.detail="centering source is not positioning root"; return; }
        done(instance.center_in_root(object,action.detail)); return;
    case Phase1Op::ImageResource: case Phase1Op::StageImageTarget: {
        auto result=apply_star_image(instance,instance.id(),request,images);
        action.status=applied_status(result.status); action.detail=std::move(result.error); return;
    }
    case Phase1Op::StageDescription:
        if (!request.locale) { action.status=Status::Unsupported; action.detail="native string-builder default not implemented"; return; }
        done(instance.set_caption_reference(object,*request.locale,action.detail),Status::ReferenceBound); return;
    case Phase1Op::EntryName: done(instance.set_caption_text(object,instance.entry().name,action.detail)); return;
    case Phase1Op::StageImageSource: {
        spui::ImageBinding image;
        if (!instance.current_image(object,image,action.detail)) { action.status=Status::Unknown; return; }
        action.status=Status::Observed; action.detail="getter descriptor observed, not resource readiness"; return;
    }
    default: action.status=Status::Unsupported; action.detail="native service/listener/layer/tooltip/profile/timestamp action not implemented"; return;
    }
}
}
bool StarUiLifecycle::open(u64 id,const spui::Layout& source,const GameEntry& entry,const StarLifecycleInputs& inputs,std::string& error) {
    if (busy_||generation_==std::numeric_limits<u64>::max()) { error="host lifecycle busy/generation exhausted"; return false; }
    auto next=std::make_unique<StarUiInstance>();
    if (!next->instantiate(id,source,entry,error)) return false;
    if (inputs.configuration_choices.size()>16) { error="configuration choices exceed host bound"; return false; }
    std::unordered_set<u32> chosen;
    for (const auto& c:inputs.configuration_choices) {
        const bool allowed=c.control==0x026e0001u||c.control==0x026e0002u||c.control==0x03c4925eu||
            c.control==0x060a1102u||c.control==0x060a1103u||c.control==0x0375408cu||c.control==0x05944088u;
        bool exists=false; for (u16 object:next->control_objects(c.control)) if (object==c.object) exists=true;
        if (!allowed||!exists||!chosen.insert(c.control).second) { error="invalid/duplicate configuration object choice"; return false; }
    }
    StarPhase1Plan plan;
    if (!plan_star_phase1(*next,inputs.phase1,plan,error)) return false;
    StarHostReport report; report.ticket=generation_+1; report.phase_blocked_on=plan.blocked_on;
    for (auto& request:plan.requests) { StarHostAction action; action.request=std::move(request); report.actions.push_back(std::move(action)); }
    StarUiConfiguration configuration;
    if (configure_star_ui(entry,inputs.suppress_stage_text,configuration,report.configuration_blocked_on)) {
        const auto emit=[&](StarHostActionKind kind,u32 control)->StarHostAction& {
            StarHostAction action; action.kind=kind; action.request.target=bind(*next,control,inputs.configuration_choices);
            report.actions.push_back(std::move(action)); return report.actions.back();
        };
        if (configuration.stage_icon_index) for (u32 control:{0x026e0001u,0x026e0002u}) {
            emit(StarHostActionKind::StageFrame,control).frame=i32(*configuration.stage_icon_index);
            emit(StarHostActionKind::AnimationPixels,control);
        }
        if (configuration.stage_caption) emit(StarHostActionKind::StageCaption,0x03c4925eu).request.locale=configuration.stage_caption;
        for (const auto& flag:configuration.flag_changes) {
            auto& a=emit(StarHostActionKind::ConfigurationFlag,flag.control); a.request.mask=flag.mask; a.request.flag_value=flag.value;
        }
    }
    instance_=std::move(next); report_=std::move(report); generation_=report_.ticket; error.clear(); return true;
}
bool StarUiLifecycle::close(std::string& error) {
    if (busy_) { error="cannot close during synchronous host pass"; return false; }
    instance_.reset(); report_={}; error.clear(); return true;
}
bool StarUiLifecycle::apply_available(u64 ticket,ImageProvider* provider,std::string& error) {
    if (busy_||!instance_||!ticket||ticket!=report_.ticket) { error="inactive/stale/reentrant host pass"; return false; }
    struct Guard { bool& busy; ~Guard() { busy=false; } } guard{busy_}; busy_=true;
    PassImages images(provider);
    for (auto& action:report_.actions) if (!terminal(action.status)) apply_action(*instance_,action,images);
    increment(report_.passes); error.clear(); return true;
}
}  // namespace os::gge
