#pragma once
#include "engine/gge_star_present.h"

namespace os::gge {
// Owned host window-graph instance, NOT native cSPUILayeredObject / original mount completion.
// IDs are caller-issued host identities, separate from record indices, star IDs and ControlID.
class StarUiInstance {
public:
    StarUiInstance() = default;
    StarUiInstance(const StarUiInstance&) = delete;
    StarUiInstance& operator=(const StarUiInstance&) = delete;
    bool instantiate(u64 instance_id,const spui::Layout& source,const GameEntry& entry,std::string& error);
    void clear();
    bool active() const { return id_!=0; }
    u64 id() const { return id_; }
    const GameEntry& entry() const { return entry_; }
    const spui::Layout& layout() const { return layout_; }
    u16 root_object() const;
    // All matches retained, including hidden controls. Native dynamic lookup order is not guessed.
    std::vector<u16> control_objects(u32 control) const;
    // Explicit serialized object identity within THIS instance. No ambiguous ControlID resolution.
    // Only visible/enabled/IgnoreMouse/Clip masks are supported by the current host geometry model.
    bool set_window_flag(u16 object,u32 mask,bool value,std::string& error);
    bool set_caption_reference(u16 object,const StarLocaleCaption& caption,std::string& error);
    bool image_target(u16 window,u16& drawable,std::string& error) const;
    bool current_image(u16 window,spui::ImageBinding& out,std::string& error) const;
    // CF3C4EAE only; override keyed by drawable identity, so windows sharing it share the image
    // within this instance, as the native setter does. Template/sibling instance remain untouched.
    bool set_image(u16 window,const spui::ImageBinding& image,std::string& error);
    bool move_root(const StarUiPlacement& placement,std::string& error);
    // Pixel-space layer, no design-resolution rescaling after moving the root. No CommandID dispatch.
    // Returned window pointer is invalidated by successful mutation, instantiate or clear.
    const spui::Window* pick(float x,float y,const spui::Rect& viewport) const;
    // Stage image-list plugins, original listeners/layering and phase-1 service calls not executed.
    bool native_mount_complete() const { return false; }
private:
    std::optional<size_t> find_object(u16 object) const;
    u64 id_=0;
    size_t root_=0;
    spui::Layout layout_;
    GameEntry entry_;
};
}  // namespace os::gge
