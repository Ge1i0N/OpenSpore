// World-ray picker vs UI-window picker. Explicit rays only; no native callback/phase dispatch.
#include "engine/gge_star_scene.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace os; using namespace os::gge;
static SceneCameraObservation camera() { SceneCameraObservation c; c.source=SceneCameraSource::ActiveProjection; c.token=1; c.projection.clip_from_world=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1}; c.projection.window_rect=spui::Rect{0,0,1024,768}; return c; }
int main(int argc,char** argv) {
    if(argc!=2){std::cerr<<"usage: gge_star_input_verify <SPUI-fixtures>\n";return 2;}
    size_t checks=0,fixtures=0; std::string error; const auto check=[&](bool ok,const char* why) { ++checks; if(!ok) std::cerr<<why<<'\n'; return ok; };
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name; size_t bytes,objects,windows,props,links,roots;
    while(manifest>>name>>bytes>>objects>>windows>>props>>links>>roots){
        if(name.find("21df01f2")==std::string::npos)continue; std::ifstream f(std::filesystem::path(argv[1])/name,std::ios::binary); std::vector<u8> data(std::istreambuf_iterator<char>(f),{}); spui::Layout source;
        if(!check(spui::parse_binary(data,source,error),"actual layout"))return 1; source.instance=kStarUiLayout;
        CatalogInput catalog;catalog.saved_status=catalog.candidate_status=CatalogAvailability::Available; StarMetadata a,b; a.star_id=1;a.position={0,0,0};a.name=u"a";b.star_id=2;b.position={3,0,0};b.name=u"b";catalog.candidates={a,b};
        StarSceneCardInput ia,ib;ia.host_id=11;ib.host_id=22;ia.lifecycle.phase1.thumbnail=ImageKey{};ib.lifecycle.phase1.thumbnail=ImageKey{};ia.lifecycle.phase1.has_profile=false;ib.lifecycle.phase1.has_profile=false;ia.lifecycle.phase1.selected_star_saved=false;ib.lifecycle.phase1.selected_star_saved=false;ia.lifecycle.phase1.selected_star_present=false;ib.lifecycle.phase1.selected_star_present=false;
        StarUiScene scene; if(!check(scene.open(catalog,source,{ia,ib},error)&&scene.update(scene.ticket(),camera(),error),"scene"))return 1; const auto ticket=scene.ticket();
        StarWorldRayObservation ray;ray.source=SceneCameraSource::ActiveProjection;ray.token=1;ray.origin={-1,0,0};ray.direction={1,0,0};ray.max_distance=15;
        auto hit=scene.world_pick(ticket,ray); if(!check(hit&&hit->entry_index==0&&hit->host_id==11&&hit->distance==0,"nearest point-segment world hit"))return 1;
        ray.origin={2.8f,1,0};ray.direction={0,-1,0};hit=scene.world_pick(ticket,ray);if(!check(hit&&hit->entry_index==1&&hit->host_id==22&&hit->distance<.21f,"ray selects second entry by distance, not UI ControlID"))return 1;
        ray.max_distance=.1f;if(!check(!scene.world_pick(ticket,ray),"threshold miss"))return 1;
        ray.max_distance=15;ray.source=SceneCameraSource::ControllerRetained;if(!check(!scene.world_pick(ticket,ray),"controller camera cannot substitute active projection ray"))return 1;
        ray.source=SceneCameraSource::ActiveProjection;ray.token=0;if(!check(!scene.world_pick(ticket,ray),"missing ray observation token"))return 1;
        ray.token=1;ray.direction={0,0,0};if(!check(!scene.world_pick(ticket,ray),"zero direction rejected"))return 1;
        ray.direction={1,0,0};ray.origin={-1,0,0};ray.max_distance=std::numeric_limits<float>::quiet_NaN();if(!check(!scene.world_pick(ticket,ray),"nonfinite threshold rejected"))return 1;
        ray.max_distance=15;ray.origin={-1,0,0};ray.direction={-1,0,0};if(!check(scene.world_pick(ticket,ray).has_value(),"point-segment behind-ray behavior"))return 1;
        if(!check(!scene.world_pick(ticket+1,ray),"stale scene ticket"))return 1;
        StarTransitionInputs t;t.requested_case=2;t.observed_current_case=1;t.choices={{0x046a9ca7u,90}};
        const bool transitioned=scene.apply_phase_transition(ticket,0,t,error);
        if(!check(transitioned&&scene.frame().serial==0&&scene.layers(1).empty(),"world picker independent from transition; frame retires"))return 1;
        if(!check(scene.apply_phase_transition(ticket,1,t,error)&&scene.world_pick(ticket,ray).has_value(),"transition does not dispatch world result"))return 1;
        if(!check(scene.close(error)&&!scene.world_pick(ticket,ray),"closed scene"))return 1; ++fixtures;
    }
    if(!check(fixtures==2,"both layouts"))return 1; std::cout<<checks<<" world-input ray/threshold/camera/ticket isolation checks PASS; no F8/FC/CommandID dispatch\n";return 0;
}
