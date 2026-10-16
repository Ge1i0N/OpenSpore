#include "states/ggehostpreview.h"
#include "engine/fs.h"
#include "engine/gge_image_provider.h"
#include "engine/gge_star_scene.h"
#include "engine/input.h"
#include "engine/log.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/uipresent.h"
#include <array>
#include <optional>

namespace os::states {
#ifdef OS_ENABLE_DEV_COMMANDS
namespace {
constexpr os::gge::ImageKey kPreviewImage{0xb89d2cd2u,0x2f7d0004u,0x31a44893u};
struct PreviewState { os::gge::StarUiScene scene; os::gge::StreamingImageProvider images; u64 ticket=0; std::string status="starting"; std::optional<size_t> selected,hovered; bool open=false; };
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
bool build(std::string& error) {
    const auto* layout=os::spui::instance().find(os::gge::kStarUiLayout); if(!layout){error="real GGE layout unavailable";return false;}
    const auto package=os::fs::resolve("Data/Spore_Graphics.package"); if(package.empty()){error="selected Graphics package unavailable";return false;}
    if(!g_preview.images.open(package.string(),error))return false;
    os::gge::CatalogInput catalog; catalog.saved_status=catalog.candidate_status=os::gge::CatalogAvailability::Available;
    for(int n=0;n<3;++n){os::gge::StarMetadata s;s.star_id=0x7000+n;s.position={-.62f+.62f*n,0,0};s.name=u"Controlled host star";s.terrain_loaded=true;s.terrain_image=kPreviewImage.instance;catalog.candidates.push_back(std::move(s));}
    std::vector<os::gge::StarSceneCardInput> cards(3); for(size_t n=0;n<cards.size();++n){cards[n].host_id=0x5000+n;cards[n].lifecycle=inputs();}
    if(!g_preview.scene.open(catalog,*layout,cards,error))return false;g_preview.ticket=g_preview.scene.ticket();
    if(!g_preview.scene.apply_available(g_preview.ticket,&g_preview.images,error))return false;
    os::gge::StarTransitionInputs transition;transition.requested_case=2;transition.observed_current_case=1;transition.choices={{0x046a9ca7u,90}};
    for(size_t n=0;n<cards.size();++n)if(!g_preview.scene.apply_phase_transition(g_preview.ticket,n,transition,error))return false;
    if(!g_preview.scene.update(g_preview.ticket,camera(),error))return false;g_preview.status="controlled catalogue / real atlas / explicit camera";g_preview.open=true;return true;
}
void enter(){std::string old;g_preview.scene.close(old);g_preview.images.close();g_preview.ticket=0;g_preview.status="starting";g_preview.selected.reset();g_preview.hovered.reset();g_preview.open=false;std::string e;if(!build(e)){g_preview.status="blocked: "+e;LOG_WARN("gge-preview","%s",e.c_str());return;}LOG_INFO("gge-preview","GGEHostPreview active: CONTROLLED, not native GGE");}
void exit(){std::string e;g_preview.scene.close(e);g_preview.images.close();g_preview.open=false;LOG_INFO("gge-preview","GGEHostPreview closed");}
void tick(f32){if(!g_preview.open||!g_preview.ticket)return;const auto serial=g_preview.scene.frame().serial;auto hover=g_preview.scene.pick(serial,(float)os::input::x(),(float)os::input::y());g_preview.hovered=hover?std::optional<size_t>(hover->entry_index):std::nullopt;if(os::input::clicked()){if(hover){g_preview.selected=hover->entry_index;g_preview.status="picked host card "+std::to_string(hover->host_id)+" (host only)";} else g_preview.status="no host window";LOG_INFO("gge-preview","%s at %d,%d",g_preview.status.c_str(),os::input::x(),os::input::y());}std::string e;if(!g_preview.scene.update(g_preview.ticket,camera(),e))g_preview.status="camera blocked: "+e;}
void render(){auto& d=os::render::device();auto& o=os::render::overlay();if(!d.d3d()||!d.begin_frame())return;d.clear(0xff080d16);o.begin(d.width(),d.height());const auto serial=g_preview.scene.frame().serial;int n=os::states::draw_gge_star_scene(g_preview.scene,serial);const auto border=[&](size_t index,u32 color,const char* label){if(index>=g_preview.scene.size())return;const auto* card=g_preview.scene.card(index);if(!card||!card->instance())return;for(const auto& w:card->instance()->layout().windows)if(w.object_index==card->instance()->root_object()){const auto& r=w.placed_area;o.rect(r.left,r.top,r.w(),2,color);o.rect(r.left,r.bottom-2,r.w(),2,color);o.rect(r.left,r.top,2,r.h(),color);o.rect(r.right-2,r.top,2,r.h(),color);o.text_fmt(r.left+4,r.top+4,color,"%s host=%llu entry=%zu",label,(unsigned long long)card->instance()->id(),index);break;}};if(g_preview.hovered)border(*g_preview.hovered,0xff55d7ff,"hover");if(g_preview.selected)border(*g_preview.selected,0xffffd34e,"selected");o.text_fmt(18,18,0xffffe3a0,"GGE HOST PREVIEW (CONTROLLED) - cards=%zu",g_preview.scene.size());o.text_fmt(18,40,0xffb6c9df,"%s",g_preview.status.c_str());o.text_fmt(18,62,0xff8197ae,"real layout + shipped PNG; no native catalogue/camera/callback");o.text_fmt(18,84,0xff8197ae,"drawn windows=%d | cyan hover / yellow host selection",n);o.end();d.end_frame();}
}
#endif
void install_gge_host_preview(Manager& manager){
#ifdef OS_ENABLE_DEV_COMMANDS
    State state;state.name="GGEHostPreview";state.on_enter=enter;state.on_exit=exit;state.on_tick=tick;state.on_render=render; if(manager.reg(std::move(state)))LOG_INFO("states","registered dev-only GGEHostPreview");
#else
    (void)manager;
#endif
}
} // namespace os::states
