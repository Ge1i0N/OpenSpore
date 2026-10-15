#include "engine/gge_star_present.h"
#include <algorithm>
#include <cmath>
#include <utility>

namespace os::gge {
namespace {
bool valid(const spui::Rect& r) {
    return std::isfinite(r.left)&&std::isfinite(r.top)&&std::isfinite(r.right)&&std::isfinite(r.bottom)&&
        std::isfinite(r.w())&&std::isfinite(r.h())&&r.w()>0&&r.h()>0;
}
}
bool project_star(const StarPosition& world,const CameraObservation& observed,ProjectedPoint& out,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (!observed.clip_from_world || !observed.window_rect) return fail("star projection camera/window observations unavailable");
    if (!valid(*observed.window_rect) || !std::isfinite(world.x)||!std::isfinite(world.y)||!std::isfinite(world.z))
        return fail("star projection invalid position/window extents");
    const auto& m=*observed.clip_from_world;
    for (float v:m) if (!std::isfinite(v)) return fail("star projection nonfinite matrix");
    const float w=m[11]*world.z+m[7]*world.y+m[3]*world.x+m[15];
    if (!std::isfinite(w)||w==0) return fail("star projection zero/nonfinite homogeneous w");
    const float inverse=1.0f/w;
    const float x=inverse*(m[8]*world.z+m[4]*world.y+m[0]*world.x+m[12]);
    const float y=inverse*(m[9]*world.z+m[5]*world.y+m[1]*world.x+m[13]);
    ProjectedPoint result;
    result.ndc_z=inverse*(m[10]*world.z+m[6]*world.y+m[2]*world.x+m[14]);
    result.clip_w=w;
    // GA DE3C20 uses width/height only, even when the observed window rectangle origin is nonzero.
    result.x=observed.window_rect->w()*(x+1.0f)*0.5f;
    result.y=(1.0f-(y+1.0f)*0.5f)*observed.window_rect->h();
    if (!std::isfinite(result.x)||!std::isfinite(result.y)||!std::isfinite(result.ndc_z))
        return fail("star projection nonfinite result");
    out=result; error.clear(); return true;
}
bool place_star_ui(const ProjectedPoint& point,bool saved,const PlacementObservation& observed,
                   StarUiPlacement& out,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (!observed.root_area || !observed.window_rect || !observed.current_position || (saved && !observed.saved_insets))
        return fail("star UI placement root/window/current/inset observations unavailable");
    if (!valid(*observed.root_area)||!valid(*observed.window_rect)||!std::isfinite(point.x)||!std::isfinite(point.y)||
        !std::isfinite((*observed.current_position)[0])||!std::isfinite((*observed.current_position)[1]))
        return fail("star UI placement invalid geometry");
    StarUiPlacement result;
    result.x=point.x-observed.root_area->w()*0.5f;
    result.y=point.y-observed.root_area->h()*0.5f;
    if (saved) {
        const auto& i=*observed.saved_insets;
        const float max_x=observed.window_rect->w()-i.right, max_y=observed.window_rect->h()-i.bottom;
        if (!std::isfinite(i.left)||!std::isfinite(i.top)||!std::isfinite(i.right)||!std::isfinite(i.bottom)||
            !std::isfinite(max_x)||!std::isfinite(max_y)||i.left>max_x||i.top>max_y)
            return fail("star UI placement invalid saved clamp bounds");
        result.x=std::clamp(result.x,i.left,max_x); result.y=std::clamp(result.y,i.top,max_y);
    }
    if (!std::isfinite(result.x)||!std::isfinite(result.y)) return fail("star UI placement nonfinite result");
    result.move_required=std::abs((*observed.current_position)[0]-result.x)>4.0f ||
                         std::abs((*observed.current_position)[1]-result.y)>4.0f;
    out=result; error.clear(); return true;
}
bool configure_star_ui(const GameEntry& entry,std::optional<bool> suppress,StarUiConfiguration& out,std::string& error) {
    if (entry.saved && !suppress) { error="star UI stage-text suppression observation unavailable"; return false; }
    StarUiConfiguration result;
    if (entry.saved) {
        u32 index=0,key=0x067735d4u;
        switch (entry.stage) {
        case 1: index=1; break;
        case 2: index=2; break;
        case 4: index=3; break;
        case 5: index=4; key=0x067735d5u; break;
        default: break;
        }
        result.stage_icon_index=index;
        if (!*suppress) result.stage_caption=StarLocaleCaption{0x2b7a0311u,key};
    }
    if (entry.save_status==SaveStatus::ProbeFalseZeroHeader) {
        result.flag_changes={{0x060a1102u,1,true},{0x060a1103u,1,true},
                             {0x0375408cu,2,false},{0x05944088u,1,false}};
    }
    out=std::move(result); error.clear(); return true;
}
}  // namespace os::gge
