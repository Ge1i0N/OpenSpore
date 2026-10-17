#pragma once
#include "engine/gge_catalog.h"
#include "engine/spui.h"
#include <array>
#include <optional>

namespace os::gge {
constexpr u32 kStarUiLayout=0x21df01f2u, kStarUiRoot=0x02e5a4a8u;
struct CameraObservation {
    // GA cViewer +0xC0..+0xFC, indexed in memory order; NOT an ICamera object's memory.
    // GameModeManager+58 returns viewer; manager+50 -> CameraManager+38 returns ICamera.
    std::optional<std::array<float,16>> clip_from_world;
    std::optional<spui::Rect> window_rect;
};
enum class ViewerProjectionKind { Perspective=1, Parallel=2, PerspectiveInfinite=3 };
struct ViewerProjectionInputs {
    // Explicit viewer+40 matrix (already inverse of viewer+00 view transform). No inversion or
    // camera construction here. All arrays use original memory order, no implicit identity.
    std::optional<std::array<float,16>> world_to_view;
    // Fields of viewer+170 raster-camera, not controller+198 ICamera.
    std::optional<ViewerProjectionKind> kind; // raster-camera+6C
    std::optional<std::array<float,2>> view_window, view_offset; // +5C and +54
    std::optional<float> near_plane, far_plane; // +70/+74; both required by this bounded subset
};
// GA 7C4370 projection ->7C4A10 second/third row exchange ->45EF90 matrix product ->viewer+C0.
// No native camera update, viewer acquisition, FOV/aspect defaults, raster or scene publication.
// Unknown/unsupported/nonfinite/degenerate input returns false with output unchanged.
bool compose_viewer_projection(const ViewerProjectionInputs& observed,std::array<float,16>& out,std::string& error);
struct CameraFovObservation {
    // Explicit controller fields +28/+2C/+34; -1 is the native unset sentinel, not Unknown.
    std::optional<float> current, target, snap_threshold;
};
struct CameraFovStep {
    // DDFB90 requests viewer FOV from OLD current before updating the controller's current value.
    std::optional<float> requested_degrees;
    float next_current=0;
};
// DDFCE6/DDFDC5..FE67 arithmetic subset only: no transform/audio/global-state/update scheduling.
bool step_camera_fov(const CameraFovObservation& observed,CameraFovStep& out,std::string& error);
struct ProjectedPoint { float x=0,y=0,ndc_z=0,clip_w=0; };
// GA 0x7C4250 + 0xDE3C20: perspective divide and window EXTENTS, no origin addition/clipping.
// Rejecting unknown/nonfinite/zero-w/invalid extents is host safety policy. Negative w is not culled.
bool project_star(const StarPosition& world,const CameraObservation& observed,ProjectedPoint& out,std::string& error);
struct SavedInsets { float left=0,top=0,right=0,bottom=0; };
struct PlacementObservation {
    std::optional<spui::Rect> root_area, window_rect;
    std::optional<std::array<float,2>> current_position;
    std::optional<SavedInsets> saved_insets;
};
struct StarUiPlacement { float x=0,y=0; bool move_required=false; };
// GA 0xDF6110: center root, clamp saved only, request move iff either axis delta >4 pixels.
// Missing root/current/window/insets is blocked, not replaced with a screen-edge guess.
bool place_star_ui(const ProjectedPoint& point,bool saved,const PlacementObservation& observed,
                   StarUiPlacement& out,std::string& error);
struct StarLocaleCaption { u32 table=0,key=0; };
struct StarFlagChange { u32 control=0,mask=0; bool value=false; };
struct StarUiConfiguration {
    std::optional<u32> stage_icon_index; // both controls 0x026E0001/2
    std::optional<StarLocaleCaption> stage_caption; // control 0x03C4925E
    std::vector<StarFlagChange> flag_changes; // raw GA mask/value, not guessed flag names
};
// GA 0xDF9200 subset after successful UI loading. Suppression flag is 0x16A0F88 observation,
// required for saved entries; no original mounting/image-plugin/locale/event calls are performed.
bool configure_star_ui(const GameEntry& entry,std::optional<bool> suppress_stage_text,
                       StarUiConfiguration& out,std::string& error);
}  // namespace os::gge
