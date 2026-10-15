#pragma once
#include "engine/gge_star_lifecycle.h"

namespace os::gge {
enum class SceneCameraSource { Unknown, ActiveProjection, ControllerRetained };
struct SceneCameraObservation {
    SceneCameraSource source=SceneCameraSource::Unknown;
    u64 token=0; // caller-issued observation identity, NOT native pointer/service availability
    CameraObservation projection;
};
struct StarSceneCardInput {
    u64 host_id=0; // unique host identity; catalogue ordinal/star id/source index are separate
    StarLifecycleInputs lifecycle;
    std::optional<SavedInsets> saved_insets;
};
enum class StarSceneFrameStatus { Unavailable, Projected };
enum class StarSceneCardStatus { Unknown, Positioned, ProjectionFailed, PlacementFailed };
struct StarSceneCardFrame {
    u64 host_id=0;
    size_t entry_index=0;
    StarSceneCardStatus status=StarSceneCardStatus::Unknown;
    std::optional<ProjectedPoint> projection;
    std::optional<StarUiPlacement> placement;
    std::string error;
};
struct StarSceneFrame {
    u64 ticket=0,serial=0,camera_token=0;
    StarSceneFrameStatus status=StarSceneFrameStatus::Unavailable;
    spui::Rect viewport; // zero-origin extents, matching DE3C20 (no window-origin addition)
    std::vector<StarSceneCardFrame> cards;
    std::string error;
};
struct StarSceneLayer { u64 host_id=0; size_t entry_index=0; const spui::Layout* layout=nullptr; };
struct StarSceneHit {
    u64 ticket=0,frame_serial=0,host_id=0;
    size_t entry_index=0;
    u16 window_object=0xffff;
    u32 control=0,command=0; // observed IDs only; NOT CommandID dispatch/native selected-star change
};
struct StarWorldRayObservation {
    SceneCameraSource source=SceneCameraSource::Unknown;
    u64 token=0;
    StarPosition origin, direction;
    float max_distance=15.f; // GA DF6490 initial selection threshold (0x1400864)
};
struct StarWorldHit {
    u64 ticket=0;
    size_t entry_index=0;
    u64 host_id=0;
    float distance=0;
};
// HOST 2D projection/card collection, not native galaxy/effects/scene factory completion.
// Requires explicit Available saved+candidate catalogue lists; no discovery/generation/default camera.
// Catalogue order is forward paint/reverse pick HOST policy, not native dynamic/depth/Z ordering.
class StarUiScene {
public:
    StarUiScene()=default;
    StarUiScene(const StarUiScene&)=delete;
    StarUiScene& operator=(const StarUiScene&)=delete;
    // Invalid construction preserves the prior snapshot; unavailable catalogue observations instead
    // suspend it for diagnostics only until a successful Available reopen (never treat Unknown empty).
    bool open(const CatalogInput& catalog,const spui::Layout& source,const std::vector<StarSceneCardInput>& cards,std::string& error);
    bool close(std::string& error);
    bool apply_available(u64 ticket,ImageProvider* provider,std::string& error);
    // Explicit case/current observation only; no mouse/selection -> native phase dispatch. Accepted
    // requests retire draw/pick views, including blocked/equal guards; invalid requests preserve them.
    bool apply_phase_transition(u64 ticket,size_t entry_index,const StarTransitionInputs& inputs,std::string& error);
    // Every accepted update retires the previous frame, even when camera is unavailable/invalid.
    // Global camera failure returns false; per-card failures are reported, excluded from layers/hits.
    bool update(u64 ticket,const SceneCameraObservation& camera,std::string& error);
    const StarSceneFrame& frame() const { return frame_; }
    u64 ticket() const { return ticket_; }
    size_t size() const { return cards_.size(); }
    CatalogAvailability catalog_status() const { return catalog_available_?CatalogAvailability::Available:CatalogAvailability::Unknown; }
    const StarUiLifecycle* card(size_t index) const;
    // Borrowed layouts valid until mutation/open/close/destruction. Stale/Unavailable frames yield no
    // layers/hits; a paint callback should not mutate this owner while consuming these views.
    std::vector<StarSceneLayer> layers(u64 frame_serial) const;
    std::optional<StarSceneHit> pick(u64 frame_serial,float x,float y) const;
    // DF6490-like world ray selection with explicit unprojected ray. It never invokes the native
    // F8 hover/FC selected-entry callbacks, changes phase, or dispatches the returned UI IDs.
    std::optional<StarWorldHit> world_pick(u64 ticket,const StarWorldRayObservation& ray) const;
    bool native_scene_ready() const { return false; }
private:
    struct Card { std::unique_ptr<StarUiLifecycle> lifecycle; std::optional<SavedInsets> insets; };
    std::vector<Card> cards_;
    u64 generation_=0,ticket_=0,frame_serial_=0;
    StarSceneFrame frame_;
    bool busy_=false;
    bool catalog_available_=false;
    void retire_frame();
};
}  // namespace os::gge
