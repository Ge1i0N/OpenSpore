#include "engine/gge_star_scene.h"
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace os::gge {
namespace {
bool valid(const spui::Rect& r) {
    return std::isfinite(r.left)&&std::isfinite(r.top)&&std::isfinite(r.right)&&std::isfinite(r.bottom)&&
        std::isfinite(r.w())&&std::isfinite(r.h())&&r.valid();
}
const spui::Window* root(const StarUiInstance& instance) {
    for (const auto& w:instance.layout().windows) if (w.object_index==instance.root_object()) return &w;
    return nullptr;
}
}
void StarUiScene::retire_frame() { frame_={}; }
const StarUiLifecycle* StarUiScene::card(size_t index) const { return index<cards_.size()?cards_[index].lifecycle.get():nullptr; }
bool StarUiScene::open(const CatalogInput& catalog,const spui::Layout& source,const std::vector<StarSceneCardInput>& inputs,std::string& error) {
    if (busy_||generation_==std::numeric_limits<u64>::max()) { error="host scene busy/generation exhausted"; return false; }
    if (catalog.saved_status!=CatalogAvailability::Available||catalog.candidate_status!=CatalogAvailability::Available) {
        catalog_available_=false; retire_frame();
        error="host scene catalogue observations unavailable; prior cards retained for diagnostics only";
        frame_.ticket=ticket_; frame_.error=error; return false;
    }
    // Aggregate HOST presentation bounds, NOT recovered galaxy population limits.
    if (catalog.saved.size()>128||catalog.candidates.size()>128-catalog.saved.size()) { error="host scene exceeds 128-card limit"; return false; }
    std::vector<GameEntry> entries;
    if (!build_catalog(catalog,entries,error)) return false;
    if (inputs.size()!=entries.size()||(!entries.empty()&&
        (source.objects.size()>65536/entries.size()||source.windows.size()>32768/entries.size()))) {
        error="host scene input count/aggregate object/window budget mismatch"; return false;
    }
    std::unordered_set<u64> ids;
    for (const auto& input:inputs) if (!input.host_id||!ids.insert(input.host_id).second) {
        error="host scene requires unique nonzero caller-issued card identities"; return false;
    }
    std::vector<Card> next; next.reserve(entries.size());
    for (size_t i=0;i<entries.size();++i) {
        Card c; c.lifecycle=std::make_unique<StarUiLifecycle>(); c.insets=inputs[i].saved_insets;
        if (!c.lifecycle->open(inputs[i].host_id,source,entries[i],inputs[i].lifecycle,error)) return false;
        next.push_back(std::move(c));
    }
    cards_=std::move(next); ticket_=++generation_; catalog_available_=true; retire_frame(); error.clear(); return true;
}
bool StarUiScene::close(std::string& error) {
    if (busy_) { error="cannot close host scene during asset pass"; return false; }
    cards_.clear(); ticket_=0; catalog_available_=false; retire_frame(); error.clear(); return true;
}
bool StarUiScene::apply_available(u64 ticket,ImageProvider* provider,std::string& error) {
    if (busy_||!catalog_available_||!ticket||ticket!=ticket_) { error="inactive/unavailable/stale/reentrant host scene asset pass"; return false; }
    struct Guard { bool& busy; ~Guard() { busy=false; } } guard{busy_}; busy_=true;
    retire_frame(); // flags/visibility/captions/assets may change; require explicit projection publication
    for (auto& c:cards_) if (!c.lifecycle->apply_available(c.lifecycle->report().ticket,provider,error)) return false;
    error.clear(); return true;
}
bool StarUiScene::apply_phase_transition(u64 ticket,size_t index,const StarTransitionInputs& inputs,std::string& error) {
    if (busy_||!catalog_available_||!ticket||ticket!=ticket_||index>=cards_.size()) {
        error="inactive/unavailable/stale/reentrant/out-of-range scene phase request"; return false;
    }
    auto& c=*cards_[index].lifecycle;
    if (!c.apply_phase_transition(c.report().ticket,inputs,error)) return false;
    retire_frame(); // visibility/enable/caption state may differ; no implicit mouse->phase dispatch
    return true;
}
bool StarUiScene::update(u64 ticket,const SceneCameraObservation& camera,std::string& error) {
    if (busy_||!catalog_available_||!ticket||ticket!=ticket_||frame_serial_==std::numeric_limits<u64>::max()) {
        error="inactive/unavailable/stale/reentrant host scene update/frame exhausted"; return false;
    }
    retire_frame(); frame_.ticket=ticket_; frame_.serial=++frame_serial_; frame_.camera_token=camera.token;
    const auto fail=[&](const char* why) { error=why; frame_.error=why; return false; };
    if (camera.source!=SceneCameraSource::ActiveProjection||!camera.token)
        return fail("active projection camera identity unobserved (controller-retained interface is not a substitute)");
    const auto& observation=camera.projection;
    if (!observation.clip_from_world||!observation.window_rect) return fail("active camera matrix/window observation unavailable");
    if (!valid(*observation.window_rect)) return fail("invalid camera window extents");
    for (float v:*observation.clip_from_world) if (!std::isfinite(v)) return fail("nonfinite active camera matrix");
    frame_.viewport={0,0,observation.window_rect->w(),observation.window_rect->h()};
    frame_.cards.reserve(cards_.size());
    for (size_t i=0;i<cards_.size();++i) {
        auto& c=cards_[i]; const auto& instance=*c.lifecycle->instance();
        StarSceneCardFrame result; result.entry_index=i; result.host_id=instance.id();
        ProjectedPoint point;
        if (!project_star(instance.entry().position,observation,point,result.error)) result.status=StarSceneCardStatus::ProjectionFailed;
        else {
            result.projection=point; PlacementObservation placement; placement.window_rect=observation.window_rect; placement.saved_insets=c.insets;
            if (const auto* r=root(instance)) { placement.root_area=r->area; placement.current_position=std::array<float,2>{r->area.left,r->area.top}; }
            StarUiPlacement move;
            if (!place_star_ui(point,instance.entry().saved,placement,move,result.error)) result.status=StarSceneCardStatus::PlacementFailed;
            else {
                result.placement=move;
                result.status=c.lifecycle->place(c.lifecycle->report().ticket,move,result.error)?
                    StarSceneCardStatus::Positioned:StarSceneCardStatus::PlacementFailed;
            }
        }
        frame_.cards.push_back(std::move(result));
    }
    frame_.status=StarSceneFrameStatus::Projected; error.clear(); return true;
}
std::vector<StarSceneLayer> StarUiScene::layers(u64 serial) const {
    std::vector<StarSceneLayer> result;
    if (busy_||!catalog_available_||!ticket_||!serial||serial!=frame_.serial||frame_.status!=StarSceneFrameStatus::Projected) return result;
    for (const auto& f:frame_.cards) if (f.status==StarSceneCardStatus::Positioned) {
        const auto& instance=*cards_[f.entry_index].lifecycle->instance();
        if (instance.entry().visible) result.push_back({instance.id(),f.entry_index,&instance.layout()});
    }
    return result;
}
std::optional<StarSceneHit> StarUiScene::pick(u64 serial,float x,float y) const {
    if (!std::isfinite(x)||!std::isfinite(y)) return std::nullopt;
    const auto visible=layers(serial);
    for (auto it=visible.rbegin();it!=visible.rend();++it) {
        const auto& instance=*cards_[it->entry_index].lifecycle->instance();
        if (const auto* w=instance.pick(x,y,frame_.viewport))
            return StarSceneHit{ticket_,serial,it->host_id,it->entry_index,w->object_index,w->control_id,w->command_id};
    }
    return std::nullopt;
}
}  // namespace os::gge
