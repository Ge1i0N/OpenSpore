// Host lifecycle only: original layouts/PNG bytes + controlled metadata/provider failures/reentry.
#include "engine/gge_star_lifecycle.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace os;
using namespace os::gge;
using Status=StarHostActionStatus;
using Kind=StarHostActionKind;
using Bytes=std::vector<u8>;
static void le(Bytes& b,size_t at,u32 v) { for (size_t i=0;i<4;++i) b[at+i]=(u8)(v>>(8*i)); }
static Bytes carrier(const spui::ExternalResource& k,const Bytes& png) {
    const size_t off=0x60+png.size(); Bytes b(off+36,0);
    le(b,0,0x46504244); le(b,4,3); le(b,0x24,1); le(b,0x2c,36); le(b,0x3c,3); le(b,0x40,(u32)off);
    std::copy(png.begin(),png.end(),b.begin()+0x60); le(b,off,4); le(b,off+8,k.type); le(b,off+12,k.group);
    le(b,off+16,k.instance); le(b,off+20,0x60); le(b,off+24,(u32)png.size()); le(b,off+28,(u32)png.size()); le(b,off+32,0x10000); return b;
}
static const spui::Window& window(const StarUiInstance& instance,u16 object) {
    for (const auto& w:instance.layout().windows) if (w.object_index==object) return w;
    throw std::runtime_error("test object missing");
}
static const StarHostAction& action(const StarUiLifecycle& host,Kind kind,u32 control,Phase1Op op=Phase1Op::Flag) {
    for (const auto& a:host.report().actions) if (a.kind==kind&&a.request.target.control==control&&
        (kind!=Kind::Phase1||a.request.operation==op)) return a;
    throw std::runtime_error("test action missing");
}
static bool same(const ImageKey& a,const spui::ExternalResource& b) { return a.instance==b.instance&&a.type==b.type&&a.group==b.group; }
struct Provider final:ImageProvider {
    enum class Mode { Missing, Found, Unknown, Ambiguous, Unsupported, Failed, Throw, Invalid } mode=Mode::Missing;
    ImageRead main,atlas; unsigned calls=0,main_calls=0,atlas_calls=0;
    StarUiLifecycle* reenter=nullptr; const spui::Layout* source=nullptr; GameEntry entry; StarLifecycleInputs inputs;
    bool denied=true;
    ImageRead read(const ImageKey& key) override {
        ++calls;
        if (same(key,main.image.resource)) ++main_calls;
        if (same(key,atlas.image.resource)) ++atlas_calls;
        if (reenter) {
            std::string error; const auto ticket=reenter->report().ticket;
            denied=denied&&!reenter->close(error)&&!reenter->open(99,*source,entry,inputs,error)&&
                !reenter->apply_available(ticket,this,error)&&reenter->report().ticket==ticket;
        }
        ImageRead result;
        switch (mode) {
        case Mode::Throw: throw std::runtime_error("controlled provider exception");
        case Mode::Unknown: result.status=ImageReadStatus::Unknown; break;
        case Mode::Ambiguous: result.status=ImageReadStatus::Ambiguous; break;
        case Mode::Unsupported: result.status=ImageReadStatus::Unsupported; break;
        case Mode::Failed: result.status=ImageReadStatus::ReadFailure; break;
        case Mode::Invalid: result.status=ImageReadStatus::Found; break;
        case Mode::Found:
            if (same(key,main.image.resource)) return main;
            if (same(key,atlas.image.resource)) return atlas;
            [[fallthrough]];
        default: result.status=ImageReadStatus::Missing; break;
        }
        result.error="controlled backend result"; return result;
    }
};
int main(int argc,char** argv) {
    if (argc!=4) { std::cerr<<"usage: gge_star_lifecycle_verify <SPUI-fixtures> <explicit-main-PNG-package> <explicit-original-atlas-package>\n"; return 2; }
    size_t checks=0,fixtures=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    ExplicitImageProvider original; ImageKey key; ImageRead main;
    if (!check(original.open(argv[2],error),"explicit PNG package open")) return 1;
    for (const auto& r:original.package().records()) {
        if (r.type()!=0x2f7d0004u||r.mem_size()>1048576||!r.instance()) continue;
        key={r.instance(),r.type(),r.group()}; main=original.read(key);
        if (main.status==ImageReadStatus::Found) break;
    }
    if (!check(main.status==ImageReadStatus::Found,"actual main PNG decode")) return 1;
    pkg::Package atlas;
    if (!check(atlas.open(argv[3]),"explicit original atlas package")) return 1;
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>name>>bytes>>objects>>windows>>props>>links>>roots) {
        if (name.find("21df01f2")==std::string::npos) continue;
        std::ifstream f(std::filesystem::path(argv[1])/name,std::ios::binary); Bytes data(std::istreambuf_iterator<char>(f),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error),"actual layout")) return 1;
        source.instance=kStarUiLayout;
        const auto original_windows=source.windows;
        GameEntry entry; entry.saved=true; entry.stage=4; entry.image=key; entry.name=u"Controlled \u0417\u0435\u043c\u043b\u044f \U0001f30d";
        StarUiInstance sibling;
        if (!check(sibling.instantiate(22,source,entry,error),"independent sibling")) return 1;
        StarLifecycleInputs unknown; StarUiLifecycle host;
        if (!check(!host.apply_available(1,nullptr,error)&&!host.instance(),"closed host rejects pass")) return 1;
        if (!check(host.open(99,source,entry,unknown,error)&&host.report().ticket&&!host.report().phase_blocked_on.empty()&&
            !host.report().configuration_blocked_on.empty()&&host.report().actions.size()==8,"unknown branches preserved independently, no configuration-default false")) return 1;
        if (!check(host.apply_available(host.report().ticket,nullptr,error)&&host.report().passes==1&&
            action(host,Kind::Phase1,0x046a9ca7).status==Status::Ambiguous&&
            action(host,Kind::Phase1,kStarUiRoot).status==Status::Applied,"available prefix executes, duplicate unresolved not first-selected")) return 1;
        const auto root_attempts=action(host,Kind::Phase1,kStarUiRoot).attempts;
        if (!check(host.apply_available(host.report().ticket,nullptr,error)&&action(host,Kind::Phase1,kStarUiRoot).attempts==root_attempts&&
            action(host,Kind::Phase1,0x046a9ca7).attempts==2,"successful work not replayed, unresolved work visibly retried")) return 1;
        StarLifecycleInputs inputs; inputs.phase1.thumbnail=key; inputs.phase1.has_profile=true;
        inputs.phase1.selected_star_saved=true; inputs.phase1.selected_star_present=true; inputs.suppress_stage_text=false;
        inputs.phase1.choices={{0x046a9ca7,90},{0x026e0002,55},{0x046ea523,60}};
        inputs.configuration_choices={{0x026e0002,45}};
        const auto old=host.report().ticket; const auto* prior=host.instance();
        auto bad=inputs; bad.configuration_choices={{0x026e0002,0xffff}};
        if (!check(!host.open(99,source,entry,bad,error)&&host.report().ticket==old&&host.instance()==prior,"invalid configuration choice transactional open")) return 1;
        bad=inputs; bad.phase1.choices.push_back({0x046a9ca7,87});
        if (!check(!host.open(99,source,entry,bad,error)&&host.report().ticket==old,"invalid phase1 choices retain prior owned lifetime")) return 1;
        bad=inputs; bad.configuration_choices={{0x026e0002,45},{0x026e0002,55}};
        if (!check(!host.open(99,source,entry,bad,error),"conflicting configuration choices rejected")) return 1;
        bad=inputs; bad.configuration_choices={{0xdeadbeef,45}};
        if (!check(!host.open(99,source,entry,bad,error),"unrelated configuration choice rejected")) return 1;
        bad=inputs; bad.configuration_choices.resize(17);
        if (!check(!host.open(99,source,entry,bad,error),"configuration choice bound")) return 1;
        if (!check(host.open(99,source,entry,inputs,error)&&host.report().ticket>old&&host.report().phase_blocked_on.empty()&&
            host.report().configuration_blocked_on.empty()&&!host.apply_available(old,nullptr,error),"same host ID new generation rejects stale ticket")) return 1;
        const auto ticket=host.report().ticket;
        if (!check(host.apply_available(ticket,nullptr,error)&&
            action(host,Kind::AnimationPixels,0x026e0002).status==Status::Unknown,
            "no provider gives Unknown not Missing/Found")) return 1;
        Provider provider; provider.main=main; spui::AnimationDescriptor descriptor;
        if (!check(host.instance()->animation(45,descriptor,error),"real animation descriptor")) return 1;
        const auto& r=descriptor.image.resource; const auto matches=atlas.find_records({r.instance,r.type,r.group}); Bytes png;
        ExplicitImageProvider wrapped;
        if (!check(matches.size()==1&&atlas.read_resource_decoded(matches[0],png)&&wrapped.open_from_memory(carrier(r,png),error),
            "unchanged original atlas PNG in explicitly constructed bounded carrier")) return 1;
        provider.atlas=wrapped.read({r.instance,r.type,r.group});
        if (!check(provider.atlas.status==ImageReadStatus::Found,"actual atlas decode, not fake pixels")) return 1;
        if (!check(host.apply_available(ticket,&provider,error)&&provider.main_calls==1&&provider.atlas_calls==1&&
            action(host,Kind::AnimationPixels,0x026e0002).status==Status::Missing,"missing assets explicit, one backend read per key per pass")) return 1;
        if (!check(action(host,Kind::Phase1,0x046ea4b6,Phase1Op::TooltipUserData).status==Status::Unsupported&&
            action(host,Kind::Phase1,0x05ff9ad2,Phase1Op::ProfileName).status!=Status::Applied&&
            action(host,Kind::Phase1,0x05b31710,Phase1Op::LayerReference).status==Status::Unsupported,
            "tooltip/profile/layer operations not silently successful")) return 1;
        if (!check(action(host,Kind::Phase1,0x0594a640,Phase1Op::StageDescription).status==Status::ReferenceBound&&
            action(host,Kind::StageCaption,0x03c4925e).status==Status::ReferenceBound,
            "caption reference binding NOT locale resource availability")) return 1;
        if (!check(window(*host.instance(),host.instance()->control_objects(0x046ea550)[0]).caption==
            "Controlled \xD0\x97\xD0\xB5\xD0\xBC\xD0\xBB\xD1\x8F \xF0\x9F\x8C\x8D", "entry UTF16 name applied exactly as UTF8 after unsupported tooltip, host-independent policy")) return 1;
        i32 frame=-1;
        if (!check(host.instance()->animation_frame(45,frame,error)&&frame==3&&host.instance()->animation_frame(93,frame,error)&&frame==3&&
            action(host,Kind::Phase1,0x026e0002,Phase1Op::StageImageTarget).request.target.object==55&&
            action(host,Kind::StageFrame,0x026e0002).request.target.object==45,
            "separate explicit image vs Animated Icon duplicate choices, stage4 to index3")) return 1;
        spui::ImageQuad quad;
        if (!check(!spui::animation_quad(host.instance()->layout(),45,{0,0,32,32},quad),"assigned index without pixels not drawable ready")) return 1;
        const auto name_attempts=action(host,Kind::Phase1,0x046ea550,Phase1Op::EntryName).attempts;
        provider.mode=Provider::Mode::Found; const auto calls=provider.calls;
        if (!check(host.apply_available(ticket,&provider,error)&&provider.calls==calls+2&&
            action(host,Kind::AnimationPixels,0x026e0002).status==Status::Applied&&
            spui::animation_quad(host.instance()->layout(),45,{0,0,32,32},quad)&&quad.pixels==provider.atlas.image.pixels,
            "retry installs actual shared atlas pixels and retains frame")) return 1;
        if (!check(host.instance()->animation_frame(45,frame,error)&&frame==3&&
            host.instance()->layout().runtime_animations.at(45).image.pixels==host.instance()->layout().runtime_animations.at(93).image.pixels&&
            action(host,Kind::Phase1,0x046ea550,Phase1Op::EntryName).attempts==name_attempts,
            "no reset/replay of frame/name, shared immutable atlas reused")) return 1;
        if (!check(action(host,Kind::Phase1,0x046ea4b6,Phase1Op::ImageResource).status==Status::Applied&&
            spui::drawable_quad(host.instance()->layout(),window(*host.instance(),89).drawable_object,{0,0,64,64},0,quad)&&quad.pixels==main.image.pixels,
            "existing ImageResource executor feeds actual plain drawable in same lifecycle")) return 1;
        const auto ready_calls=provider.calls;
        if (!check(host.apply_available(ticket,&provider,error)&&provider.calls==ready_calls&&!host.native_mount_complete()&&!host.native_services_executed(),
            "settled asset actions not reloaded, no synthetic native phase completion")) return 1;
        for (size_t i=0;i<source.windows.size();++i) {
            const auto& w=source.windows[i]; const auto& original_window=original_windows[i]; const auto& other=sibling.layout().windows[i];
            if (!check(source.runtime_images.empty()&&source.runtime_animations.empty()&&sibling.layout().runtime_animations.empty()&&
                sibling.layout().runtime_images.empty()&&w.flags==original_window.flags&&w.area.left==original_window.area.left&&
                w.caption==original_window.caption&&other.flags==original_window.flags&&other.area.left==original_window.area.left&&
                other.caption==original_window.caption,"template and sibling retain raw provenance")) return 1;
        }
        for (auto mode:{Provider::Mode::Ambiguous,Provider::Mode::Unsupported,Provider::Mode::Failed,Provider::Mode::Throw,Provider::Mode::Invalid}) {
            provider.mode=mode;
            if (!check(host.open(99,source,entry,inputs,error)&&host.apply_available(host.report().ticket,&provider,error),"failed provider pass accepted as partial report")) return 1;
            const auto expected=mode==Provider::Mode::Ambiguous?Status::Ambiguous:mode==Provider::Mode::Unsupported?Status::Unsupported:Status::Failed;
            if (!check(action(host,Kind::AnimationPixels,0x026e0002).status==expected&&
                host.instance()->layout().runtime_animations.at(45).image.pixels==nullptr,
                "provider errors/dishonest Found stay explicit, no blank pixels installed")) return 1;
        }
        provider.mode=Provider::Mode::Found; provider.reenter=&host; provider.source=&source; provider.entry=entry; provider.inputs=inputs;
        if (!check(host.open(99,source,entry,inputs,error)&&host.apply_available(host.report().ticket,&provider,error)&&provider.denied&&
            action(host,Kind::AnimationPixels,0x026e0002).status==Status::Applied,"provider reentry open/close/apply rejected before lifetime mutation")) return 1;
        provider.reenter=nullptr;
        const auto retired=host.report().ticket;
        if (!check(host.close(error)&&!host.instance()&&host.report().actions.empty()&&!host.apply_available(retired,&provider,error)&&
            host.open(99,source,entry,inputs,error)&&host.report().ticket>retired,"close releases state, later open cannot accept stale ticket")) return 1;
        auto wrong_class=inputs; wrong_class.configuration_choices={{0x026e0002,55}};
        if (!check(host.open(99,source,entry,wrong_class,error)&&host.apply_available(host.report().ticket,&provider,error)&&
            action(host,Kind::StageFrame,0x026e0002).status==Status::Unsupported&&
            action(host,Kind::AnimationPixels,0x026e0002).status==Status::Unsupported,
            "valid regular-window choice NOT silently rewritten to Animated Icon sibling")) return 1;
        auto missing_choice=inputs; missing_choice.configuration_choices.clear();
        if (!check(host.open(99,source,entry,missing_choice,error)&&host.apply_available(host.report().ticket,&provider,error)&&
            action(host,Kind::StageFrame,0x026e0002).status==Status::Ambiguous,
            "unresolved configuration duplicate remains Ambiguous, even though one candidate has known class")) return 1;
        auto suppressed=inputs; suppressed.suppress_stage_text=true;
        if (!check(host.open(99,source,entry,suppressed,error),"observed suppression true")) return 1;
        bool stage_caption_seen=false; for (const auto& a:host.report().actions) if (a.kind==Kind::StageCaption) stage_caption_seen=true;
        if (!check(!stage_caption_seen,"observed suppressed caption is omitted, not an unobserved-service completion")) return 1;
        // Controlled host graph variant makes a static getter available. The source window and
        // raw serialized property disagree deliberately: this tests host action wiring, not a new asset.
        auto getter_source=source;
        bool getter_selected=false;
        for (auto& w:getter_source.windows) {
            if (w.object_index==55||w.object_index==60||w.object_index==90||w.object_index==45||w.object_index==93||
                w.object_index==sibling.root_object()) continue;
            spui::ImageBinding image;
            if (sibling.current_image(w.object_index,image,error)&&same({image.resource.instance,image.resource.type,image.resource.group},provider.atlas.image.resource)) {
                w.control_id=entry.stage+0x03744ff0u; getter_selected=true; break;
            }
        }
        if (!check(getter_selected,"controlled source selected only with actual serialized descriptor for available atlas key")) return 1;
        if (!check(host.open(99,getter_source,entry,inputs,error)&&host.apply_available(host.report().ticket,&provider,error)&&
            action(host,Kind::Phase1,entry.stage+0x03744ff0u,Phase1Op::StageImageSource).status==Status::Observed,
            "controlled getter descriptor observed separately from decoded asset result")) return 1;
        bool transferred=false; for (const auto& a:host.report().actions) if (a.kind==Kind::Phase1&&
            a.request.operation==Phase1Op::StageImageTarget&&a.status==Status::Applied) transferred=true;
        if (!check(transferred,"controlled available getter drives StageImageTarget through existing image executor")) return 1;
        auto probe_false=entry; probe_false.save_status=SaveStatus::ProbeFalseZeroHeader;
        if (!check(host.open(99,source,probe_false,inputs,error)&&host.apply_available(host.report().ticket,&provider,error),
            "explicit false-probe/zero-header configuration pass")) return 1;
        bool flags_applied=true; unsigned flag_count=0;
        for (const auto& a:host.report().actions) if (a.kind==Kind::ConfigurationFlag) { ++flag_count; flags_applied=flags_applied&&a.status==Status::Applied; }
        if (!check(flags_applied&&flag_count==4&&(window(*host.instance(),94).flags&1u)&&(window(*host.instance(),47).flags&1u)&&
            !(window(*host.instance(),114).flags&2u)&&!(window(*host.instance(),125).flags&1u),
            "four raw configuration flag changes applied to real windows, not renamed layer semantics")) return 1;
        // Focused owned-instance primitive boundaries used by the lifecycle.
        StarUiInstance primitive;
        if (!check(primitive.instantiate(7,source,entry,error),"primitive instance")) return 1;
        const u16 target=primitive.control_objects(0x046ea550)[0];
        if (!check(primitive.set_caption_reference(target,{1,2},error)&&primitive.set_caption_text(target,u"",error)&&
            window(primitive,target).caption.empty()&&!window(primitive,target).caption_is_ref,"observed empty name clears old ref, not Unknown")) return 1;
        if (!check(primitive.set_caption_text(target,u"keep",error)&&!primitive.set_caption_text(target,std::u16string{char16_t(0xd800)},error)&&
            window(primitive,target).caption=="keep","unpaired high surrogate transactional")) return 1;
        if (!check(!primitive.set_caption_text(target,std::u16string{char16_t(0xdc00)},error)&&
            !primitive.set_caption_text(target,std::u16string{u'a',0,u'b'},error)&&window(primitive,target).caption=="keep","low surrogate/NUL fail without corrupting caption")) return 1;
        if (!check(!primitive.set_caption_text(target,std::u16string(4097,u'x'),error)&&!primitive.set_caption_text(0xffff,u"x",error),"text/target bounds")) return 1;
        const auto root=window(primitive,primitive.root_object()).area; const auto child=window(primitive,176).area;
        if (!check(primitive.move_root({13.5f,7.5f,true},error)&&primitive.center_in_root(176,error)&&
            window(primitive,176).area.left==std::floor((root.w()-child.w())*.5f)&&
            window(primitive,176).area.top==std::floor((root.h()-child.h())*.5f),"centering floors LOCAL coordinate in zero-origin root extents, independent of moved root origin")) return 1;
        if (!check(!primitive.center_in_root(primitive.root_object(),error)&&!primitive.center_in_root(0xffff,error),"centering root/missing target rejected")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both real templates tested")) return 1;
    std::cout<<checks<<" host lifecycle/action/retry/ownership/UTF/centering checks PASS; actual PNG pixels, controlled metadata; no native mount or live GGE caller\n";
    return 0;
}
