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
bool compose_viewer_projection(const ViewerProjectionInputs& observed,std::array<float,16>& out,std::string& error) {
    const auto fail=[&](const char* why){error=why;return false;};
    if(!observed.world_to_view||!observed.kind||!observed.view_window||!observed.view_offset||
       !observed.near_plane||!observed.far_plane)return fail("viewer projection fields unobserved");
    const auto kind=*observed.kind;
    if(kind!=ViewerProjectionKind::Perspective&&kind!=ViewerProjectionKind::Parallel&&kind!=ViewerProjectionKind::PerspectiveInfinite)
        return fail("unsupported viewer projection kind");
    const auto& view=*observed.world_to_view;
    for(float v:view)if(!std::isfinite(v))return fail("nonfinite viewer world-to-view matrix");
    const float wx=(*observed.view_window)[0],wy=(*observed.view_window)[1];
    const float ox=(*observed.view_offset)[0],oy=(*observed.view_offset)[1];
    const float near=*observed.near_plane,far=*observed.far_plane;
    // Conservative HOST input policy, not a claim about native rejection/exception behavior.
    if(!std::isfinite(wx)||!std::isfinite(wy)||wx<=0||wy<=0||!std::isfinite(ox)||!std::isfinite(oy)||
       !std::isfinite(near)||!std::isfinite(far)||near<=0||far<=near||!std::isfinite(far-near))
        return fail("invalid viewer window/offset/near/far observations");
    std::array<float,16> projection{};
    const float sx=1.f/wx,sy=1.f/wy;
    projection[0]=sx;projection[5]=sy;
    if(kind==ViewerProjectionKind::Parallel) {
        projection[10]=1.f/(far-near);
        projection[12]=sx*ox;projection[13]=sy*oy;
        projection[14]=-(projection[10]*near);projection[15]=1.f;
    }else {
        projection[11]=1.f;
        if(kind==ViewerProjectionKind::PerspectiveInfinite) {
            projection[10]=1.f;projection[14]=-near;
        }else {
            projection[8]=sx*ox;projection[9]=sy*oy;
            projection[10]=far/(far-near);projection[14]=-(projection[10]*near);
        }
    }
    for(size_t i=0;i<4;++i)std::swap(projection[4+i],projection[8+i]);
    for(float v:projection)if(!std::isfinite(v))return fail("nonfinite composed viewer projection coefficients");
    std::array<float,16> result{};
    // 45EF90/45F2C0: each row of the left matrix is multiplied by the right matrix.
    // Preserve scalar sum order; bit-identical SSE/x87/FMA rounding is not asserted.
    for(size_t row=0;row<4;++row)for(size_t col=0;col<4;++col) {
        const float v=((view[row*4]*projection[col]+view[row*4+1]*projection[4+col])+
                       view[row*4+2]*projection[8+col])+view[row*4+3]*projection[12+col];
        if(!std::isfinite(v))return fail("nonfinite viewer matrix product");
        result[row*4+col]=v;
    }
    out=result;error.clear();return true;
}
bool step_camera_fov(const CameraFovObservation& observed,CameraFovStep& out,std::string& error) {
    if(!observed.current||!observed.target||!observed.snap_threshold) {
        error="camera FOV current/target/threshold unobserved";return false;
    }
    const float current=*observed.current,target=*observed.target,threshold=*observed.snap_threshold;
    if(!std::isfinite(current)||!std::isfinite(target)||!std::isfinite(threshold)||threshold<0) {
        error="invalid camera FOV observation";return false;
    }
    CameraFovStep result;
    if(current>0)result.requested_degrees=current*57.295780181884766f; // GA 147D9EC
    if(current==-1.f&&target>0)result.next_current=target;
    else if(target==-1.f&&current>0)result.next_current=current;
    else {
        const float difference=target-current;
        if(!std::isfinite(difference)){error="camera FOV difference overflow";return false;}
        // GA snaps only when abs(diff) is STRICTLY LESS than threshold.
        result.next_current=std::abs(difference)<threshold?target:current+difference*0.10000000149011612f;
    }
    if(!std::isfinite(result.next_current)||(result.requested_degrees&&!std::isfinite(*result.requested_degrees))) {
        error="camera FOV output overflow";return false;
    }
    out=result;error.clear();return true;
}
bool compose_camera_orbit(const CameraOrbitObservation& observed,CameraPose& out,std::string& error) {
    if(!observed.basis||!observed.pivot||!observed.additional_offset||!observed.first_angle||
       !observed.second_angle||!observed.distance) {
        error="camera orbit basis/pivot/offset/angles/distance unobserved";return false;
    }
    for(float v:*observed.basis)if(!std::isfinite(v)){error="nonfinite camera orbit basis";return false;}
    for(float v:{observed.pivot->x,observed.pivot->y,observed.pivot->z,
                 observed.additional_offset->x,observed.additional_offset->y,observed.additional_offset->z,
                 *observed.first_angle,*observed.second_angle,*observed.distance})
        if(!std::isfinite(v)){error="nonfinite camera orbit input";return false;}
    CameraPose result;result.rotation=*observed.basis;
    // GA 4099B0(_CIsin/_CIcos) rotates rows0/1, then5A2D70 rotates rows1/2.
    const float s1=std::sin(-*observed.first_angle),c1=std::cos(-*observed.first_angle);
    const float s2=std::sin(*observed.second_angle),c2=std::cos(*observed.second_angle);
    for(size_t col=0;col<3;++col) {
        const float a=(*observed.basis)[col],b=(*observed.basis)[3+col],c=(*observed.basis)[6+col];
        const float rotated_b=(-s1)*a+c1*b;
        result.rotation[col]=c1*a+s1*b;
        result.rotation[3+col]=c2*rotated_b+s2*c;
        result.rotation[6+col]=(-s2)*rotated_b+c2*c;
    }
    result.position={observed.pivot->x+(-*observed.distance)*result.rotation[3]+observed.additional_offset->x,
                     observed.pivot->y+(-*observed.distance)*result.rotation[4]+observed.additional_offset->y,
                     observed.pivot->z+(-*observed.distance)*result.rotation[5]+observed.additional_offset->z};
    for(float v:result.rotation)if(!std::isfinite(v)){error="camera orbit rotation overflow";return false;}
    for(float v:{result.position.x,result.position.y,result.position.z})if(!std::isfinite(v)) {
        error="camera orbit position overflow";return false;
    }
    out=result;error.clear();return true;
}
bool camera_pose_world_to_view(const CameraPose& pose,std::array<float,16>& out,std::string& error) {
    for(float v:pose.rotation)if(!std::isfinite(v)){error="nonfinite camera pose rotation";return false;}
    for(float v:{pose.position.x,pose.position.y,pose.position.z})if(!std::isfinite(v)) {
        error="nonfinite camera pose position";return false;
    }
    for(size_t a=0;a<3;++a)for(size_t b=a;b<3;++b) {
        float dot=0;
        for(size_t i=0;i<3;++i)dot+=pose.rotation[3*a+i]*pose.rotation[3*b+i];
        if(!std::isfinite(dot)||std::abs(dot-(a==b?1.f:0.f))>0.0001f) {
            error="camera pose rigid inverse requires orthonormal scale1 rotation";return false;
        }
    }
    std::array<float,16> result{};
    for(size_t a=0;a<3;++a)for(size_t b=0;b<3;++b)result[4*a+b]=pose.rotation[3*b+a];
    for(size_t i=0;i<3;++i) {
        result[12+i]=-((pose.position.x*pose.rotation[3*i]+pose.position.y*pose.rotation[3*i+1])+
                       pose.position.z*pose.rotation[3*i+2]);
        if(!std::isfinite(result[12+i])){error="camera pose inverse translation overflow";return false;}
    }
    result[15]=1.f;out=result;error.clear();return true;
}
bool project_star(const StarPosition& world,const CameraObservation& observed,ProjectedPoint& out,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (!observed.clip_from_world || !observed.window_rect) return fail("star projection viewer-matrix/window observations unavailable");
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
