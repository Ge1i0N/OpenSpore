#include "engine/spui_drawable.h"
#include <cmath>
#include <cstring>
#include <utility>

namespace os::spui {
namespace {
const Object* object_at(const Layout& layout, u16 index) {
    for (const auto& o : layout.objects) if (o.index == index) return &o;
    return nullptr;
}

const Property* property(const Object& object, u32 id) {
    for (const auto& p : object.properties) if (p.depth == 0 && p.id == id) return &p;
    return nullptr;
}

template<class T> bool value(const Object& o, u32 id, u16 code, T& out) {
    const auto* p = property(o, id);
    if (!p) return true; // keep constructor default
    if (p->code != code || p->count != 1 || p->raw.size() != sizeof(T)) return false;
    std::memcpy(&out, p->raw.data(), sizeof(T));
    return true;
}

bool finite(const Rect& r) {
    return std::isfinite(r.left) && std::isfinite(r.top) &&
           std::isfinite(r.right) && std::isfinite(r.bottom) &&
           std::isfinite(r.w()) && std::isfinite(r.h()) && r.valid();
}

float align(float start, float end, float extent, u32 alignment) {
    if (alignment == 2) return end - extent;
    if (alignment == 3) return start + (end - start - extent) * .5f;
    return start;
}
}  // namespace

bool valid_image_binding(const ImageBinding& image) {
    if (!finite(image.uv)||image.logical_width<0||image.logical_height<0) return false;
    if (!image.pixels) return true; // resource descriptor, not proof of decoded availability
    const auto& p=*image.pixels;
    return p.width&&p.height&&p.width<=4096&&p.height<=4096&&
        p.bgra.size()==size_t(p.width)*p.height*4&&p.resource.instance==image.resource.instance&&
        p.resource.type==image.resource.type&&p.resource.group==image.resource.group;
}
bool image_drawable_binding(const Layout& layout,u16 drawable,ImageBinding& out) {
    const auto* d=object_at(layout,drawable);
    if (!d||d->class_id!=0xcf3c4eaeu) return false;
    if (const auto it=layout.runtime_images.find(drawable);it!=layout.runtime_images.end()) {
        if (!valid_image_binding(it->second)) return false;
        out=it->second; return true;
    }
    const auto* link=property(*d,0x4f3c4a26u);
    if (!link||link->code!=19||link->refs.size()!=1) return false;
    const auto* image=object_at(layout,link->refs[0]);
    if (!image||image->class_id!=0x01be6b15u) return false;
    const auto* external=property(*image,0x01be0001u);
    if (!external||external->code!=19||external->refs.size()!=1||external->refs[0]>=layout.external_resources.size()) return false;
    ImageBinding result; result.resource=layout.external_resources[external->refs[0]];
    struct Size { i32 width=0,height=0; } size;
    if (!value(*image,0x01be0002u,16,result.uv)||!value(*image,0x01be0003u,15,size)) return false;
    result.logical_width=size.width; result.logical_height=size.height;
    if (!valid_image_binding(result)) return false;
    out=std::move(result); return true;
}

bool drawable_quad(const Layout& layout, u16 drawable, const Rect& area, u32 state, ImageQuad& out) {
    if (!finite(area)) return false;
    const auto* d = object_at(layout, drawable);
    if (!d) return false;
    const bool plain = d->class_id == 0xcf3c4eaeu;
    const bool standard = d->class_id == 0xaf3a9e26u;
    const bool radio = d->class_id == 0x2f3adc5du;
    if (!plain && !standard && !radio) return false;
    ImageQuad quad;
    quad.area = area;
    quad.uv = {0, 0, 1, 1};
    struct Size { int width = 0, height = 0; } size;
    if (plain && layout.runtime_images.count(drawable)) {
        ImageBinding image;
        if (!image_drawable_binding(layout,drawable,image)) return false;
        quad.pixels=image.pixels; quad.uv=image.uv;
        size.width=image.logical_width; size.height=image.logical_height;
        // Runtime overrides must be decoded; never fall back to the template/global first-key index.
        if (!quad.pixels) return false;
    } else {
        const auto* link=property(*d,plain?0x4f3c4a26u:0x0f3ac75eu);
        if (!link||link->code!=19||link->refs.size()!=1) return false;
        const auto* image=object_at(layout,link->refs[0]);
        if (!image||image->class_id!=0x01be6b15u) return false;
        const auto* external=property(*image,0x01be0001u);
        if (!external||external->code!=19||external->refs.size()!=1||external->refs[0]>=layout.external_resources.size()) return false;
        quad.external=external->refs[0];
        if (!value(*image,0x01be0002u,16,quad.uv)||!finite(quad.uv)||!value(*image,0x01be0003u,15,size)) return false;
    }
    if (plain) {
        u32 flags = 0, horizontal = 3, vertical = 3, tiling = 0;
        float scale = 1.f;
        if (!value(*d, 0x4f3c4a27u, 9, flags) || !value(*d, 0x4f3c4a29u, 9, horizontal) ||
            !value(*d, 0x4f3c4a28u, 9, vertical) || !value(*d, 0x4f3c4a2au, 9, tiling) ||
            !value(*d, 0x4f3c4a2bu, 11, scale) || tiling != 0 ||
            !std::isfinite(scale) || scale <= 0.f) return false;
        // GA 0x97d24b..0x97d2e8: scale affects only fixed dimensions in the None branch.
        if (flags & 1) {
            const float width = (float)size.width * scale;
            quad.area.left = align(area.left, area.right, width, horizontal);
            quad.area.right = quad.area.left + width;
        }
        if (flags & 2) {
            const float height = (float)size.height * scale;
            quad.area.top = align(area.top, area.bottom, height, vertical);
            quad.area.bottom = quad.area.top + height;
        }
    } else {
        u8 tileable = 0;
        if (!value(*d, 0xef3c000au, 2, tileable) || tileable) return false;
        const u32 base = state & 7;
        u32 frame = 0;
        const bool selected = (state & 8) != 0;
        // GA 0x966a40 / 0x966ad0, including their default for out-of-range base states.
        if (standard) {
            if (base == 0) frame = selected ? 3u : 1u;
            else if (base == 2) frame = 2;
            else if (base == 3) frame = 3;
        } else if (base <= 3) {
            frame = (base >= 2 ? base - 1 : 0) + (selected ? 3u : 0u);
        }
        const float step = quad.uv.w() / (standard ? 4.f : 6.f);
        quad.uv.left += step * (float)frame;
        quad.uv.right = quad.uv.left + step;
    }
    if (!finite(quad.area)) return false;
    out = quad;
    return true;
}
}  // namespace os::spui
