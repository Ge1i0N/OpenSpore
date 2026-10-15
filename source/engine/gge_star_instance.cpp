#include "engine/gge_star_instance.h"
#include "engine/spui_drawable.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>
#include <utility>

namespace os::gge {
namespace {
bool finite(const spui::Rect& area) {
    return std::isfinite(area.left)&&std::isfinite(area.top)&&std::isfinite(area.right)&&std::isfinite(area.bottom);
}
// Validate a bounded single-root tree BEFORE paint_order/picking can traverse it. Parent/Children
// must agree; geometry/flags use the same host policies as the sequential SPUI reader.
bool refresh(std::vector<spui::Window>& windows,size_t root,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (windows.empty()||windows.size()>4096||root>=windows.size()||windows[root].parent!=-1)
        return fail("star instance requires a bounded single-root window graph");
    std::unordered_set<u16> objects;
    for (const auto& w:windows) if (w.object_index==0xffff||!objects.insert(w.object_index).second)
        return fail("duplicate serialized window object index");
    std::vector<bool> seen(windows.size(),false);
    std::vector<size_t> pending{root};
    size_t visited=0;
    while (!pending.empty()) {
        const size_t n=pending.back(); pending.pop_back();
        if (n>=windows.size()||seen[n]) return fail("cyclic/repeated window child link");
        seen[n]=true; ++visited;
        auto& w=windows[n];
        if (!finite(w.area)||!std::isfinite(w.area.w())||!std::isfinite(w.area.h())) return fail("nonfinite local window geometry");
        w.placed_area=w.area; w.visible=(w.flags&1u)!=0; w.enabled=(w.flags&2u)!=0;
        if (w.parent>=0) {
            if ((size_t)w.parent>=windows.size()||!seen[(size_t)w.parent]) return fail("window parent order/link mismatch");
            const auto& p=windows[(size_t)w.parent];
            w.placed_area.left+=p.placed_area.left; w.placed_area.right+=p.placed_area.left;
            w.placed_area.top+=p.placed_area.top; w.placed_area.bottom+=p.placed_area.top;
            w.visible=w.visible&&p.visible; w.enabled=w.enabled&&p.enabled;
        }
        if (!finite(w.placed_area)) return fail("nonfinite inherited window geometry");
        for (auto it=w.children.rbegin();it!=w.children.rend();++it) {
            if (*it>=windows.size()||windows[*it].parent!=(int)n) return fail("child/parent graph mismatch");
            pending.push_back(*it);
        }
    }
    if (visited!=windows.size()) return fail("unreachable star layout windows");
    return true;
}
}
bool StarUiInstance::instantiate(u64 instance_id,const spui::Layout& source,const GameEntry& entry,std::string& error) {
    if (!instance_id||source.instance!=kStarUiLayout||!source.sequential||source.roots.size()!=1||
        source.objects.size()>65536||source.windows.empty()||source.windows.size()>4096||
        entry.name.size()>4096||entry.save_stem.size()>4096) {
        error="invalid star host identity/template or host graph/text limits"; return false;
    }
    // Full ownership only after validation; source assets retain their original serialized identity.
    spui::Layout next=source;
    next.runtime_images.clear(); // only raw template provenance seeds a new host lifetime
    const size_t root=source.roots[0];
    if (!refresh(next.windows,root,error)) return false;
    if (next.windows[root].control_id!=kStarUiRoot||!next.windows[root].has_area||!next.windows[root].area.valid()) {
        error="star layout positioning root missing/invalid"; return false;
    }
    GameEntry next_entry=entry;
    layout_=std::move(next); entry_=std::move(next_entry); root_=root; id_=instance_id; error.clear(); return true;
}
void StarUiInstance::clear() { layout_={}; entry_={}; id_=0; root_=0; }
u16 StarUiInstance::root_object() const { return active()?layout_.windows[root_].object_index:0xffff; }
std::optional<size_t> StarUiInstance::find_object(u16 object) const {
    if (!active()) return std::nullopt;
    for (size_t i=0;i<layout_.windows.size();++i) if (layout_.windows[i].object_index==object) return i;
    return std::nullopt;
}
std::vector<u16> StarUiInstance::control_objects(u32 control) const {
    std::vector<u16> matches;
    if (active()) for (const auto& w:layout_.windows) if (w.control_id==control) matches.push_back(w.object_index);
    return matches;
}
bool StarUiInstance::set_window_flag(u16 object,u32 mask,bool value,std::string& error) {
    const auto n=find_object(object);
    if (!n||!mask||(mask&~0x413u)) { error="unsupported star window target/flag mask"; return false; }
    auto windows=layout_.windows;
    if (value) windows[*n].flags|=mask; else windows[*n].flags&=~mask;
    if (!refresh(windows,root_,error)) return false;
    layout_.windows=std::move(windows); error.clear(); return true;
}
bool StarUiInstance::set_caption_reference(u16 object,const StarLocaleCaption& caption,std::string& error) {
    const auto n=find_object(object);
    if (!n) { error="star caption object not found"; return false; }
    auto& w=layout_.windows[*n];
    w.caption.clear(); w.caption_raw.clear(); w.caption_table=caption.table; w.caption_key=caption.key;
    w.caption_is_ref=true; w.caption_key_seen=true; error.clear(); return true;
}
bool StarUiInstance::move_root(const StarUiPlacement& placement,std::string& error) {
    if (!active()||!std::isfinite(placement.x)||!std::isfinite(placement.y)) {
        error="star root move unavailable/invalid"; return false;
    }
    if (!placement.move_required) { error.clear(); return true; }
    auto windows=layout_.windows;
    auto& root=windows[root_];
    const float width=root.area.w(),height=root.area.h();
    root.area={placement.x,placement.y,placement.x+width,placement.y+height};
    if (!root.area.valid()) { error="star root move collapses window extents"; return false; }
    if (!refresh(windows,root_,error)) return false;
    layout_.windows=std::move(windows); error.clear(); return true;
}
bool StarUiInstance::image_target(u16 window,u16& drawable,std::string& error) const {
    const auto n=find_object(window);
    if (!n) { error="image target window unavailable"; return false; }
    const u16 target=layout_.windows[*n].drawable_object;
    for (const auto& object:layout_.objects) if (object.index==target&&object.class_id==0xcf3c4eaeu) {
        drawable=target; error.clear(); return true;
    }
    error="image target has no supported CF3C4EAE fill drawable (Animated Icon is separate)"; return false;
}
bool StarUiInstance::current_image(u16 window,spui::ImageBinding& out,std::string& error) const {
    u16 drawable=0xffff;
    if (!image_target(window,drawable,error)) return false;
    if (!spui::image_drawable_binding(layout_,drawable,out)) { error="drawable image unobserved/absent"; return false; }
    error.clear(); return true;
}
bool StarUiInstance::set_image(u16 window,const spui::ImageBinding& image,std::string& error) {
    u16 drawable=0xffff;
    if (!image_target(window,drawable,error)) return false;
    if (!image.pixels||!spui::valid_image_binding(image)) { error="drawable override requires valid decoded image pixels"; return false; }
    layout_.runtime_images.insert_or_assign(drawable,image); error.clear(); return true;
}
const spui::Window* StarUiInstance::pick(float x,float y,const spui::Rect& viewport) const {
    if (!active()||!finite(viewport)||!std::isfinite(viewport.w())||!std::isfinite(viewport.h())||!viewport.valid()||
        !std::isfinite(x)||!std::isfinite(y)||!viewport.contains(x,y)) return nullptr;
    const auto order=spui::paint_order(layout_);
    for (auto it=order.rbegin();it!=order.rend();++it) {
        const auto& w=layout_.windows[*it];
        if (!w.has_area||!w.enabled||(w.flags&0x10u)||!w.placed_area.contains(x,y)) continue;
        if (spui::inherited_clip(layout_,*it,viewport).contains(x,y)) return &w;
    }
    return nullptr;
}
}  // namespace os::gge
