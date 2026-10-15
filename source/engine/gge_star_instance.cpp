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
    next.runtime_animations.clear();
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
bool StarUiInstance::set_caption_text(u16 object,const std::u16string& text,std::string& error) {
    const auto n=find_object(object);
    if (!n||text.size()>4096) { error="star caption target/text limit invalid"; return false; }
    std::string utf8;
    for (size_t i=0;i<text.size();++i) {
        u32 c=text[i];
        if (c>=0xd800&&c<=0xdbff) {
            if (i+1>=text.size()||text[i+1]<0xdc00||text[i+1]>0xdfff) { error="unpaired UTF-16 caption surrogate"; return false; }
            c=0x10000+((c-0xd800)<<10)+(text[++i]-0xdc00);
        } else if (c>=0xdc00&&c<=0xdfff) { error="unpaired UTF-16 caption surrogate"; return false; }
        if (!c) { error="embedded NUL in caption unsupported by host painter"; return false; }
        if (c<0x80) utf8+=char(c);
        else if (c<0x800) { utf8+=char(0xc0|(c>>6)); utf8+=char(0x80|(c&63)); }
        else if (c<0x10000) { utf8+=char(0xe0|(c>>12)); utf8+=char(0x80|((c>>6)&63)); utf8+=char(0x80|(c&63)); }
        else { utf8+=char(0xf0|(c>>18)); utf8+=char(0x80|((c>>12)&63)); utf8+=char(0x80|((c>>6)&63)); utf8+=char(0x80|(c&63)); }
    }
    auto& w=layout_.windows[*n]; w.caption=std::move(utf8); w.caption_raw.clear();
    w.caption_table=w.caption_key=0; w.caption_is_ref=w.caption_key_seen=false; error.clear(); return true;
}
bool StarUiInstance::center_in_root(u16 object,std::string& error) {
    const auto n=find_object(object);
    if (!n||*n==root_||!layout_.windows[*n].has_area||!layout_.windows[*n].area.valid()) {
        error="star centering target/area unavailable"; return false;
    }
    auto windows=layout_.windows; auto& w=windows[*n];
    const float width=w.area.w(),height=w.area.h();
    const float x=std::floor((windows[root_].area.w()-width)*.5f);
    const float y=std::floor((windows[root_].area.h()-height)*.5f);
    w.area={x,y,x+width,y+height}; // local SetLocation semantics; not an absolute-parent transform
    if (!w.area.valid()) { error="star centering collapses geometry"; return false; }
    if (!refresh(windows,root_,error)) return false;
    layout_.windows=std::move(windows); error.clear(); return true;
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
bool StarUiInstance::copy_local_rect(u16 target,u16 source,std::string& error) {
    const auto to=find_object(target),from=find_object(source);
    if (!to||!from||*to==root_||!layout_.windows[*to].has_area||!layout_.windows[*from].has_area||
        !finite(layout_.windows[*from].area)||!layout_.windows[*from].area.valid()) {
        error="window rectangle-copy target/source/geometry unsupported"; return false;
    }
    auto windows=layout_.windows; windows[*to].area=windows[*from].area;
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
bool StarUiInstance::animation(u16 window,spui::AnimationDescriptor& out,std::string& error) const {
    const auto n=find_object(window);
    if (!n||layout_.windows[*n].class_id!=0x0106f14bu) { error="Animated Icon window unavailable/unsupported"; return false; }
    return spui::animation_descriptor(layout_,window,out,error);
}
bool StarUiInstance::animation_frame(u16 window,i32& out,std::string& error) const {
    spui::AnimationDescriptor descriptor;
    if (!animation(window,descriptor,error)) return false;
    const auto it=layout_.runtime_animations.find(window);
    out=it==layout_.runtime_animations.end()?1:it->second.frame;
    error.clear(); return true;
}
bool StarUiInstance::set_animation_frame(u16 window,i32 frame,FrameChange& result,std::string& error) {
    spui::AnimationDescriptor descriptor;
    if (!animation(window,descriptor,error)) return false;
    if (frame<0||frame>=descriptor.frames) { result=FrameChange::IgnoredOutOfRange; error.clear(); return true; }
    const auto it=layout_.runtime_animations.find(window);
    const i32 old=it==layout_.runtime_animations.end()?1:it->second.frame;
    if (frame==old) { result=FrameChange::Unchanged; error.clear(); return true; }
    if (it==layout_.runtime_animations.end()) {
        spui::AnimationBinding binding; binding.image=descriptor.image; binding.frame=frame;
        layout_.runtime_animations.emplace(window,std::move(binding));
    } else it->second.frame=frame;
    result=FrameChange::Changed; error.clear(); return true;
}
bool StarUiInstance::set_animation_pixels(u16 window,std::shared_ptr<const spui::ImagePixels> pixels,std::string& error) {
    spui::AnimationDescriptor descriptor;
    if (!animation(window,descriptor,error)) return false;
    descriptor.image.pixels=std::move(pixels);
    if (!descriptor.image.pixels||!spui::valid_image_binding(descriptor.image)) {
        error="Animated Icon requires decoded pixels matching its exact serialized resource key"; return false;
    }
    const auto it=layout_.runtime_animations.find(window);
    const i32 frame=it==layout_.runtime_animations.end()?1:it->second.frame;
    layout_.runtime_animations.insert_or_assign(window,spui::AnimationBinding{std::move(descriptor.image),frame});
    error.clear(); return true;
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
