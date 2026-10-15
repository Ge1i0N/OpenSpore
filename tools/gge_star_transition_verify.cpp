// Actual layouts + explicit CONTROLLED current-phase/caller observations. Never native dispatch.
#include "engine/gge_star_scene.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
using namespace os;
using namespace os::gge;
using Op=StarTransitionOp;
using Status=StarHostActionStatus;
static const spui::Window& window(const StarUiInstance& i,u16 object) {
    for (const auto& w:i.layout().windows) if (w.object_index==object) return w;
    throw std::runtime_error("test window missing");
}
static bool visible(const StarUiInstance& i,u16 object) { return window(i,object).visible; }
static const StarTransitionResult& outcome(const StarUiLifecycle& c,Op op,u32 control=0) {
    const auto& r=*c.report().transition;
    for (size_t n=0;n<r.plan.requests.size();++n) if (r.plan.requests[n].operation==op&&r.plan.requests[n].target.control==control) return r.results[n];
    throw std::runtime_error("test transition action missing");
}
static SceneCameraObservation camera() {
    SceneCameraObservation c; c.source=SceneCameraSource::ActiveProjection; c.token=1;
    c.projection.clip_from_world=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    c.projection.window_rect=spui::Rect{0,0,1024,768}; return c;
}
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr<<"usage: gge_star_transition_verify <SPUI-fixtures>\n"; return 2; }
    size_t checks=0,fixtures=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>name>>bytes>>objects>>windows>>props>>links>>roots) {
        if (name.find("21df01f2")==std::string::npos) continue;
        std::ifstream f(std::filesystem::path(argv[1])/name,std::ios::binary); std::vector<u8> data(std::istreambuf_iterator<char>(f),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error),"actual star layout")) return 1;
        source.instance=kStarUiLayout; const auto original=source.windows;
        GameEntry entry; entry.name=u"controlled phase name"; entry.saved=true; entry.stage=4;
        StarLifecycleInputs init; init.phase1.thumbnail=ImageKey{}; init.phase1.has_profile=false;
        init.phase1.selected_star_saved=false; init.phase1.selected_star_present=false;
        init.phase1.choices={{0x046a9ca7,90},{0x026e0002,55},{0x046ea523,60}};
        init.suppress_stage_text=false; init.configuration_choices={{0x026e0002,45}};
        StarUiLifecycle card;
        const auto reset=[&]() { return card.open(77,source,entry,init,error)&&card.apply_available(card.report().ticket,nullptr,error); };
        if (!check(reset()&&!visible(*card.instance(),41)&&!visible(*card.instance(),90)&&!visible(*card.instance(),176),
            "phase1 actual template panels hidden; no fixture flag changes")) return 1;
        const auto ticket=card.report().ticket; StarTransitionInputs request; request.requested_case=2; request.choices={{0x046a9ca7,90}};
        if (!check(card.apply_phase_transition(ticket,request,error)&&card.report().transition->plan.guard==StarTransitionGuard::UnknownCurrent&&
            card.report().transition->plan.requests.empty()&&!card.report().transition->plan.blocked_on.empty()&&!visible(*card.instance(),41),
            "Unknown current phase blocks whole body, no guessed phase1 success latch")) return 1;
        request.observed_current_case=2;
        if (!check(card.apply_phase_transition(ticket,request,error)&&card.report().transition->plan.guard==StarTransitionGuard::AlreadyCurrent&&
            card.report().transition->results.empty()&&!visible(*card.instance(),41),"observed equal phase skips event and flags even if host window differs")) return 1;
        request.observed_current_case=1;
        if (!check(card.apply_phase_transition(ticket,request,error)&&card.report().transition->plan.requests.size()==4&&
            card.report().transition->plan.requests[0].event==0xd06d9182u&&outcome(card,Op::ServiceEvent).status==Status::Unsupported&&
            visible(*card.instance(),41)&&visible(*card.instance(),28)&&!visible(*card.instance(),90),
            "case2 event requirement then exact hide/show flags, no enabled-mask substitution")) return 1;
        if (!check(card.report().transition->plan.observed_current_case==1u&&!card.native_mount_complete()&&!card.native_services_executed(),
            "input guard snapshot retained unchanged, no synthetic native phase2 latch")) return 1;
        const auto& normal=window(*card.instance(),41); const float x=normal.placed_area.left+normal.placed_area.w()*.5f;
        const float y=normal.placed_area.top+normal.placed_area.h()*.5f;
        if (!check(card.instance()->pick(x,y,{0,0,1024,768})&&window(*card.instance(),177).flags==19,
            "case2 makes actual panel pickable WITHOUT clearing root IgnoreMouse or revealing all panels")) return 1;
        if (!check(card.apply_available(ticket,nullptr,error)&&visible(*card.instance(),41),"settled initial actions not replayed to undo later show")) return 1;
        request.requested_case=4; request.choices.clear(); request.observed_current_case=2;
        if (!check(card.apply_phase_transition(ticket,request,error)&&card.report().transition->plan.requests.size()==3&&
            card.report().transition->plan.requests[0].event==0xd2fc3ad5u&&!visible(*card.instance(),41)&&!visible(*card.instance(),28)&&
            card.report().transition->plan.observed_current_case==2u,"case4 hides only its controls; native path does NOT write +2C; host writes none")) return 1;
        if (!check(reset(),"fresh card before saved branch")) return 1;
        request={}; request.requested_case=3; request.observed_current_case=1; request.choices={{0x046a9ca7,90}};
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&card.report().transition->plan.requests.size()==6&&
            visible(*card.instance(),90)&&visible(*card.instance(),28)&&!visible(*card.instance(),41)&&!visible(*card.instance(),176),
            "case3 reveals selected saved panel only, source order/count retained")) return 1;
        if (!check(outcome(card,Op::ServiceControlRequest,0x046a9ca7).status==Status::Unsupported&&
            window(*card.instance(),card.instance()->control_objects(0x046ea550u)[0]).caption=="controlled phase name"&&
            outcome(card,Op::EntryName,0x046ea550u).status==Status::Applied,
            "unimplemented service control request not called; independent names are exact entry text")) return 1;
        request.requested_case=5; request.observed_current_case=3;
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&card.report().transition->plan.requests.size()==6&&
            card.report().transition->plan.requests[0].event==0x6c6da889u&&!visible(*card.instance(),41)&&!visible(*card.instance(),90)&&
            visible(*card.instance(),176)&&visible(*card.instance(),28)&&outcome(card,Op::CopyWindowRect,0x05b31710u).status==Status::Applied&&
            window(*card.instance(),28).area.left==window(*card.instance(),95).area.left&&
            window(*card.instance(),28).area.top==window(*card.instance(),95).area.top&&
            window(*card.instance(),28).area.w()==window(*card.instance(),95).area.w()&&
            window(*card.instance(),28).area.h()==window(*card.instance(),95).area.h(),
            "case5 panel176 plus exact local rectangle copy before related-panel show, no parent-space conversion")) return 1;
        StarUiInstance primitive;
        if (!check(primitive.instantiate(8,source,entry,error)&&!primitive.copy_local_rect(0xffff,95,error)&&
            !primitive.copy_local_rect(28,0xffff,error)&&!primitive.copy_local_rect(177,95,error),"rectangle-copy bad objects/root host guard")) return 1;
        const auto area=window(primitive,28).area;
        auto malformed=source; for (auto& w:malformed.windows) if (w.object_index==95) { w.area.right=w.area.left; }
        if (!check(primitive.instantiate(8,malformed,entry,error)&&!primitive.copy_local_rect(28,95,error)&&window(primitive,28).area.left==area.left,
            "collapsed source rect rejected without corrupting prior target")) return 1;
        if (!check(reset(),"fresh ambiguous card")) return 1;
        request={}; request.requested_case=3; request.observed_current_case=1;
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&outcome(card,Op::Flag,0x046a9ca7u).status==Status::Ambiguous&&
            !visible(*card.instance(),90)&&card.report().transition->plan.requests[1].target.candidates==std::vector<u16>{87,90},
            "duplicate panel remains Ambiguous; no first-match/class/visible choice")) return 1;
        request.choices={{0x046a9ca7u,87}};
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&(window(*card.instance(),87).flags&1u)&&
            !visible(*card.instance(),87)&&!visible(*card.instance(),90),"explicit inner duplicate respected; hidden ancestor not forced visible")) return 1;
        StarTransitionPlan sentinel; sentinel.instance_id=999;
        const auto reject=[&](StarTransitionInputs bad) {
            return !plan_star_transition(*card.instance(),bad,sentinel,error)&&sentinel.instance_id==999&&
                !card.apply_phase_transition(card.report().ticket,bad,error)&&card.report().transition->plan.requests[1].target.object==u16{87};
        };
        for (u32 phase:{0u,1u,6u,0xffffffffu}) { auto bad=request; bad.requested_case=phase; if (!check(reject(bad),"out-of-scope case fails atomically, not native false return")) return 1; }
        auto bad=request; bad.choices={{0x046a9ca7u,0xffff}};
        if (!check(reject(bad),"wrong object choice transactional")) return 1;
        bad=request; bad.choices.push_back({0x046a9ca7u,90});
        if (!check(reject(bad),"conflicting duplicate choices rejected")) return 1;
        bad=request; bad.choices={{0xdeadbeefu,90}};
        if (!check(reject(bad),"unrelated control not ignored")) return 1;
        bad=request; bad.choices.resize(17);
        if (!check(reject(bad),"choice allocation bound")) return 1;
        if (!check(!card.apply_phase_transition(card.report().ticket+1,request,error),"stale lifecycle generation rejected before mutation")) return 1;
        request.observed_current_case=0;
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&card.report().transition->plan.guard==StarTransitionGuard::CaseBody,
            "observed zero phase is a value, NOT Unknown")) return 1;
        request.observed_current_case=0xffffffffu;
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&card.report().transition->plan.guard==StarTransitionGuard::CaseBody,
            "raw observed current word retains native equality semantics")) return 1;
        auto missing=source; for (auto& w:missing.windows) if (w.object_index==28) w.control_id^=0x80000000u;
        request={5u,1u,{{0x046a9ca7u,90}}};
        if (!check(card.open(77,missing,entry,init,error)&&card.apply_available(card.report().ticket,nullptr,error)&&
            card.apply_phase_transition(card.report().ticket,request,error)&&outcome(card,Op::CopyWindowRect,0x05b31710u).status==Status::Missing&&
            outcome(card,Op::Flag,0x05b31710u).status==Status::Missing,
            "controlled missing target stays Missing, not guessed native optional completion")) return 1;
        auto duplicate_source=source; for (auto& w:duplicate_source.windows) if (w.object_index==94) w.control_id=0x05b34d93u;
        if (!check(card.open(77,duplicate_source,entry,init,error)&&card.apply_available(card.report().ticket,nullptr,error)&&
            card.apply_phase_transition(card.report().ticket,request,error)&&outcome(card,Op::CopyWindowRect,0x05b31710u).status==Status::Ambiguous,
            "controlled duplicate rect source not first-selected")) return 1;
        request.choices.push_back({0x05b34d93u,95});
        if (!check(card.apply_phase_transition(card.report().ticket,request,error)&&outcome(card,Op::CopyWindowRect,0x05b31710u).status==Status::Applied&&
            card.report().transition->plan.requests[4].source->candidates.size()==2,
            "explicit rect-source object choice retains all candidates")) return 1;
        // A complete lifecycle may have distinct configured names, but transitions never fabricate profiles.
        if (!check(card.close(error)&&!card.apply_phase_transition(1,request,error),"closed lifecycle rejects transition")) return 1;
        CatalogInput catalog; catalog.saved_status=catalog.candidate_status=CatalogAvailability::Available;
        StarMetadata star; star.star_id=42; star.name=u"actual-panel controlled metadata"; catalog.candidates={star};
        StarSceneCardInput scene_card; scene_card.host_id=1; scene_card.lifecycle=init;
        StarUiScene scene;
        if (!check(scene.open(catalog,source,{scene_card},error)&&scene.apply_available(scene.ticket(),nullptr,error)&&scene.update(scene.ticket(),camera(),error),
            "scene baseline original template/camera observations")) return 1;
        auto serial=scene.frame().serial;
        if (!check(!scene.pick(serial,512,384),"initial raw card still IgnoreMouse/hidden after phase1")) return 1;
        request={}; request.requested_case=2; request.observed_current_case=1; request.choices={{0x046a9ca7u,90}};
        if (!check(scene.apply_phase_transition(scene.ticket(),0,request,error)&&scene.layers(serial).empty()&&!scene.pick(serial,512,384)&&
            scene.update(scene.ticket(),camera(),error),"explicit scene phase request retires previous draw/pick before republishing")) return 1;
        const auto& panel=window(*scene.card(0)->instance(),41);
        const auto hit=scene.pick(scene.frame().serial,panel.placed_area.left+panel.placed_area.w()*.5f,panel.placed_area.top+panel.placed_area.h()*.5f);
        if (!check(hit&&hit->entry_index==0&&visible(*scene.card(0)->instance(),41)&&!visible(*scene.card(0)->instance(),90)&&
            window(*scene.card(0)->instance(),177).flags==19&&!scene.native_scene_ready(),
            "real case2 panel hit on untouched source flags, not controlled IgnoreMouse workaround/native dispatch")) return 1;
        serial=scene.frame().serial;
        if (!check(!scene.apply_phase_transition(scene.ticket(),1,request,error)&&!scene.apply_phase_transition(scene.ticket()+1,0,request,error)&&
            !scene.layers(serial).empty(),"invalid scene target/ticket does not retire valid snapshot")) return 1;
        if (!check(!scene.open(CatalogInput{},source,{},error)&&!scene.apply_phase_transition(scene.ticket(),0,request,error),
            "unavailable catalogue blocks transition as well as draw/picking")) return 1;
        for (size_t i=0;i<source.windows.size();++i) if (!check(source.windows[i].flags==original[i].flags&&source.windows[i].caption==original[i].caption&&
            source.windows[i].area.left==original[i].area.left,"phase changes isolate raw source/template")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both original layouts")) return 1;
    std::cout<<checks<<" observed phase-case/guard/visibility/name/duplicate/scene-picking checks PASS; controlled native phase observations, actual SPUI; no native latch/services/dispatch\n";
    return 0;
}
