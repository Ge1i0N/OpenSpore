#include "engine/gge_star_present.h"
#include <filesystem>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace os;
using namespace os::gge;
static bool corpus(const std::filesystem::path& dir,size_t& matches) {
    std::ifstream manifest(dir/"manifest.txt"); std::string file;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>file>>bytes>>objects>>windows>>props>>links>>roots) {
        if (file.find("21df01f2")==std::string::npos) continue;
        std::ifstream input(dir/file,std::ios::binary); std::vector<u8> data(std::istreambuf_iterator<char>(input),{});
        spui::Layout layout; std::string error;
        if (!spui::parse_binary(data,layout,error)||data.size()!=24812||objects!=152||windows!=58) return false;
        size_t positioning_roots=0;
        for (const auto& win:layout.windows) if (win.control_id==kStarUiRoot) {
            ++positioning_roots;
            if (win.object_index!=177||win.class_id!=0x4ec1b8d8u||win.parent!=-1||!win.has_area||
                win.area.left!=76||win.area.top!=167||win.area.right!=124||win.area.bottom!=215||
                win.child_objects!=std::vector<u16>{28,41,90,176}) return false;
        }
        if (positioning_roots!=1||layout.roots.size()!=1||
            layout.windows[layout.roots[0]].object_index!=177) return false;
        bool serialized_root=false;
        for (const auto& obj:layout.objects) if (obj.index==177) serialized_root=obj.root;
        if (!serialized_root) return false;
        for (u32 id:{kStarUiRoot,0x026e0001u,0x026e0002u,0x03c4925eu,0x060a1102u,0x060a1103u,0x0375408cu,0x05944088u}) {
            bool found=false; for (const auto& win:layout.windows) if (win.control_id==id) found=true;
            if (!found) { std::cerr<<"missing real star UI control 0x"<<std::hex<<id<<std::dec<<'\n'; return false; }
        }
        ++matches;
    }
    return matches!=0;
}
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr<<"usage: gge_star_present_verify <SPUI-fixture-dir>\n"; return 2; }
    size_t checks=0,matches=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    if (!check(fnv1_lower("GGEGameStarUIv4")==kStarUiLayout && corpus(argv[1],matches),"real star layout/control binding")) return 1;
    CameraObservation camera;
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    ProjectedPoint point; point.x=777; std::string error;
    if (!check(!project_star({},camera,point,error)&&point.x==777&&!error.empty(),"unknown camera unchanged")) return 1;
    camera.clip_from_world=identity;
    if (!check(!project_star({},camera,point,error),"missing window observation")) return 1;
    camera.window_rect=spui::Rect{100,200,900,800}; // extents 800x600; original adds no origin.
    if (!check(project_star({},camera,point,error)&&point.x==400&&point.y==300&&point.clip_w==1,"center from extents, not rectangle origin")) return 1;
    for (const auto& c:std::vector<std::array<float,4>>{{-1,1,0,0},{1,-1,800,600},{2,2,1200,-300}})
        if (!check(project_star({c[0],c[1],0},camera,point,error)&&point.x==c[2]&&point.y==c[3],"axes/outside remains unclipped")) return 1;
    auto m=identity; m[15]=2; camera.clip_from_world=m;
    if (!check(project_star({1,1,4},camera,point,error)&&point.x==600&&point.y==150&&point.ndc_z==2&&point.clip_w==2,"perspective divide not affine-only")) return 1;
    m[15]=-2; camera.clip_from_world=m;
    if (!check(project_star({1,1,4},camera,point,error)&&point.x==200&&point.y==450&&point.clip_w==-2,"negative w not invented culling")) return 1;
    // Asymmetric matrix catches transpose/order mistakes; choose w=1 and exact arithmetic.
    camera.clip_from_world=std::array<float,16>{1,2,3,0,4,5,6,0,7,8,9,0,10,11,12,1};
    if (!check(project_star({1,2,3},camera,point,error)&&point.x==16400&&point.y==-13800&&point.ndc_z==54,
               "matrix memory order includes translation and z contributions")) return 1;
    const auto rejected=[&](const CameraObservation& c,const StarPosition& world) {
        ProjectedPoint sentinel; sentinel.x=123; return !project_star(world,c,sentinel,error)&&sentinel.x==123&&!error.empty();
    };
    camera.clip_from_world=identity;
    ViewerProjectionInputs viewer;
    std::array<float,16> composed=identity;
    const auto rejected_viewer=[&](const ViewerProjectionInputs& input) {
        auto sentinel=identity;
        return !compose_viewer_projection(input,sentinel,error)&&sentinel==identity&&!error.empty();
    };
    if(!check(rejected_viewer(viewer),"missing viewer fields never fabricate an identity camera"))return 1;
    viewer.world_to_view=identity;viewer.kind=ViewerProjectionKind::Perspective;
    viewer.view_window=std::array<float,2>{2,4};viewer.view_offset=std::array<float,2>{.5f,-1};
    viewer.near_plane=2.f;viewer.far_plane=10.f;
    const std::array<float,16> perspective{.5f,0,0,0,.25f,-.25f,1.25f,1,0,.25f,0,0,0,0,-2.5f,0};
    const std::array<float,16> parallel{.5f,0,0,0,0,0,.125f,0,0,.25f,0,0,.25f,-.25f,-.25f,1};
    const std::array<float,16> infinite{.5f,0,0,0,0,0,1,1,0,.25f,0,0,0,0,-2,0};
    if(!check(compose_viewer_projection(viewer,composed,error)&&composed==perspective,
        "literal perspective coefficients + viewer axis-row exchange"))return 1;
    CameraObservation built;built.clip_from_world=composed;built.window_rect=spui::Rect{100,200,900,800};
    ProjectedPoint projected;
    if(!check(project_star({2,4,8},built,projected,error)&&projected.x==600&&projected.y==225&&
        projected.clip_w==4&&projected.ndc_z==.625f,"viewer composition feeds actual GGE perspective projector"))return 1;
    viewer.kind=ViewerProjectionKind::Parallel;
    if(!check(compose_viewer_projection(viewer,composed,error)&&composed==parallel,
        "parallel projection translates offsets and uses explicit near/far"))return 1;
    built.clip_from_world=composed;
    if(!check(project_star({2,4,8},built,projected,error)&&projected.x==900&&projected.y==-225&&
        projected.clip_w==1&&projected.ndc_z==.25f,"parallel composition preserves original unclipped mapping"))return 1;
    viewer.kind=ViewerProjectionKind::PerspectiveInfinite;
    if(!check(compose_viewer_projection(viewer,composed,error)&&composed==infinite,
        "infinite branch ignores offset coefficients, not generalized perspective"))return 1;
    built.clip_from_world=composed;
    if(!check(project_star({2,4,8},built,projected,error)&&projected.x==500&&projected.y==150&&
        projected.clip_w==4&&projected.ndc_z==.5f,"infinite projection branch end-to-end"))return 1;
    viewer.kind=ViewerProjectionKind::Perspective;
    (*viewer.world_to_view)[12]=3;(*viewer.world_to_view)[13]=5;(*viewer.world_to_view)[14]=7;
    auto translated=perspective;translated[12]=2.75f;translated[13]=.5f;translated[14]=3.75f;translated[15]=5;
    if(!check(compose_viewer_projection(viewer,composed,error)&&composed==translated,
        "world-to-view LEFT product retains noncommuting translation; not P*V"))return 1;
    // Independent explicit two-stage reference for a general asymmetric affine input.
    viewer.world_to_view=std::array<float,16>{1,2,3,0,4,5,6,0,7,8,10,0,11,12,13,1};
    if(!check(compose_viewer_projection(viewer,composed,error),"general asymmetric viewer matrix product"))return 1;
    built.clip_from_world=composed;
    CameraObservation projection_only=built;projection_only.clip_from_world=perspective;
    ProjectedPoint staged;
    if(!check(project_star({2,3,4},built,projected,error)&&
        project_star({53,63,77},projection_only,staged,error)&&
        std::abs(projected.x-staged.x)<.001f&&std::abs(projected.y-staged.y)<.001f&&
        projected.clip_w==staged.clip_w&&std::abs(projected.ndc_z-staged.ndc_z)<.00001f,
        "asymmetric composition agrees with explicit world-to-view then projection"))return 1;
    viewer.world_to_view=identity;
    for(int field=0;field<6;++field) {
        auto missing_viewer=viewer;
        switch(field){case 0:missing_viewer.world_to_view.reset();break;case 1:missing_viewer.kind.reset();break;
        case 2:missing_viewer.view_window.reset();break;case 3:missing_viewer.view_offset.reset();break;
        case 4:missing_viewer.near_plane.reset();break;case 5:missing_viewer.far_plane.reset();break;}
        if(!check(rejected_viewer(missing_viewer),"each unobserved viewer field blocks atomic composition"))return 1;
    }
    for(float value:{0.f,-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        auto invalid=viewer;(*invalid.view_window)[0]=value;
        if(!check(rejected_viewer(invalid),"invalid view-window coefficient"))return 1;
    }
    auto invalid_viewer=viewer;invalid_viewer.far_plane=2.f;
    if(!check(rejected_viewer(invalid_viewer),"equal near/far rejected by host policy"))return 1;
    invalid_viewer=viewer;invalid_viewer.near_plane=0.f;
    if(!check(rejected_viewer(invalid_viewer),"nonpositive near rejected by host policy"))return 1;
    invalid_viewer=viewer;invalid_viewer.kind=static_cast<ViewerProjectionKind>(99);
    if(!check(rejected_viewer(invalid_viewer),"unsupported enum not treated as perspective"))return 1;
    invalid_viewer=viewer;(*invalid_viewer.view_offset)[1]=std::numeric_limits<float>::infinity();
    if(!check(rejected_viewer(invalid_viewer),"nonfinite offset"))return 1;
    invalid_viewer=viewer;(*invalid_viewer.world_to_view)[7]=std::numeric_limits<float>::quiet_NaN();
    if(!check(rejected_viewer(invalid_viewer),"nonfinite matrix"))return 1;
    invalid_viewer=viewer;(*invalid_viewer.view_window)[0]=std::numeric_limits<float>::denorm_min();
    if(!check(rejected_viewer(invalid_viewer),"finite input with overflowing reciprocal rejected"))return 1;
    invalid_viewer=viewer;(*invalid_viewer.world_to_view)[0]=std::numeric_limits<float>::max();
    (*invalid_viewer.view_window)[0]=.25f;
    if(!check(rejected_viewer(invalid_viewer),"overflowing product rejected without publication"))return 1;
    CameraOrbitObservation orbit;
    CameraPose pose;pose.position.x=123.f;
    if(!check(!compose_camera_orbit(orbit,pose,error)&&pose.position.x==123.f,
        "missing orbit fields never synthesize initial pose"))return 1;
    orbit.basis=std::array<float,9>{1,0,0,0,1,0,0,0,1};orbit.pivot=StarPosition{2,3,4};
    orbit.additional_offset=StarPosition{5,6,7};orbit.first_angle=0.f;orbit.second_angle=0.f;orbit.distance=10.f;
    if(!check(compose_camera_orbit(orbit,pose,error)&&pose.rotation==*orbit.basis&&
        pose.position.x==7&&pose.position.y==-1&&pose.position.z==11,
        "zero angles preserve explicit basis; negative row1 orbit and separate additional offset"))return 1;
    std::array<float,16> inverse_pose;
    if(!check(camera_pose_world_to_view(pose,inverse_pose,error)&&inverse_pose[12]==-7&&inverse_pose[13]==1&&inverse_pose[14]==-11,
        "explicit pose to viewer world-to-view rigid inverse"))return 1;
    viewer.world_to_view=inverse_pose;viewer.kind=ViewerProjectionKind::Perspective;
    if(!check(compose_viewer_projection(viewer,composed,error),"pose bridges into recovered viewer composition"))return 1;
    built.clip_from_world=composed;
    if(!check(project_star({7,3,11},built,projected,error)&&projected.x==500&&projected.y==375&&projected.clip_w==4,
        "orbit pose -> rigid inverse -> viewer projection -> original pixel mapping"))return 1;
    const float quarter_turn=1.5707963267948966f;
    orbit.pivot=StarPosition{};orbit.additional_offset=StarPosition{};
    orbit.first_angle=quarter_turn;orbit.second_angle=0.f;
    if(!check(compose_camera_orbit(orbit,pose,error)&&std::abs(pose.rotation[1]+1)<.00001f&&
        std::abs(pose.rotation[3]-1)<.00001f&&std::abs(pose.position.x+10)<.00001f,
        "first rotation sign and row pair, not generic Euler guess"))return 1;
    orbit.first_angle=0.f;orbit.second_angle=quarter_turn;
    if(!check(compose_camera_orbit(orbit,pose,error)&&std::abs(pose.rotation[5]-1)<.00001f&&
        std::abs(pose.rotation[7]+1)<.00001f&&std::abs(pose.position.z+10)<.00001f,
        "second rotation acts on rows1/2 and positions along resulting row1"))return 1;
    orbit.first_angle=.3f;orbit.second_angle=-.7f;orbit.distance=0.f;
    if(!check(compose_camera_orbit(orbit,pose,error)&&camera_pose_world_to_view(pose,inverse_pose,error),
        "nontrivial finite angles yield host-admissible rigid pose"))return 1;
    for(size_t row=0;row<3;++row)for(size_t col=0;col<3;++col) {
        float v=0;for(size_t k=0;k<3;++k)v+=pose.rotation[row*3+k]*inverse_pose[k*4+col];
        if(!check(std::abs(v-(row==col?1.f:0.f))<.00001f,"asymmetric rotation times inverse is identity"))return 1;
    }
    const auto reject_orbit=[&](const CameraOrbitObservation& input) {
        CameraPose sentinel;sentinel.position.x=123;
        return !compose_camera_orbit(input,sentinel,error)&&sentinel.position.x==123;
    };
    for(int field=0;field<6;++field) {
        auto missing_orbit=orbit;
        switch(field){case 0:missing_orbit.basis.reset();break;case 1:missing_orbit.pivot.reset();break;
        case 2:missing_orbit.additional_offset.reset();break;case 3:missing_orbit.first_angle.reset();break;
        case 4:missing_orbit.second_angle.reset();break;case 5:missing_orbit.distance.reset();break;}
        if(!check(reject_orbit(missing_orbit),"each missing orbit field blocks, never defaults"))return 1;
    }
    auto missing_orbit=orbit;
    missing_orbit=orbit;(*missing_orbit.basis)[0]=std::numeric_limits<float>::infinity();
    if(!check(reject_orbit(missing_orbit),"nonfinite orbit basis fails atomically"))return 1;
    missing_orbit=orbit;missing_orbit.second_angle=std::numeric_limits<float>::quiet_NaN();
    if(!check(reject_orbit(missing_orbit),"nonfinite orbit angle fails atomically"))return 1;
    missing_orbit=orbit;missing_orbit.first_angle=0.f;missing_orbit.second_angle=0.f;
    missing_orbit.distance=std::numeric_limits<float>::max();(*missing_orbit.basis)[4]=2.f;
    if(!check(reject_orbit(missing_orbit),"finite orbit input with position overflow is rejected"))return 1;
    auto invalid_pose=pose;invalid_pose.rotation[0]*=2.f;auto sentinel_inverse=identity;
    if(!check(!camera_pose_world_to_view(invalid_pose,sentinel_inverse,error)&&sentinel_inverse==identity,
        "non-rigid pose is not silently inverted by rigid helper"))return 1;
    invalid_pose=pose;invalid_pose.position.y=std::numeric_limits<float>::quiet_NaN();
    if(!check(!camera_pose_world_to_view(invalid_pose,sentinel_inverse,error)&&sentinel_inverse==identity,
        "invalid pose inverse output remains unchanged"))return 1;
    // A non-identity source basis makes rotation order/sign mistakes visible without relying on
    // the runtime initializer or assuming the live camera basis is immutable.
    orbit.basis=std::array<float,9>{0,1,0,-1,0,0,0,0,1};orbit.first_angle=quarter_turn;
    orbit.second_angle=0.f;orbit.distance=-2.f;orbit.pivot=StarPosition{1,2,3};orbit.additional_offset=StarPosition{4,5,6};
    if(!check(compose_camera_orbit(orbit,pose,error)&&std::abs(pose.rotation[0]-1)<.00001f&&
        std::abs(pose.position.y-9)<.00001f&&std::abs(pose.position.x-5)<.00001f&&pose.position.z==9,
        "observed nonidentity basis and negative displacement retained; no synthetic clamp"))return 1;
    orbit.first_angle=.3f;orbit.second_angle=-.7f;orbit.distance=8.f;
    if(!check(compose_camera_orbit(orbit,pose,error)&&camera_pose_world_to_view(pose,inverse_pose,error),
        "translated rotated orbit pose admits rigid inverse"))return 1;
    for(size_t col=0;col<3;++col) {
        const float at_origin=pose.position.x*inverse_pose[col]+pose.position.y*inverse_pose[4+col]+
                              pose.position.z*inverse_pose[8+col]+inverse_pose[12+col];
        if(!check(std::abs(at_origin)<.00001f,"camera world position maps to local origin"))return 1;
    }
    CameraDynamicsAxisStep dynamics;
    if(!check(step_camera_dynamics_axis({1.f,3.f,2.f,0.f,10.f,1.f,.1f,2.f},dynamics,error)&&
        std::abs(dynamics.velocity-4.2f)<.00001f&&std::abs(dynamics.current-5.2f)<.00001f,
        "DDE580 damping/stiffness/current order"))return 1;
    if(!check(step_camera_dynamics_axis({1.f,3.f,2.f,0.f,5.f,1.f,.1f,2.f},dynamics,error)&&
        dynamics.current==5.f&&std::abs(dynamics.velocity-4.2f)<.00001f,"DDE580 max clamp after velocity"))return 1;
    if(!check(step_camera_dynamics_axis({5.f,1.f,-2.f,0.f,5.f,1.f,.1f,2.f},dynamics,error)&&
        dynamics.current==0.f&&std::abs(dynamics.velocity+8.2f)<.00001f,"DDE580 min clamp and negative direction"))return 1;
    if(!check(step_camera_dynamics_axis({2.f,2.f,9.f,0.f,10.f,0.f,.1f,2.f},dynamics,error)&&
        dynamics.current==2.f&&dynamics.velocity==9.f,"zero delta leaves velocity unchanged, no movement"))return 1;
    if(!check(step_camera_dynamics_axis({2.f,4.f,0.f,0.f,10.f,.5f,.1f,2.f},dynamics,error)&&
        std::abs(dynamics.velocity-2.f)<.00001f,"fractional delta and pow damping"))return 1;
    const auto reject_dynamics=[&](const CameraDynamicsAxisObservation& input) {
        CameraDynamicsAxisStep sentinel{123.f,456.f};
        return !step_camera_dynamics_axis(input,sentinel,error)&&sentinel.current==123.f&&sentinel.velocity==456.f;
    };
    if(!check(reject_dynamics({})&&reject_dynamics({2.f,3.f,0.f,0.f,1.f,-1.f,.1f,2.f})&&
        reject_dynamics({2.f,3.f,0.f,2.f,1.f,1.f,.1f,2.f}),"missing/invalid dynamics fields atomic"))return 1;
    if(!check(reject_dynamics({2.f,3.f,0.f,0.f,1.f,1.f,0.f,2.f})&&
        reject_dynamics({2.f,3.f,0.f,0.f,1.f,1.f,.1f,std::numeric_limits<float>::quiet_NaN()}),
        "invalid damping/stiffness atomic"))return 1;
    auto bad=camera; (*bad.clip_from_world)[15]=0;
    CameraFovStep fov;
    if(!check(step_camera_fov({1.f,2.f,.01f},fov,error)&&fov.next_current==1.1f&&
        fov.requested_degrees==57.295780181884766f,"FOV viewer request uses OLD current then fixed .1 update"))return 1;
    if(!check(step_camera_fov({-1.f,1.f,.01f},fov,error)&&!fov.requested_degrees&&fov.next_current==1.f,
        "unset current adopts positive target but no viewer request this frame"))return 1;
    if(!check(step_camera_fov({1.f,-1.f,.01f},fov,error)&&fov.requested_degrees&&fov.next_current==1.f,
        "unset target holds positive current"))return 1;
    if(!check(step_camera_fov({1.f,1.25f,.5f},fov,error)&&fov.next_current==1.25f,
        "below threshold snaps"))return 1;
    if(!check(step_camera_fov({1.f,1.5f,.5f},fov,error)&&fov.next_current==1.05f,
        "exact threshold smooths, not snaps"))return 1;
    if(!check(step_camera_fov({0.f,1.f,0.f},fov,error)&&!fov.requested_degrees&&fov.next_current==.1f,
        "zero is observed, not an unset or Unknown substitute"))return 1;
    const auto reject_fov=[&](const CameraFovObservation& input) {
        CameraFovStep sentinel;sentinel.next_current=123.f;sentinel.requested_degrees=456.f;
        return !step_camera_fov(input,sentinel,error)&&sentinel.next_current==123.f&&sentinel.requested_degrees==456.f;
    };
    if(!check(reject_fov({})&&reject_fov({1.f,2.f,std::nullopt})&&reject_fov({1.f,2.f,-1.f}),
        "unobserved/negative-threshold FOV fails atomically"))return 1;
    if(!check(reject_fov({std::numeric_limits<float>::quiet_NaN(),1.f,.1f})&&
        reject_fov({std::numeric_limits<float>::max(),1.f,.1f}),"nonfinite/overflow FOV request fails atomically"))return 1;
    if (!check(rejected(bad,{}),"zero w")) return 1;
    bad=camera; (*bad.clip_from_world)[0]=std::numeric_limits<float>::infinity();
    if (!check(rejected(bad,{}),"nonfinite matrix")) return 1;
    bad=camera; bad.window_rect=spui::Rect{};
    if (!check(rejected(bad,{}),"empty window rejected")) return 1;
    if (!check(rejected(camera,{std::numeric_limits<float>::quiet_NaN(),0,0}),"NaN world input")) return 1;
    bad=camera; (*bad.clip_from_world)[0]=std::numeric_limits<float>::max();
    if (!check(rejected(bad,{2,0,0}),"nonfinite projected result")) return 1;
    PlacementObservation placement;
    placement.root_area=spui::Rect{-10,-20,90,30}; placement.window_rect=*camera.window_rect;
    placement.current_position=std::array<float,2>{350,275}; placement.saved_insets=SavedInsets{10,20,80,90};
    StarUiPlacement placed;
    if (!check(place_star_ui({400,300,0,1},true,placement,placed,error)&&placed.x==350&&placed.y==275&&!placed.move_required,"root centering no redundant move")) return 1;
    for (float delta:{-5.f,-4.f,0.f,4.f,5.f}) {
        if (!check(place_star_ui({400+delta,300,0,1},false,placement,placed,error)&&
            placed.move_required==(delta==5||delta==-5),"strict >4 movement threshold")) return 1;
    }
    if (!check(place_star_ui({-100,900,0,1},true,placement,placed,error)&&placed.x==10&&placed.y==510,"saved clamp extents/insets")) return 1;
    if (!check(place_star_ui({-100,900,0,1},false,placement,placed,error)&&placed.x==-150&&placed.y==875,"new entries not clamped as saved")) return 1;
    const auto reject_placement=[&](const PlacementObservation& o) {
        StarUiPlacement sentinel; sentinel.x=999;
        return !place_star_ui({400,300,0,1},true,o,sentinel,error)&&sentinel.x==999;
    };
    auto missing=placement; missing.saved_insets.reset();
    if (!check(reject_placement(missing),"unknown saved margins not guessed")) return 1;
    missing=placement; missing.current_position.reset();
    if (!check(reject_placement(missing),"unknown current root position")) return 1;
    missing=placement; missing.saved_insets->left=999;
    if (!check(reject_placement(missing),"inverted clamp bounds")) return 1;
    GameEntry entry; StarUiConfiguration config;
    if (!check(configure_star_ui(entry,std::nullopt,config,error)&&!config.stage_icon_index&&!config.stage_caption&&config.flag_changes.empty(),"new entry does not need suppression observation")) return 1;
    entry.saved=true; config.stage_icon_index=999;
    if (!check(!configure_star_ui(entry,std::nullopt,config,error)&&config.stage_icon_index==999u,"saved unknown global flag atomic")) return 1;
    const std::array<u32,8> icon{0,1,2,0,3,4,0,0};
    for (u32 stage=0;stage<8;++stage) {
        entry.stage=stage;
        for (bool suppress:{false,true}) {
            if (!check(configure_star_ui(entry,suppress,config,error)&&config.stage_icon_index==icon[stage]&&
                (suppress?!config.stage_caption:(config.stage_caption && config.stage_caption->table==0x2b7a0311u &&
                 config.stage_caption->key==(stage==5?0x067735d5u:0x067735d4u))),"saved stage icon and locale branches")) return 1;
        }
    }
    for (auto status:{SaveStatus::HeaderMatches,SaveStatus::HeaderDiffers,SaveStatus::ProbeFalseNonzeroHeader,SaveStatus::ProbeFalseZeroHeader,SaveStatus::NotSaved}) {
        entry.save_status=status;
        if (!check(configure_star_ui(entry,false,config,error)&&config.flag_changes.size()==(status==SaveStatus::ProbeFalseZeroHeader?4u:0u),"only original status3 requests flag changes")) return 1;
    }
    entry.save_status=SaveStatus::ProbeFalseZeroHeader; configure_star_ui(entry,false,config,error);
    if (!check(config.flag_changes[0].control==0x060a1102u && config.flag_changes[0].mask==1 && config.flag_changes[0].value &&
        config.flag_changes[1].control==0x060a1103u && config.flag_changes[2].control==0x0375408cu &&
        config.flag_changes[2].mask==2 && !config.flag_changes[2].value && config.flag_changes[3].control==0x05944088u &&
        config.flag_changes[3].mask==1 && !config.flag_changes[3].value,"exact raw control/mask/value requests")) return 1;
    std::cout<<checks<<" GGE projection/placement/star-configuration checks PASS; "<<matches
        <<" real 152-object/58-window layouts bound; controlled camera only, not live scene\n";
    return 0;
}
