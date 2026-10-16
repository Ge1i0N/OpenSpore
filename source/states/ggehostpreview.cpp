#include "states/ggehostpreview.h"
#include "engine/fs.h"
#include "engine/gge_image_provider.h"
#include "engine/gge_catalog_snapshot.h"
#include "engine/gge_star_scene.h"
#include "engine/input.h"
#include "engine/log.h"
#include "engine/console.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/uipresent.h"
#include <array>
#include <optional>
#include <memory>

namespace os::states {
#ifdef OS_ENABLE_DEV_COMMANDS
namespace {
constexpr os::gge::ImageKey kPreviewImage{0xb89d2cd2u,0x2f7d0004u,0x31a44893u};
struct PreviewState { std::unique_ptr<os::gge::StarUiScene> scene; std::unique_ptr<os::gge::StreamingImageProvider> images; u64 ticket=0; std::string status="starting",source="built-in controlled entries"; std::optional<size_t> selected,hovered; bool open=false; };
PreviewState g_preview;
os::gge::StarLifecycleInputs inputs() {
    os::gge::StarLifecycleInputs in; in.phase1.thumbnail=os::gge::ImageKey{}; in.phase1.has_profile=false;
    in.phase1.selected_star_saved=false; in.phase1.selected_star_present=false;
    in.phase1.choices={{0x046a9ca7u,90},{0x026e0002u,55},{0x046ea523u,60}}; in.suppress_stage_text=false;
    in.configuration_choices={{0x026e0002u,45}}; return in;
}
os::gge::SceneCameraObservation camera() {
    auto& d=os::render::device(); os::gge::SceneCameraObservation c; c.source=os::gge::SceneCameraSource::ActiveProjection; c.token=1;
    c.projection.clip_from_world=std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    c.projection.window_rect=os::spui::Rect{0,0,(float)d.width(),(float)d.height()}; return c;
}
bool build(const os::gge::CatalogInput& catalog,const std::filesystem::path& package,std::unique_ptr<os::gge::StarUiScene>& out_scene,std::unique_ptr<os::gge::StreamingImageProvider>& out_images,std::string& error) {
    const auto* layout=os::spui::instance().find(os::gge::kStarUiLayout); if(!layout){error="real GGE layout unavailable";return false;}
    auto images=std::make_unique<os::gge::StreamingImageProvider>(); if(!images->open(package.string(),error))return false;
    std::vector<os::gge::StarSceneCardInput> cards(catalog.saved.size()+catalog.candidates.size()); for(size_t n=0;n<cards.size();++n){cards[n].host_id=0x5000+n;cards[n].lifecycle=inputs();}
    auto scene=std::make_unique<os::gge::StarUiScene>(); if(!scene->open(catalog,*layout,cards,error))return false;const auto ticket=scene->ticket();
    if(!scene->apply_available(ticket,images.get(),error))return false;
    os::gge::StarTransitionInputs transition;transition.requested_case=2;transition.observed_current_case=1;transition.choices={{0x046a9ca7u,90}};
    for(size_t n=0;n<cards.size();++n)if(!scene->apply_phase_transition(ticket,n,transition,error))return false;
    if(!scene->update(ticket,camera(),error))return false;out_scene=std::move(scene);out_images=std::move(images);return true;
}
void enter(){std::string old;if(g_preview.scene)g_preview.scene->close(old);if(g_preview.images)g_preview.images->close();g_preview.ticket=0;g_preview.status="starting";g_preview.source="built-in controlled entries";g_preview.selected.reset();g_preview.hovered.reset();g_preview.open=false;std::string e;os::gge::CatalogInput catalog;catalog.saved_status=catalog.candidate_status=os::gge::CatalogAvailability::Available;for(int n=0;n<3;++n){os::gge::StarMetadata s;s.star_id=0x7000+n;s.position={-.62f+.62f*n,0,0};s.name=u"Controlled host star";s.terrain_loaded=true;s.terrain_image=kPreviewImage.instance;catalog.candidates.push_back(std::move(s));}auto p=os::fs::resolve("Data/Spore_Graphics.package");std::unique_ptr<os::gge::StarUiScene> scene;std::unique_ptr<os::gge::StreamingImageProvider> images;if(p.empty()||!build(catalog,p,scene,images,e)){g_preview.status="blocked: "+(p.empty()?std::string("selected Graphics package unavailable"):e);LOG_WARN("gge-preview","%s",g_preview.status.c_str());return;}g_preview.scene=std::move(scene);g_preview.images=std::move(images);g_preview.ticket=g_preview.scene->ticket();g_preview.status="controlled catalogue / real atlas / explicit camera";g_preview.open=true;LOG_INFO("gge-preview","GGEHostPreview active: CONTROLLED, not native GGE");}
void exit(){if(g_preview.scene){std::string e;g_preview.scene->close(e);}if(g_preview.images)g_preview.images->close();g_preview.scene.reset();g_preview.images.reset();g_preview.open=false;LOG_INFO("gge-preview","GGEHostPreview closed");}
std::string command(const os::console::CmdContext& c) {
    if(c.args.size()<2||c.args[1]=="status") {
        if(!g_preview.scene) return g_preview.status+" cards=0";
        const auto& p=g_preview.scene->producer_observations();
        auto label=[](os::gge::CatalogProducerStatus s){return s==os::gge::CatalogProducerStatus::Available?"Available":s==os::gge::CatalogProducerStatus::Failed?"Failed":"Unknown";};
        const auto append=[](os::gge::CatalogRefillOutcome s){return s==os::gge::CatalogRefillOutcome::Appended?"Appended":s==os::gge::CatalogRefillOutcome::NotAppended?"NotAppended":"Unknown";};
        return g_preview.status+" source="+g_preview.scene->catalog_source()+" cards="+std::to_string(g_preview.scene->size())+
            " producer="+label(p.manager)+","+label(p.starsdb)+","+label(p.planet_records)+","+label(p.candidate_refill)+
            " resources="+label(p.resource_root)+","+label(p.resource_index)+","+label(p.resource_context)+","+label(p.resource_class)+
            " refill_append="+append(p.refill_append);
    }
    if(c.args[1]!="load"||c.args.size()<3) return "usage: ggepreview:load,<explicit-snapshot-path>";
    os::gge::CatalogInput catalog; std::string error;
    if(!os::gge::load_catalog_snapshot(c.args[2],catalog,error)) return "snapshot blocked: "+error;
    const auto package=os::fs::resolve("Data/Spore_Graphics.package"); if(package.empty()) return "snapshot blocked: Graphics package unavailable";
    std::unique_ptr<os::gge::StarUiScene> scene; std::unique_ptr<os::gge::StreamingImageProvider> images;
    if(!build(catalog,package,scene,images,error)) return "snapshot blocked: "+error;
    if(g_preview.scene){std::string old;g_preview.scene->close(old);}if(g_preview.images)g_preview.images->close();
    g_preview.scene=std::move(scene);g_preview.images=std::move(images);g_preview.ticket=g_preview.scene->ticket();g_preview.selected.reset();g_preview.hovered.reset();g_preview.open=true;g_preview.source="caller snapshot: "+c.args[2];g_preview.status="loaded caller snapshot (controlled host)";
    LOG_INFO("gge-preview","caller catalogue snapshot loaded: %s",c.args[2].c_str()); return g_preview.status;
}
void tick(f32){if(!g_preview.open||!g_preview.ticket)return;const auto serial=g_preview.scene->frame().serial;auto hover=g_preview.scene->pick(serial,(float)os::input::x(),(float)os::input::y());g_preview.hovered=hover?std::optional<size_t>(hover->entry_index):std::nullopt;if(os::input::clicked()){if(hover){g_preview.selected=hover->entry_index;g_preview.status="picked host card "+std::to_string(hover->host_id)+" (host only)";}else g_preview.status="no host window";LOG_INFO("gge-preview","%s at %d,%d",g_preview.status.c_str(),os::input::x(),os::input::y());}std::string e;if(!g_preview.scene->update(g_preview.ticket,camera(),e))g_preview.status="camera blocked: "+e;}
void render(){auto& d=os::render::device();auto& o=os::render::overlay();if(!d.d3d()||!d.begin_frame())return;d.clear(0xff080d16);o.begin(d.width(),d.height());const auto serial=g_preview.scene->frame().serial;int n=os::states::draw_gge_star_scene(*g_preview.scene,serial);const auto border=[&](size_t index,u32 color,const char* label){if(index>=g_preview.scene->size())return;const auto* card=g_preview.scene->card(index);if(!card||!card->instance())return;for(const auto& w:card->instance()->layout().windows)if(w.object_index==card->instance()->root_object()){const auto& r=w.placed_area;o.rect(r.left,r.top,r.w(),2,color);o.rect(r.left,r.bottom-2,r.w(),2,color);o.rect(r.left,r.top,2,r.h(),color);o.rect(r.right-2,r.top,2,r.h(),color);o.text_fmt(r.left+4,r.top+4,color,"%s host=%llu entry=%zu",label,(unsigned long long)card->instance()->id(),index);break;}};if(g_preview.hovered)border(*g_preview.hovered,0xff55d7ff,"hover");if(g_preview.selected)border(*g_preview.selected,0xffffd34e,"selected");const auto& p=g_preview.scene->producer_observations();o.text_fmt(18,18,0xffffe3a0,"GGE HOST PREVIEW (CONTROLLED) - cards=%zu",g_preview.scene->size());o.text_fmt(18,40,0xffb6c9df,"%s",g_preview.status.c_str());o.text_fmt(18,62,0xff8197ae,"source: %s | catalog: %s",g_preview.source.c_str(),g_preview.scene->catalog_source().empty()?"built-in/controlled":g_preview.scene->catalog_source().c_str());o.text_fmt(18,84,0xff8197ae,"producer M/S/P/R: %d/%d/%d/%d | resources R/I/C/K: %d/%d/%d/%d | append=%d | drawn=%d",(int)p.manager,(int)p.starsdb,(int)p.planet_records,(int)p.candidate_refill,(int)p.resource_root,(int)p.resource_index,(int)p.resource_context,(int)p.resource_class,(int)p.refill_append,n);o.end();d.end_frame();}
}
#endif
void install_gge_host_preview(Manager& manager){
#ifdef OS_ENABLE_DEV_COMMANDS
    State state;state.name="GGEHostPreview";state.on_enter=enter;state.on_exit=exit;state.on_tick=tick;state.on_render=render; if(manager.reg(std::move(state)))LOG_INFO("states","registered dev-only GGEHostPreview");
    os::console::reg({"ggepreview","Controlled host catalogue preview: status or load an explicit snapshot","status^load^<path>",command});
#else
    (void)manager;
#endif
}
} // namespace os::states
