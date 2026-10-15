// Actual explicit PNG resource + real SPUI controls; controlled failure/provider/pixel fixtures.
#include "engine/gge_image_provider.h"
#include "engine/spui_drawable.h"
#include "render/device.h"
#include "render/overlay.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
using namespace os;
using namespace os::gge;
using Bytes=std::vector<u8>;
static void le(Bytes& b,size_t at,u32 v) { for (size_t i=0;i<4;++i) b[at+i]=(u8)(v>>(8*i)); }
static Bytes package(const ImageKey& key,const Bytes& png,bool duplicate=false) {
    size_t offset=0x60+png.size(); const size_t count=duplicate?2:1;
    Bytes out(offset+8+28*count,0);
    le(out,0,0x46504244); le(out,4,3); le(out,0x24,(u32)count); le(out,0x2c,(u32)(8+28*count));
    le(out,0x3c,3); le(out,0x40,(u32)offset); le(out,offset,4);
    std::copy(png.begin(),png.end(),out.begin()+0x60);
    for (size_t i=0;i<count;++i) {
        const size_t at=offset+8+28*i;
        le(out,at,key.type); le(out,at+4,key.group); le(out,at+8,key.instance); le(out,at+12,0x60);
        le(out,at+16,(u32)png.size()); le(out,at+20,(u32)png.size()); le(out,at+24,0x10000);
    }
    return out;
}
struct Provider final:ImageProvider {
    ImageRead value; unsigned calls=0;
    StarUiInstance* clear_during_read=nullptr;
    ImageRead read(const ImageKey&) override { ++calls; if (clear_during_read) clear_during_read->clear(); return value; }
};
static Phase1Request request(const spui::Window& w,Phase1Op op,const ImageKey& key) {
    Phase1Request r; r.operation=op; r.required_drawable_interface=0xef3c47cf;
    r.target.control=w.control_id; r.target.object=w.object_index; r.target.status=StarBindingStatus::Found;
    r.resource=key; return r;
}
static spui::ImageBinding solid() {
    auto p=std::make_shared<spui::ImagePixels>(); p->width=p->height=2; p->resource={123,0x2f7d0004,456};
    p->bgra={0,0,255,255,0,0,255,255,0,0,255,255,0,0,255,255}; p->provenance="controlled red2x2 fixture";
    spui::ImageBinding b; b.resource=p->resource; b.logical_width=b.logical_height=2; b.pixels=p; return b;
}
static bool pixels(const spui::ImageQuad& quad,const char* capture) {
    if (std::filesystem::exists(capture)) return false;
    HWND window=CreateWindowExW(0,L"STATIC",L"Controlled GGE image binding readback",WS_OVERLAPPEDWINDOW,
        0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return false;
    render::Device device; render::Overlay overlay; render::Config cfg; cfg.width=cfg.height=64; cfg.vsync=false;
    std::string error; IDirect3DTexture9* texture=nullptr;
    bool ok=device.init(window,cfg,error)&&overlay.init(device.d3d())&&device.begin_frame();
    if (ok) {
        texture=device.create_texture_bgra(quad.pixels->bgra.data(),(int)quad.pixels->width,(int)quad.pixels->height);
        ok=texture!=nullptr; device.clear(0xff000000); overlay.begin(64,64);
        if (ok) {
            ok=overlay.set_clip(16,16,48,48);
            overlay.image_uv(quad.area.left,quad.area.top,quad.area.w(),quad.area.h(),texture,
                quad.uv.left,quad.uv.top,quad.uv.right,quad.uv.bottom);
        }
        overlay.reset_clip(); overlay.end(); device.request_capture(capture); device.end_frame();
    }
    if (texture) texture->Release(); overlay.shutdown(); device.shutdown(); DestroyWindow(window);
    if (!ok) return false;
    std::ifstream file(capture,std::ios::binary); Bytes bmp(std::istreambuf_iterator<char>(file),{});
    if (bmp.size()!=54+64*64*4||bmp[0]!='B'||bmp[1]!='M') return false;
    const auto pixel=[&](int x,int y) { u32 v=0; std::memcpy(&v,bmp.data()+54+((63-y)*64+x)*4,4); return v&0xffffff; };
    return pixel(24,24)==0xff0000&&pixel(15,24)==0&&pixel(48,24)==0&&pixel(24,48)==0;
}
int main(int argc,char** argv) {
    if (argc!=4) { std::cerr<<"usage: gge_image_verify <SPUI-fixture-dir> <explicit-PNG-package> <new-controlled-capture.bmp>\n"; return 2; }
    size_t checks=0,fixtures=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    ExplicitImageProvider actual;
    if (!check(actual.open(argv[2],error),"explicit actual package open")) return 1;
    ImageKey key; ImageRead real; u32 record=0;
    for (u32 i=0;i<actual.package().records().size();++i) {
        const auto& r=actual.package().records()[i];
        if (r.type()!=0x2f7d0004u||r.mem_size()>1048576||!r.instance()||actual.package().find_records({r.instance(),r.type(),r.group()}).size()!=1) continue;
        key={r.instance(),r.type(),r.group()}; real=actual.read(key);
        if (real.status==ImageReadStatus::Found) { record=i; break; }
    }
    if (!check(real.status==ImageReadStatus::Found&&real.image.pixels&&spui::valid_image_binding(real.image)&&
        !real.image.pixels->provenance.empty(),"actual PNG decoded to bounded BGRA with exact provenance")) return 1;
    pkg::Package original; Bytes png;
    if (!check(original.open(argv[2])&&original.read_resource_decoded(record,png),"original resource bytes for constructed provider failures")) return 1;
    ExplicitImageProvider constructed;
    if (!check(constructed.open_from_memory(package(key,png),error)&&constructed.read(key).status==ImageReadStatus::Found,
               "same actual PNG through synthetic one-record container")) return 1;
    if (!check(constructed.read({key.instance,key.type,key.group^0x80000000u}).status==ImageReadStatus::Missing,"actual Missing key not fallback")) return 1;
    if (!check(constructed.open_from_memory(package(key,png,true),error)&&constructed.read(key).status==ImageReadStatus::Ambiguous,"actual duplicate key Ambiguous not first match")) return 1;
    auto broken=png; broken[0]=0;
    if (!check(constructed.open_from_memory(package(key,broken),error)&&constructed.read(key).status==ImageReadStatus::ReadFailure,"corrupt PNG header fails")) return 1;
    broken=png; broken[16]=0x7f;
    if (!check(constructed.open_from_memory(package(key,broken),error)&&constructed.read(key).status==ImageReadStatus::Unsupported,"PNG IHDR dimension cap before GDI+ allocation")) return 1;
    broken=Bytes(png.begin(),png.begin()+33);
    if (!check(constructed.open_from_memory(package(key,broken),error)&&constructed.read(key).status==ImageReadStatus::ReadFailure,"valid header missing pixel payload")) return 1;
    if (!check(!constructed.open_from_memory({},error)&&constructed.read(key).status==ImageReadStatus::Unknown,"closed package not observed missing asset")) return 1;
    if (!check(actual.read({0,key.type,key.group}).status==ImageReadStatus::Unsupported&&
        actual.read({key.instance,42,key.group}).status==ImageReadStatus::Unsupported,"zero/nonPNG key policy")) return 1;

    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>name>>bytes>>objects>>windows>>props>>links>>roots) {
        if (name.find("21df01f2")==std::string::npos) continue;
        std::ifstream file(std::filesystem::path(argv[1])/name,std::ios::binary); Bytes data(std::istreambuf_iterator<char>(file),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error),"actual SPUI")) return 1;
        source.instance=kStarUiLayout; GameEntry entry; entry.image=key;
        StarUiInstance a,b;
        if (!check(a.instantiate(11,source,entry,error)&&b.instantiate(22,source,entry,error),"actual independent instances")) return 1;
        const spui::Window* target=nullptr; const spui::Window* other=nullptr;
        for (const auto& w:source.windows) {
            u16 drawable=0;
            if (!a.image_target(w.object_index,drawable,error)) continue;
            if (!target) target=&w;
            else if (w.drawable_object!=target->drawable_object) { other=&w; break; }
        }
        if (!check(target&&other,"actual plain drawable targets found, not heuristic reachable image")) return 1;
        const auto r=request(*target,Phase1Op::ImageResource,key);
        if (!check(apply_star_image(a,11,r,actual).status==ImageApplyStatus::Applied,"actual provider -> actual CF drawable override")) return 1;
        spui::ImageQuad quad;
        if (!check(spui::drawable_quad(a.layout(),target->drawable_object,{0,0,64,64},0,quad)&&quad.pixels&&
            quad.pixels->resource.instance==key.instance&&quad.external==0xffff&&source.runtime_images.empty()&&
            b.layout().runtime_images.empty(),"resolver uses decoded override, source and sibling unaffected")) return 1;
        const auto before=a.layout().runtime_images.at(target->drawable_object).pixels;
        Provider controlled;
        for (auto s:{ImageReadStatus::Unknown,ImageReadStatus::Missing,ImageReadStatus::Ambiguous,ImageReadStatus::Unsupported,ImageReadStatus::ReadFailure}) {
            controlled.value={}; controlled.value.status=s;
            const auto applied=apply_star_image(a,11,r,controlled);
            if (!check(applied.status!=ImageApplyStatus::Applied&&a.layout().runtime_images.at(target->drawable_object).pixels==before,
                       "all provider nonFound cases retain previous image")) return 1;
        }
        controlled.value={}; controlled.value.status=ImageReadStatus::Found;
        if (!check(apply_star_image(a,11,r,controlled).status==ImageApplyStatus::ReadFailure,"dishonest Found no pixels rejected")) return 1;
        controlled.value.image=solid();
        if (!check(apply_star_image(a,11,r,controlled).status==ImageApplyStatus::ReadFailure&&
            a.layout().runtime_images.at(target->drawable_object).pixels==before,"dishonest provider Found with wrong key rejected")) return 1;
        const unsigned calls=controlled.calls;
        auto invalid=r; invalid.target.status=StarBindingStatus::Ambiguous;
        if (!check(apply_star_image(a,11,invalid,controlled).status==ImageApplyStatus::Ambiguous&&controlled.calls==calls,"ambiguous targets block before provider")) return 1;
        invalid=r; invalid.target.object=u16{0xffff};
        if (!check(apply_star_image(a,11,invalid,controlled).status==ImageApplyStatus::Blocked&&controlled.calls==calls,"stale object mapping rejected")) return 1;
        if (!check(apply_star_image(a,12,r,controlled).status==ImageApplyStatus::Blocked&&controlled.calls==calls,"wrong instance rejected")) return 1;
        invalid=r; invalid.operation=Phase1Op::ProfileName;
        if (!check(apply_star_image(a,11,invalid,controlled).status==ImageApplyStatus::Unsupported&&controlled.calls==calls,"unimplemented phase action not silently completed")) return 1;
        for (const auto& w:source.windows) if (w.class_id==0x0106f14bu) {
            auto animated=request(w,Phase1Op::ImageResource,key);
            if (!check(apply_star_image(a,11,animated,controlled).status==ImageApplyStatus::Unsupported&&controlled.calls==calls,"Animated Icon NOT implemented through plain drawable setter")) return 1;
        }
        // Share a decoded image getter result; no further provider read and preserve atlas metadata.
        auto atlas=real.image; atlas.uv={.25f,.25f,.75f,.75f}; atlas.logical_width=20; atlas.logical_height=10;
        if (!check(a.set_image(target->object_index,atlas,error),"explicit atlas descriptor")) return 1;
        auto transfer=request(*other,Phase1Op::StageImageTarget,{});
        transfer.source=r.target;
        if (!check(apply_star_image(a,11,transfer,controlled).status==ImageApplyStatus::Applied&&controlled.calls==calls&&
            a.layout().runtime_images.at(other->drawable_object).pixels==atlas.pixels&&
            a.layout().runtime_images.at(other->drawable_object).uv.left==.25f&&
            a.layout().runtime_images.at(other->drawable_object).logical_width==20,"shared getter bypasses resource fallback and preserves UV/logical dimensions")) return 1;
        invalid=transfer; invalid.source->status=StarBindingStatus::Ambiguous;
        if (!check(apply_star_image(a,11,invalid,controlled).status==ImageApplyStatus::Ambiguous&&controlled.calls==calls,
                   "ambiguous getter source not observed null image")) return 1;
        invalid=transfer; invalid.source.reset();
        if (!check(apply_star_image(a,11,invalid,controlled).status==ImageApplyStatus::Blocked&&controlled.calls==calls,
                   "unobserved getter no fallback lookup")) return 1;
        // An unloaded serialized atlas getter is loaded through the provider, preserving its UV/size.
        const spui::Window* getter=nullptr; spui::ImageBinding descriptor;
        for (const auto& w:b.layout().windows) if (b.current_image(w.object_index,descriptor,error)&&!descriptor.pixels) { getter=&w; break; }
        if (!check(getter!=nullptr,"actual serialized image getter descriptor")) return 1;
        auto loaded_atlas=std::make_shared<spui::ImagePixels>(*real.image.pixels);
        loaded_atlas->resource=descriptor.resource;
        controlled.value.status=ImageReadStatus::Found; controlled.value.image=real.image;
        controlled.value.image.resource=descriptor.resource; controlled.value.image.pixels=loaded_atlas;
        auto load_transfer=request(*other,Phase1Op::StageImageTarget,{});
        load_transfer.source=request(*getter,Phase1Op::ImageResource,{}).target;
        const auto prior_calls=controlled.calls;
        if (!check(apply_star_image(b,22,load_transfer,controlled).status==ImageApplyStatus::Applied&&controlled.calls==prior_calls+1&&
            b.layout().runtime_images.at(other->drawable_object).uv.left==descriptor.uv.left&&
            b.layout().runtime_images.at(other->drawable_object).logical_width==descriptor.logical_width,
            "unloaded getter descriptor resolved explicitly, retains atlas metadata")) return 1;
        auto aliased=source;
        for (auto& w:aliased.windows) if (w.object_index==other->object_index) w.drawable_object=target->drawable_object;
        StarUiInstance shared;
        if (!check(shared.instantiate(33,aliased,entry,error)&&shared.set_image(target->object_index,atlas,error)&&
            shared.current_image(other->object_index,descriptor,error)&&descriptor.pixels==atlas.pixels&&
            shared.layout().runtime_images.size()==1,"windows sharing drawable share image in one instance, not template/sibling")) return 1;
        // Controlled drawable property variant: image replacement must preserve fixed-axis alignment.
        auto fixed=source;
        for (auto& d:fixed.objects) if (d.index==target->drawable_object) {
            d.properties.erase(std::remove_if(d.properties.begin(),d.properties.end(),[](const spui::Property& p) {
                return p.id>=0x4f3c4a27u&&p.id<=0x4f3c4a2bu;
            }),d.properties.end());
            spui::Property flags; flags.id=0x4f3c4a27u; flags.code=9; flags.count=1; flags.raw={3,0,0,0};
            d.properties.push_back(flags);
        }
        auto red=solid(); StarUiInstance fixed_instance;
        if (!check(fixed_instance.instantiate(44,fixed,entry,error)&&fixed_instance.set_image(target->object_index,red,error)&&
            spui::drawable_quad(fixed_instance.layout(),target->drawable_object,{0,0,64,64},0,quad)&&
            quad.area.left==31&&quad.area.top==31&&quad.area.w()==2&&quad.area.h()==2,
            "image replacement keeps drawable fixed axes and constructor center alignment")) return 1;
        // Absent link in raw drawable can be populated, but missing drawable itself remains unsupported.
        auto absent_link=source;
        for (auto& d:absent_link.objects) if (d.index==target->drawable_object)
            d.properties.erase(std::remove_if(d.properties.begin(),d.properties.end(),[](const spui::Property& p) {
                return p.id==0x4f3c4a26u;
            }),d.properties.end());
        StarUiInstance no_image;
        if (!check(no_image.instantiate(55,absent_link,entry,error)&&!no_image.current_image(target->object_index,descriptor,error)&&
            no_image.set_image(target->object_index,red,error)&&no_image.current_image(target->object_index,descriptor,error)&&
            descriptor.pixels==red.pixels,"CF drawable with no serialized image can accept explicit image")) return 1;
        // Full pixel readback uses a CONTROLLED solid image and raw resolver quad, not a fake GGE scene.
        if (fixtures==0) {
            if (!check(a.set_image(target->object_index,red,error)&&spui::drawable_quad(a.layout(),target->drawable_object,{0,0,64,64},0,quad)&&
                quad.pixels==red.pixels,"controlled replacement reaches same resolver")) return 1;
            if (!check(pixels(quad,argv[3]),"real D3D9 decoded-override quad and scissor pixel readback")) return 1;
        }
        auto invalid_image=atlas; invalid_image.uv.right=std::numeric_limits<float>::infinity();
        const auto keep=a.layout().runtime_images.at(target->drawable_object).pixels;
        if (!check(!a.set_image(target->object_index,invalid_image,error)&&a.layout().runtime_images.at(target->drawable_object).pixels==keep,"invalid UV atomic")) return 1;
        invalid_image=atlas; invalid_image.pixels.reset();
        if (!check(!a.set_image(target->object_index,invalid_image,error),"unloaded descriptor cannot be applied")) return 1;
        auto malformed=std::make_shared<spui::ImagePixels>(*atlas.pixels); malformed->bgra.pop_back();
        invalid_image=atlas; invalid_image.pixels=malformed;
        if (!check(!a.set_image(target->object_index,invalid_image,error),"malformed decoded byte count rejected")) return 1;
        invalid_image=atlas; invalid_image.logical_width=-1;
        if (!check(!a.set_image(target->object_index,invalid_image,error),"negative logical dimensions rejected")) return 1;
        if (!check(a.instantiate(11,source,entry,error)&&a.layout().runtime_images.empty()&&!a.native_mount_complete(),"reinstantiate releases overrides, no native readiness")) return 1;
        controlled.value=real; controlled.clear_during_read=&a;
        if (!check(apply_star_image(a,11,r,controlled).status==ImageApplyStatus::Blocked&&!a.active(),
                   "provider reentry clears lifetime safely, borrowed target not dereferenced")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both actual star layouts")) return 1;
    std::cout<<checks<<" image provider/binding/resolver/isolation checks PASS; actual PNG record="<<record
        <<" size="<<real.image.pixels->width<<'x'<<real.image.pixels->height
        <<"; controlled D3D9 readback PASS, not a live GGE scene\n";
    return 0;
}
