// Static binding plan, real templates plus controlled dynamic observations. No native calls.
#include "engine/gge_star_phase1.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace os;
using namespace os::gge;
static const Phase1Request* find(const StarPhase1Plan& plan,Phase1Op op,u32 control) {
    for (const auto& request:plan.requests) if (request.operation==op&&request.target.control==control) return &request;
    return nullptr;
}
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr<<"usage: gge_star_phase1_verify <SPUI-fixture-dir>\n"; return 2; }
    size_t checks=0,fixtures=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt");
    std::string file,error; size_t bytes,objects,windows,props,links,roots;
    while (manifest>>file>>bytes>>objects>>windows>>props>>links>>roots) {
        if (file.find("21df01f2")==std::string::npos) continue;
        std::ifstream input(std::filesystem::path(argv[1])/file,std::ios::binary);
        std::vector<u8> data(std::istreambuf_iterator<char>(input),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error)&&source.windows.size()==58,"actual layout parse")) return 1;
        source.instance=kStarUiLayout;
        GameEntry entry; entry.saved=true; entry.stage=5; entry.name=u"controlled"; entry.image={123,0x2f7d0004,456};
        StarUiInstance instance;
        if (!check(instance.instantiate(99,source,entry,error),"instance")) return 1;
        StarPhase1Plan plan; StarPhase1Inputs observed;
        if (!check(plan_star_phase1(instance,observed,plan,error)&&!plan.inspected_branches_observed&&
            !plan.blocked_on.empty()&&plan.requests.size()==8&&plan.requests[0].target.control==kStarUiRoot&&
            plan.requests[0].mask==1&&plan.requests[0].flag_value&&!plan.native_mount_complete&&!plan.native_services_executed,
            "Unknown thumbnail yields exact bounded prefix, not absent-image branch")) return 1;
        if (!check(plan.requests[6].target.control==0x046a9ca7u&&plan.requests[6].target.status==StarBindingStatus::Ambiguous&&
            plan.requests[6].target.candidates.size()==2&&!plan.requests[6].target.object,
            "duplicate phase1 panel not silently first-selected")) return 1;
        observed.thumbnail=ImageKey{};
        if (!check(plan_star_phase1(instance,observed,plan,error)&&!plan.blocked_on.empty()&&
            find(plan,Phase1Op::Flag,0x02e71340)&&!find(plan,Phase1Op::ImageResource,0x02e71340)&&
            find(plan,Phase1Op::Flag,0x068dac20)->flag_value&&
            find(plan,Phase1Op::ImageResource,0x046a9a21)->resource->instance==123,
            "observed zero thumbnail instance takes native absent branch, main image unaffected")) return 1;
        const auto* stage_text=find(plan,Phase1Op::StageDescription,0x0594a640);
        if (!check(stage_text&&stage_text->locale&&stage_text->locale->table==0x2b7a0311u&&
            stage_text->locale->key==0x05b08706u&&!find(plan,Phase1Op::StageImageSource,0x03744ff5u),
            "correct phase1 locale, unlike DF9200 caption; later ops absent until profile branch observed")) return 1;
        observed.thumbnail=ImageKey{789,0x2f7d0004,9}; observed.has_profile=false;
        if (!check(plan_star_phase1(instance,observed,plan,error)&&
            find(plan,Phase1Op::ImageResource,0x02e71340)->resource->instance==789&&
            !find(plan,Phase1Op::Flag,0x068dac20)&&!find(plan,Phase1Op::ProfileName,0x05ff9ad2)&&
            find(plan,Phase1Op::LayerReference,0x05b31710)->source->control==0x05b347bcu&&
            !plan.inspected_branches_observed,"thumbnail present and profile observed absent, selected-star byte still Unknown")) return 1;
        observed.selected_star_saved=false;
        if (!check(plan_star_phase1(instance,observed,plan,error)&&
            find(plan,Phase1Op::StageImageSource,0x03744ff5u)&&find(plan,Phase1Op::EntryName,0x02e712e4)&&
            !find(plan,Phase1Op::SavedTerrainLayer,0x05b31710)&&!plan.inspected_branches_observed,
            "stage/name requests after observed false saved byte, timestamp presence still Unknown")) return 1;
        observed.selected_star_present=false;
        if (!check(plan_star_phase1(instance,observed,plan,error)&&plan.inspected_branches_observed&&plan.blocked_on.empty()&&
            !plan.native_mount_complete&&!plan.native_services_executed&&!find(plan,Phase1Op::SavedTimestamp,0x046a9a9b),
            "all branch predicates observed != native readiness or service calls")) return 1;
        const auto* ambiguous=find(plan,Phase1Op::StageImageTarget,0x026e0002);
        if (!check(ambiguous&&ambiguous->target.status==StarBindingStatus::Ambiguous&&ambiguous->target.candidates.size()==2&&
            ambiguous->source&&ambiguous->source->control==0x03744ff5u&&
            ambiguous->resource&&ambiguous->resource->instance==0&&ambiguous->resource->type==0&&
            ambiguous->resource->group==0&&ambiguous->resource_status==StarBindingStatus::Unknown,
            "stage source distinct, zero fallback key explicit, no fake loaded image")) return 1;
        const auto candidates=ambiguous->target.candidates;
        for (u16 selected:candidates) {
            observed.choices={{0x026e0002,selected}};
            if (!check(plan_star_phase1(instance,observed,plan,error)&&
                find(plan,Phase1Op::StageImageTarget,0x026e0002)->target.object==selected&&
                find(plan,Phase1Op::StageImageTarget,0x026e0002)->target.status==StarBindingStatus::Found&&
                find(plan,Phase1Op::StageImageTarget,0x026e0002)->target.candidates.size()==2,
                "caller explicit duplicate choice does not discard candidates")) return 1;
        }
        observed.choices.clear(); observed.has_profile=true; observed.selected_star_saved=true; observed.selected_star_present=true;
        if (!check(plan_star_phase1(instance,observed,plan,error)&&find(plan,Phase1Op::ProfileName,0x05ff9ad2)&&
            find(plan,Phase1Op::ProfileName,0x05ff9d44)&&find(plan,Phase1Op::SavedTerrainLayer,0x05b31710)&&
            find(plan,Phase1Op::SavedTimestamp,0x046a9a9b)&&find(plan,Phase1Op::TimestampColour,0x047e3cd3)&&
            plan.requests.back().operation==Phase1Op::TimestampColour,
            "profile/selected star true branches retained without execution")) return 1;
        for (const auto& request:plan.requests)
            if (!check(request.resource_status==StarBindingStatus::Unknown&&
                request.required_drawable_interface==((request.operation==Phase1Op::ImageResource||
                request.operation==Phase1Op::StageImageSource||request.operation==Phase1Op::StageImageTarget)?0xef3c47cfu:0u),
                "no fake resource Found; drawable image interface distinct from Animated Icon")) return 1;
        const auto reject=[&](const StarPhase1Inputs& invalid) {
            StarPhase1Plan sentinel; sentinel.instance_id=777; sentinel.blocked_on="unchanged";
            return !plan_star_phase1(instance,invalid,sentinel,error)&&!error.empty()&&sentinel.instance_id==777&&
                sentinel.blocked_on=="unchanged"&&sentinel.requests.empty();
        };
        auto invalid=observed; invalid.choices={{0x026e0002,0xffff}};
        if (!check(reject(invalid),"wrong object choice atomic")) return 1;
        invalid=observed; invalid.choices={{0x026e0002,candidates[0]},{0x026e0002,candidates[1]}};
        if (!check(reject(invalid),"duplicate conflicting choices not ignored")) return 1;
        invalid=observed; invalid.choices={{0xdeadbeef,candidates[0]}};
        if (!check(reject(invalid),"unrelated control mapping invalid")) return 1;
        invalid=observed; invalid.choices.resize(65);
        if (!check(reject(invalid),"host choice cap")) return 1;
        // Stage-dependent locale/source IDs. No first-image or invented out-of-range stage fallback.
        for (u32 stage:{0u,1u,2u,3u,4u,5u,6u,0xffffffffu}) {
            entry.stage=stage; entry.saved=false;
            if (!check(instance.instantiate(99,source,entry,error)&&plan_star_phase1(instance,observed,plan,error),"stage variants")) return 1;
            const auto* description=find(plan,Phase1Op::StageDescription,0x0594a640);
            const u32 key=stage==0?0x05b08702u:stage==1?0x05b08703u:stage==2?0x05b08704u:
                          stage==4?0x05b08705u:stage==5?0x05b08706u:0u;
            if (!check(description&&(key?(description->locale&&description->locale->key==key):!description->locale)&&
                find(plan,Phase1Op::StageImageSource,stage+0x03744ff0u)&&
                find(plan,Phase1Op::LayerReference,0x05b31710)->source->control==0x04656452u,
                "phase1 stages + uint32 source arithmetic + new layer source")) return 1;
            if (stage==6||stage==0xffffffffu)
                if (!check(find(plan,Phase1Op::StageImageSource,stage+0x03744ff0u)->target.status==StarBindingStatus::Missing,
                           "absent source remains Missing, not a fabricated image")) return 1;
        }
        for (const auto& window:source.windows) {
            for (const auto& cloned:instance.layout().windows) if (cloned.object_index==window.object_index)
                if (!check(cloned.flags==window.flags&&cloned.area.left==window.area.left&&cloned.caption==window.caption,
                           "planner never mutates host instance/template")) return 1;
        }
        instance.clear(); StarPhase1Plan sentinel; sentinel.instance_id=777;
        if (!check(!plan_star_phase1(instance,observed,sentinel,error)&&sentinel.instance_id==777,"inactive instance unknown is error unchanged")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both original package layouts inspected")) return 1;
    std::cout<<checks<<" phase1 binding/branch/order/unknown/duplicate/no-mutation checks PASS; "<<fixtures
        <<" real layouts; no native phase completion or scene readiness\n";
    return 0;
}
