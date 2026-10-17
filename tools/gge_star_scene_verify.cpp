// Controlled Available metadata/camera on REAL star layouts. Not a native galaxy/runtime corpus.
#include "engine/gge_star_scene.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace os;
using namespace os::gge;
using CardStatus=StarSceneCardStatus;
static spui::Rect root_area(const StarUiLifecycle& c) {
    for (const auto& w:c.instance()->layout().windows) if (w.object_index==c.instance()->root_object()) return w.area;
    return {};
}
static SceneCameraObservation camera() {
    SceneCameraObservation c; c.source=SceneCameraSource::ActiveProjection; c.token=7;
    c.projection.clip_from_world=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    c.projection.window_rect=spui::Rect{77,88,1101,856}; return c; // 1024x768 EXTENTS, not an origin offset
}
static std::optional<std::array<float,2>> hittable(const StarUiInstance& instance,const spui::Rect& viewport) {
    for (const auto& w:instance.layout().windows) {
        const float x=w.placed_area.left+w.placed_area.w()*.5f,y=w.placed_area.top+w.placed_area.h()*.5f;
        if (instance.pick(x,y,viewport)) return std::array<float,2>{x,y};
    }
    return std::nullopt;
}
struct Reentry final:ImageProvider {
    StarUiScene* scene=nullptr; const CatalogInput* catalog=nullptr; const spui::Layout* source=nullptr;
    const std::vector<StarSceneCardInput>* cards=nullptr; StreamingImageProvider* backend=nullptr;
    bool denied=true; unsigned calls=0;
    ImageRead read(const ImageKey& key) override {
        ++calls; std::string error; const auto t=scene->ticket();
        denied=denied&&!scene->close(error)&&!scene->open(*catalog,*source,*cards,error)&&
            !scene->apply_available(t,this,error)&&!scene->update(t,camera(),error)&&
            !scene->apply_phase_transition(t,0,StarTransitionInputs{2u,1u,{}},error)&&scene->layers(scene->frame().serial).empty();
        return backend->read(key);
    }
};
int main(int argc,char** argv) {
    if (argc!=3) { std::cerr<<"usage: gge_star_scene_verify <SPUI-fixtures> <explicit-original-Graphics.package>\n"; return 2; }
    size_t checks=0,fixtures=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    StreamingImageProvider images;
    if (!check(images.open(argv[2],error),"direct original streaming provider")) return 1;
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>name>>bytes>>objects>>windows>>props>>links>>roots) {
        if (name.find("21df01f2")==std::string::npos) continue;
        std::ifstream f(std::filesystem::path(argv[1])/name,std::ios::binary); std::vector<u8> data(std::istreambuf_iterator<char>(f),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error),"actual star layout")) return 1;
        source.instance=kStarUiLayout;
        const auto pristine=source;
        StarUiScene scene; CatalogInput unknown;
        if (!check(!scene.open(unknown,source,{},error)&&!scene.ticket()&&!scene.update(1,camera(),error),"unknown catalogue is not observed empty/ready")) return 1;
        CatalogInput catalog; catalog.saved_status=catalog.candidate_status=CatalogAvailability::Available;
        SavedGameMetadata saved; saved.star.star_id=42; saved.star.name=u"Controlled saved"; saved.stage=4;
        catalog.saved={saved};
        StarMetadata candidate; candidate.star_id=42; candidate.name=u"Controlled candidate"; candidate.position.z=1;
        catalog.candidates={candidate,candidate}; catalog.candidates[1].position.z=2;
        auto caller_catalog=catalog; caller_catalog.source_label="caller-catalog.txt";
        if (!check(!scene.open(caller_catalog,source,{},error),"caller-labelled catalogue with unknown producer observations is blocked")) return 1;
        caller_catalog.producer.manager=caller_catalog.producer.starsdb=caller_catalog.producer.planet_records=caller_catalog.producer.candidate_refill=CatalogProducerStatus::Available;
        std::vector<StarSceneCardInput> inputs(3);
        for (size_t i=0;i<inputs.size();++i) {
            inputs[i].host_id=100+i; inputs[i].saved_insets=SavedInsets{0,0,0,0};
            inputs[i].lifecycle.phase1.thumbnail=ImageKey{}; inputs[i].lifecycle.phase1.has_profile=false;
            inputs[i].lifecycle.phase1.selected_star_saved=false; inputs[i].lifecycle.phase1.selected_star_present=false;
            inputs[i].lifecycle.phase1.choices={{0x046a9ca7,90},{0x026e0002,55},{0x046ea523,60}};
            inputs[i].lifecycle.suppress_stage_text=false; inputs[i].lifecycle.configuration_choices={{0x026e0002,45}};
        }
        if (!check(scene.open(catalog,pristine,inputs,error)&&scene.apply_available(scene.ticket(),&images,error)&&
            scene.update(scene.ticket(),camera(),error)&&!hittable(*scene.card(2)->instance(),scene.frame().viewport),
            "actual untouched template remains hidden/IgnoreMouse after current phase subset, no forced native visibility")) return 1;
        // Controlled picking fixture: clear ONLY the positioning root's IgnoreMouse flag. The real
        // panels remain hidden; this tests layer/identity ordering, not native phase2/show policy.
        for (auto& w:source.windows) if (w.object_index==177) w.flags&=~0x10u;
        const auto template_windows=source.windows;
        if (!check(scene.open(catalog,source,inputs,error)&&scene.size()==3&&scene.ticket()&&!scene.card(3),"explicit Available typed catalogue owns three real-layout cards")) return 1;
        if (!check(scene.open(caller_catalog,source,inputs,error)&&scene.catalog_source()=="caller-catalog.txt"&&scene.producer_observations().manager==CatalogProducerStatus::Available,
            "caller-labelled catalogue requires and retains all producer Available observations")) return 1;
        const auto ticket=scene.ticket();
        if (!check(scene.card(0)->instance()->entry().source_index==0&&scene.card(1)->instance()->entry().source_index==0&&
            scene.card(2)->instance()->entry().source_index==1&&scene.card(0)->instance()->entry().star_id==scene.card(2)->instance()->entry().star_id,
            "saved/candidate ordinals reset separately, duplicate star IDs preserved, host IDs independent")) return 1;
        if (!check(scene.layers(1).empty()&&!scene.pick(1,512,384),"no projection publication -> no layers/hits")) return 1;
        if (!check(scene.apply_available(ticket,&images,error)&&!scene.native_scene_ready(),"host image/phase actions separate from native scene")) return 1;
        if (!check(scene.card(0)->instance()->layout().runtime_animations.at(45).image.pixels&&
            scene.card(0)->instance()->layout().runtime_animations.at(45).image.pixels->provenance.find("(file-backed)")!=std::string::npos,
            "direct original atlas retained inside scene-owned lifecycle")) return 1;
        auto c=camera();
        if (!check(scene.update(ticket,c,error)&&scene.frame().status==StarSceneFrameStatus::Projected&&scene.frame().cards.size()==3,
            "active observation drives projection/placement, not factory creation")) return 1;
        auto serial=scene.frame().serial;
        const auto area=root_area(*scene.card(2));
        if (!check(scene.frame().viewport.left==0&&scene.frame().viewport.top==0&&scene.frame().viewport.w()==1024&&
            area.left==512-area.w()*.5f&&area.top==384-area.h()*.5f,
            "projection uses EXTENTS, never adds observed window origin")) return 1;
        const auto layers=scene.layers(serial);
        if (!check(layers.size()==3&&layers[0].host_id==100&&layers[2].host_id==102&&layers[2].entry_index==2,
            "catalogue-order layer views preserve host identity")) return 1;
        const auto point=hittable(*scene.card(2)->instance(),scene.frame().viewport);
        if (!check(point.has_value(),"real placed card has hittable window")) return 1;
        const auto hit=scene.pick(serial,(*point)[0],(*point)[1]);
        if (!check(hit&&hit->ticket==ticket&&hit->frame_serial==serial&&hit->host_id==102&&hit->entry_index==2&&hit->window_object!=0xffff,
            "overlapping cards reverse-pick same paint order, return value identity not native selection")) return 1;
        if (!check(!scene.pick(serial,-1,20)&&!scene.pick(serial,1024,20)&&
            !scene.pick(serial,std::numeric_limits<float>::quiet_NaN(),20),"viewport half-open/NaN picking guards")) return 1;
        const auto old_serial=serial;
        (*c.projection.clip_from_world)[12]=8.f/1024;
        if (!check(scene.update(ticket,c,error)&&root_area(*scene.card(2)).left==area.left&&
            !scene.frame().cards[2].placement->move_required&&scene.layers(old_serial).empty(),"exact 4px suppressed; old frame retired even without movement")) return 1;
        (*c.projection.clip_from_world)[12]=8.125f/1024;
        if (!check(scene.update(ticket,c,error)&&scene.frame().cards[2].placement->move_required&&
            root_area(*scene.card(2)).left==area.left+4.0625f,"strict >4px threshold commits owned graph move")) return 1;
        serial=scene.frame().serial;
        auto lost=camera(); lost.source=SceneCameraSource::ControllerRetained;
        if (!check(!scene.update(ticket,lost,error)&&scene.frame().status==StarSceneFrameStatus::Unavailable&&scene.layers(serial).empty()&&
            scene.layers(scene.frame().serial).empty()&&!scene.pick(serial,(*point)[0],(*point)[1]),
            "controller198 observation cannot substitute; camera loss retires old draw/pick snapshot")) return 1;
        lost=camera(); lost.token=0;
        if (!check(!scene.update(ticket,lost,error),"unobserved camera identity no implicit service token")) return 1;
        lost=camera(); lost.projection.clip_from_world.reset();
        if (!check(!scene.update(ticket,lost,error),"missing matrix not identity camera")) return 1;
        lost=camera(); lost.projection.window_rect.reset();
        if (!check(!scene.update(ticket,lost,error),"missing viewport not default screen")) return 1;
        lost=camera(); lost.projection.window_rect=spui::Rect{0,0,0,768};
        if (!check(!scene.update(ticket,lost,error),"invalid window extents retire frame")) return 1;
        lost=camera(); (*lost.projection.clip_from_world)[0]=std::numeric_limits<float>::infinity();
        if (!check(!scene.update(ticket,lost,error),"nonfinite matrix globally rejected")) return 1;
        c=camera(); (*c.projection.clip_from_world)[11]=1; (*c.projection.clip_from_world)[15]=0;
        if (!check(scene.update(ticket,c,error)&&scene.frame().cards[0].status==CardStatus::ProjectionFailed&&
            scene.frame().cards[1].status==CardStatus::Positioned&&scene.layers(scene.frame().serial).size()==2,
            "zero-w card fails explicitly, other observed cards remain positioned")) return 1;
        c=camera(); (*c.projection.clip_from_world)[15]=-1;
        if (!check(scene.update(ticket,c,error)&&scene.layers(scene.frame().serial).size()==3&&
            scene.frame().cards[2].projection->clip_w<0,"negative w not silently culled by host collection")) return 1;
        auto no_insets=inputs; no_insets[0].saved_insets.reset();
        if (!check(scene.open(catalog,source,no_insets,error)&&!scene.update(ticket,camera(),error)&&
            scene.update(scene.ticket(),camera(),error)&&scene.frame().cards[0].status==CardStatus::PlacementFailed&&
            scene.layers(scene.frame().serial).size()==2,"missing saved clamp observation blocks only saved card; stale scene ticket rejected")) return 1;
        auto clamp=inputs; clamp[0].saved_insets=SavedInsets{20,30,40,50};
        c=camera(); (*c.projection.clip_from_world)[12]=100; (*c.projection.clip_from_world)[13]=-100;
        if (!check(scene.open(catalog,source,clamp,error)&&scene.update(scene.ticket(),c,error)&&
            root_area(*scene.card(0)).left==984&&root_area(*scene.card(0)).top==718&&
            root_area(*scene.card(2)).left>1024,"saved clamp uses original bounds, candidates not artificially clamped")) return 1;
        auto bad_insets=inputs; bad_insets[0].saved_insets=SavedInsets{2000,0,0,0};
        if (!check(scene.open(catalog,source,bad_insets,error)&&scene.update(scene.ticket(),camera(),error)&&
            scene.frame().cards[0].status==CardStatus::PlacementFailed,"inverted saved clamp range explicit failure")) return 1;
        if (!check(scene.open(catalog,source,inputs,error)&&scene.apply_available(scene.ticket(),&images,error)&&scene.update(scene.ticket(),camera(),error),"fresh scene snapshot")) return 1;
        serial=scene.frame().serial; const auto old_ticket=scene.ticket(); const auto* old_card=scene.card(0);
        auto invalid=inputs; invalid[2].host_id=invalid[0].host_id;
        if (!check(!scene.open(catalog,source,invalid,error)&&scene.ticket()==old_ticket&&scene.card(0)==old_card&&
            !scene.layers(serial).empty(),"failed reopen preserves existing snapshot/ownership")) return 1;
        invalid=inputs; invalid[0].host_id=0;
        if (!check(!scene.open(catalog,source,invalid,error),"zero host identity rejected")) return 1;
        invalid=inputs; invalid.pop_back();
        if (!check(!scene.open(catalog,source,invalid,error),"card inputs must align with complete catalogue order")) return 1;
        if (!check(!scene.open(unknown,source,{},error)&&scene.ticket()==old_ticket,"Unknown refresh not fabricated as empty catalogue")) return 1;
        if (!check(scene.size()==3&&scene.catalog_status()==CatalogAvailability::Unknown&&scene.layers(serial).empty()&&
            !scene.apply_available(old_ticket,&images,error)&&!scene.update(old_ticket,camera(),error),
            "catalogue observation loss suspends old draw/pick/update path without deleting diagnostic cards or inventing empty source")) return 1;
        auto huge=catalog; huge.candidates.resize(129);
        if (!check(!scene.open(huge,source,{},error),"host card-count allocation budget")) return 1;
        auto heavy=source; heavy.objects.resize(65536/3+1);
        if (!check(!scene.open(catalog,heavy,inputs,error),"aggregate object budget before per-card copies")) return 1;
        heavy=source; heavy.windows.resize(32768/3+1);
        if (!check(!scene.open(catalog,heavy,inputs,error),"aggregate window budget before per-card copies")) return 1;
        auto nonfinite_catalog=catalog; nonfinite_catalog.candidates[0].position.x=std::numeric_limits<float>::quiet_NaN();
        if (!check(!scene.open(nonfinite_catalog,source,inputs,error)&&scene.card(0)==old_card,
            "invalid Available metadata rejected transactionally, no partial catalogue projection")) return 1;
        invalid=inputs; invalid[1].lifecycle.phase1.choices.push_back({0x046a9ca7,87});
        if (!check(!scene.open(catalog,source,invalid,error)&&scene.card(0)==old_card,"later-card failed planning does not publish partial new catalogue")) return 1;
        Reentry backend; backend.scene=&scene; backend.catalog=&catalog; backend.source=&source; backend.cards=&inputs; backend.backend=&images;
        if (!check(scene.open(catalog,source,inputs,error)&&scene.apply_available(scene.ticket(),&backend,error)&&backend.calls&&backend.denied,
            "provider reentry cannot close/reopen/update scene or borrow layers during dirty pass")) return 1;
        if (!check(scene.frame().status==StarSceneFrameStatus::Unavailable&&scene.layers(serial).empty()&&
            scene.update(scene.ticket(),camera(),error),"asset changes require new projection publication")) return 1;
        auto disabled=source;
        for (auto& w:disabled.windows) if (w.object_index==177) w.flags&=~2u;
        if (!check(scene.open(catalog,disabled,inputs,error)&&scene.apply_available(scene.ticket(),&images,error)&&scene.update(scene.ticket(),camera(),error)&&
            !scene.pick(scene.frame().serial,512,384),"ancestor-disabled cards draw structurally but are not pickable")) return 1;
        for (size_t i=0;i<source.windows.size();++i) {
            if (!check(source.windows[i].area.left==template_windows[i].area.left&&source.windows[i].flags==template_windows[i].flags&&
                source.runtime_images.empty()&&source.runtime_animations.empty(),"scene projection/flags/images do not mutate original template")) return 1;
        }
        CatalogInput empty; empty.saved_status=empty.candidate_status=CatalogAvailability::Available;
        if (!check(scene.open(empty,source,{},error)&&scene.size()==0&&scene.update(scene.ticket(),camera(),error)&&
            scene.frame().status==StarSceneFrameStatus::Projected&&scene.layers(scene.frame().serial).empty(),"observed Available-empty distinct from Unknown, no fabricated stars")) return 1;
        const auto retired=scene.ticket();
        if (!check(scene.close(error)&&!scene.ticket()&&!scene.card(0)&&!scene.apply_available(retired,&images,error)&&
            !scene.update(retired,camera(),error)&&scene.layers(serial).empty(),"close retires ownership/tickets/frame views")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both original star templates")) return 1;
    std::cout<<checks<<" host scene/catalogue/active-camera/projection/placement/pick/ownership checks PASS; controlled metadata/camera, actual SPUI/PNG; no native galaxy or automatic GGE caller\n";
    return 0;
}
