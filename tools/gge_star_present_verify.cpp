#include "engine/gge_star_present.h"
#include <filesystem>
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
    auto bad=camera; (*bad.clip_from_world)[15]=0;
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
