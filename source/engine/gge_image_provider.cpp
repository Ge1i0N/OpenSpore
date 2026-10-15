#include "engine/gge_image_provider.h"
#include "engine/spui_drawable.h"
#include "render/imgfile.h"
#include <cstring>
#include <utility>

namespace os::gge {
namespace {
u32 be32(const std::vector<u8>& b,size_t at) { return (u32(b[at])<<24)|(u32(b[at+1])<<16)|(u32(b[at+2])<<8)|b[at+3]; }
ImageApplyStatus status(ImageReadStatus s) {
    switch (s) {
    case ImageReadStatus::Found: return ImageApplyStatus::Applied;
    case ImageReadStatus::Missing: return ImageApplyStatus::Missing;
    case ImageReadStatus::Ambiguous: return ImageApplyStatus::Ambiguous;
    case ImageReadStatus::Unsupported: return ImageApplyStatus::Unsupported;
    case ImageReadStatus::ReadFailure: return ImageApplyStatus::ReadFailure;
    default: return ImageApplyStatus::Blocked;
    }
}
const spui::Window* target(const StarUiInstance& instance,const StarControlBinding& binding) {
    if (binding.status!=StarBindingStatus::Found||!binding.object) return nullptr;
    for (const auto& w:instance.layout().windows) if (w.object_index==*binding.object&&w.control_id==binding.control) return &w;
    return nullptr;
}
}
bool ExplicitImageProvider::open(const std::string& path,std::string& error) {
    label_=path; return reader_.open(path,error);
}
bool ExplicitImageProvider::open_from_memory(const std::vector<u8>& bytes,std::string& error) {
    label_="caller-supplied DBPF bytes"; return reader_.open_from_memory(bytes,error);
}
ImageRead ExplicitImageProvider::read(const ImageKey& key) {
    ImageRead result;
    if (!key.instance||key.type!=0x2f7d0004u) {
        result.status=ImageReadStatus::Unsupported; result.error="provider supports nonzero-instance PNG keys only"; return result;
    }
    auto resource=reader_.read_unique({key.instance,key.type,key.group});
    switch (resource.status) {
    case starsdb::ResourceReadStatus::NotFound: result.status=ImageReadStatus::Missing; break;
    case starsdb::ResourceReadStatus::Ambiguous: result.status=ImageReadStatus::Ambiguous; break;
    case starsdb::ResourceReadStatus::Found: break;
    case starsdb::ResourceReadStatus::PackageRejected: result.status=ImageReadStatus::Unknown; break;
    default: result.status=ImageReadStatus::ReadFailure; break;
    }
    if (resource.status!=starsdb::ResourceReadStatus::Found) { result.error=resource.error; return result; }
    const u8 signature[8]{137,80,78,71,13,10,26,10};
    if (resource.bytes.size()<33||std::memcmp(resource.bytes.data(),signature,8)||be32(resource.bytes,8)!=13||
        std::memcmp(resource.bytes.data()+12,"IHDR",4)) {
        result.status=ImageReadStatus::ReadFailure; result.error="PNG header rejected"; return result;
    }
    const u32 width=be32(resource.bytes,16),height=be32(resource.bytes,20);
    if (!width||!height||width>4096||height>4096) {
        result.status=ImageReadStatus::Unsupported; result.error="PNG dimensions exceed 4096 host bound"; return result;
    }
    auto pixels=std::make_shared<spui::ImagePixels>(); int w=0,h=0;
    if (!image::decode_from_memory(resource.bytes.data(),resource.bytes.size(),pixels->bgra,w,h)||
        w!=(int)width||h!=(int)height||pixels->bgra.size()!=size_t(width)*height*4) {
        result.status=ImageReadStatus::ReadFailure; result.error="PNG pixel decode failed/dimensions mismatch"; return result;
    }
    pixels->resource={key.instance,key.type,key.group}; pixels->width=width; pixels->height=height;
    pixels->provenance=label_+" record["+std::to_string(resource.matching_record_indices[0])+"]";
    result.image.resource=pixels->resource; result.image.logical_width=w; result.image.logical_height=h;
    result.image.pixels=std::move(pixels); result.status=ImageReadStatus::Found; return result;
}
ImageApplyResult apply_star_image(StarUiInstance& instance,u64 plan_id,const Phase1Request& request,ImageProvider& provider) {
    const auto fail=[](ImageApplyStatus s,const char* why) { return ImageApplyResult{s,why}; };
    if (!instance.active()||instance.id()!=plan_id) return fail(ImageApplyStatus::Blocked,"image request instance identity mismatch");
    if (request.operation!=Phase1Op::ImageResource&&request.operation!=Phase1Op::StageImageTarget)
        return fail(ImageApplyStatus::Unsupported,"not an executable image-binding operation");
    if (request.required_drawable_interface!=0xef3c47cfu)
        return fail(ImageApplyStatus::Unsupported,"image request interface unsupported (Animated Icon separate)");
    if (request.target.status==StarBindingStatus::Ambiguous) return fail(ImageApplyStatus::Ambiguous,"unresolved duplicate target ControlID");
    if (request.target.status==StarBindingStatus::Missing) return fail(ImageApplyStatus::Missing,"image target missing");
    const auto* window=target(instance,request.target);
    if (!window) return fail(ImageApplyStatus::Blocked,"image target object/control binding not observed");
    const u16 target_object=window->object_index; // do not retain a borrowed Window across provider code
    std::string error; u16 drawable=0xffff;
    if (!instance.image_target(window->object_index,drawable,error)) return {ImageApplyStatus::Unsupported,error};
    spui::ImageBinding binding;
    bool shared=false;
    if (request.operation==Phase1Op::StageImageTarget) {
        if (!request.source) return fail(ImageApplyStatus::Blocked,"stage image getter source unobserved");
        if (request.source->status==StarBindingStatus::Ambiguous) return fail(ImageApplyStatus::Ambiguous,"stage source ambiguous");
        const auto* source=target(instance,*request.source);
        if (!source) return fail(ImageApplyStatus::Blocked,"stage getter source missing/unresolved, not observed null image");
        if (!instance.current_image(source->object_index,binding,error)) return {ImageApplyStatus::Blocked,error};
        shared=true;
    }
    if (!binding.pixels) {
        if (!shared&&!request.resource) return fail(ImageApplyStatus::Blocked,"image resource key unobserved");
        const auto key=shared?ImageKey{binding.resource.instance,binding.resource.type,binding.resource.group}:*request.resource;
        auto loaded=provider.read(key);
        if (loaded.status!=ImageReadStatus::Found) return {status(loaded.status),loaded.error};
        if (!loaded.image.pixels||!spui::valid_image_binding(loaded.image)||loaded.image.resource.instance!=key.instance||
            loaded.image.resource.type!=key.type||loaded.image.resource.group!=key.group)
            return fail(ImageApplyStatus::ReadFailure,"provider Found lacks valid matching decoded pixels");
        if (shared) binding.pixels=loaded.image.pixels; // keep original atlas UV/logical size
        else binding=std::move(loaded.image);
    }
    const auto* current=target(instance,request.target);
    if (!instance.active()||instance.id()!=plan_id||!current||current->drawable_object!=drawable)
        return fail(ImageApplyStatus::Blocked,"image target lifetime/binding changed during provider call");
    if (!instance.set_image(target_object,binding,error)) return {ImageApplyStatus::ReadFailure,error};
    return {ImageApplyStatus::Applied,{}};
}
}  // namespace os::gge
