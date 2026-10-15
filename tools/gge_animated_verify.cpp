// Static AnimatedIconWin frame selector: actual SPUI/atlas + explicitly controlled edge/GPU tests.
#include "engine/gge_image_provider.h"
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
using Change=StarUiInstance::FrameChange;
static void le(Bytes& b,size_t at,u32 v) { for (size_t i=0;i<4;++i) b[at+i]=(u8)(v>>(8*i)); }
static spui::Property* prop(spui::Layout& layout,u16 object,u32 id) {
    for (auto& o:layout.objects) if (o.index==object) for (auto& p:o.properties) if (!p.depth&&p.id==id) return &p;
    return nullptr;
}
static void scalar(spui::Layout& layout,u16 object,u32 id,u32 value) { auto* p=prop(layout,object,id); le(p->raw,0,value); }
static bool close_float(float a,float b) { return std::abs(a-b)<.00001f; }
static bool gpu(const spui::ImageQuad& first,const spui::ImageQuad& last,const char* capture) {
    if (std::filesystem::exists(capture)) return false;
    HWND window=CreateWindowExW(0,L"STATIC",L"Controlled Animated Icon frame readback",WS_OVERLAPPEDWINDOW,
        0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if (!window) return false;
    render::Device device; render::Overlay overlay; render::Config cfg; cfg.width=64; cfg.height=32; cfg.vsync=false;
    std::string error; IDirect3DTexture9* texture=nullptr;
    bool ok=device.init(window,cfg,error)&&overlay.init(device.d3d())&&device.begin_frame();
    if (ok) {
        texture=device.create_texture_bgra(first.pixels->bgra.data(),(int)first.pixels->width,(int)first.pixels->height);
        ok=texture!=nullptr; device.clear(0xff000000); overlay.begin(64,32);
        if (ok) {
            ok=overlay.set_clip(4,4,60,28);
            for (const auto* q:{&first,&last}) overlay.image_uv(q->area.left,q->area.top,q->area.w(),q->area.h(),texture,
                q->uv.left,q->uv.top,q->uv.right,q->uv.bottom);
        }
        overlay.reset_clip(); overlay.end(); device.request_capture(capture); device.end_frame();
    }
    if (texture) texture->Release();
    overlay.shutdown(); device.shutdown(); DestroyWindow(window);
    if (!ok) return false;
    std::ifstream f(capture,std::ios::binary); Bytes bmp(std::istreambuf_iterator<char>(f),{});
    if (bmp.size()!=54+64*32*4) return false;
    const auto pixel=[&](int x,int y) { u32 v=0; std::memcpy(&v,bmp.data()+54+((31-y)*64+x)*4,4); return v&0xffffff; };
    return pixel(16,16)==0xff0000&&pixel(48,16)==0x0000ff&&pixel(2,16)==0&&pixel(62,16)==0&&pixel(16,2)==0;
}
int main(int argc,char** argv) {
    if (argc!=4) { std::cerr<<"usage: gge_animated_verify <SPUI-fixtures> <explicit-original-atlas-package> <new-controlled.bmp>\n"; return 2; }
    size_t checks=0,fixtures=0; std::string error;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    StreamingImageProvider atlas;
    if (!check(atlas.open(argv[2],error),"explicit original atlas streaming provider open")) return 1;
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt"); std::string name;
    size_t bytes,objects,windows,props,links,roots;
    while (manifest>>name>>bytes>>objects>>windows>>props>>links>>roots) {
        if (name.find("21df01f2")==std::string::npos) continue;
        std::ifstream f(std::filesystem::path(argv[1])/name,std::ios::binary); Bytes data(std::istreambuf_iterator<char>(f),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error),"actual star layout parsed")) return 1;
        source.instance=kStarUiLayout; GameEntry entry; entry.stage=4; entry.saved=true; // controlled metadata, not a real save
        StarUiInstance a,b;
        if (!check(a.instantiate(11,source,entry,error)&&b.instantiate(22,source,entry,error),"independent real instances")) return 1;
        if (!check(a.control_objects(0x026e0002u)==std::vector<u16>{45,55},"duplicate ControlID retained, no class-based choice")) return 1;
        i32 frame=-9; Change change=Change::Changed; spui::AnimationDescriptor descriptor;
        if (!check(!a.animation(55,descriptor,error)&&!a.animation_frame(0xffff,frame,error)&&frame==-9,
                   "wrong regular window/missing target rejected, output unchanged")) return 1;
        if (!check(!a.set_animation_frame(55,2,change,error)&&change==Change::Changed,"regular duplicate cannot accept index")) return 1;
        std::vector<u16> icons;
        for (const auto& w:source.windows) if (w.class_id==0x0106f14bu) icons.push_back(w.object_index);
        if (!check(icons==std::vector<u16>{45,93},"two actual Animated Icon windows")) return 1;
        spui::ImageQuad unbound; unbound.external=1234;
        if (!check(!spui::animation_quad(a.layout(),45,{0,0,32,32},unbound)&&unbound.external==1234,
            "unbound image is unavailable, no guessed static/global resource")) return 1;
        for (u16 icon:icons) {
            if (!check(a.animation(icon,descriptor,error)&&descriptor.frames==5&&descriptor.rows==1&&descriptor.columns==5&&
                descriptor.image.logical_width==150&&descriptor.image.logical_height==30,"actual 5-frame strip descriptor/atlas logical size")) return 1;
            if (!check(a.animation_frame(icon,frame,error)&&frame==1,"native constructor frame1 retained, not zero")) return 1;
            if (!check(a.set_animation_frame(icon,1,change,error)&&change==Change::Unchanged&&!a.layout().runtime_animations.count(icon),
                "same index causes no new binding/redraw request")) return 1;
            const auto& key=descriptor.image.resource;
            const auto matches=atlas.package().find_records({key.instance,key.type,key.group});
            if (!check(matches.size()==1,"exact original atlas resource, no first-key/fallback lookup")) return 1;
            auto loaded=atlas.read({key.instance,key.type,key.group});
            if (!check(loaded.status==ImageReadStatus::Found&&loaded.image.pixels&&
                loaded.image.pixels->provenance.find("(file-backed)")!=std::string::npos,"actual original atlas decoded directly, no constructed DBPF carrier")) return 1;
            if (!check(a.set_animation_pixels(icon,loaded.image.pixels,error)&&a.animation_frame(icon,frame,error)&&frame==1,
                "atlas pixels installed without resetting current frame")) return 1;
            spui::ImageQuad quad;
            for (i32 i=0;i<5;++i) {
                if (!check(a.set_animation_frame(icon,i,change,error)&&change==Change::Changed&&a.animation_frame(icon,frame,error)&&frame==i,
                    "valid frame changes (including zero)")) return 1;
                if (!check(spui::animation_quad(a.layout(),icon,{0,0,32,32},quad)&&quad.pixels==loaded.image.pixels&&quad.external==0xffff&&
                    close_float(quad.uv.left,descriptor.image.uv.left+float(i)*descriptor.image.uv.w()/5)&&
                    close_float(quad.uv.right,descriptor.image.uv.left+float(i+1)*descriptor.image.uv.w()/5)&&
                    close_float(quad.uv.top,descriptor.image.uv.top)&&close_float(quad.uv.bottom,descriptor.image.uv.bottom),
                    "row-major frame quad composed inside actual atlas image rect")) return 1;
            }
            for (i32 invalid:{-1,5,std::numeric_limits<i32>::min(),std::numeric_limits<i32>::max()}) {
                if (!check(a.set_animation_frame(icon,invalid,change,error)&&change==Change::IgnoredOutOfRange&&
                    a.animation_frame(icon,frame,error)&&frame==4,"invalid index is native no-op true, NOT successful frame assignment")) return 1;
            }
            if (!check(a.set_animation_frame(icon,4,change,error)&&change==Change::Unchanged,"equal index no invalidation")) return 1;
            if (!check(!a.set_animation_pixels(icon,nullptr,error)&&spui::animation_quad(a.layout(),icon,{0,0,32,32},quad)&&
                quad.pixels==loaded.image.pixels,"failed pixel replacement preserves prior image")) return 1;
            auto wrong=std::make_shared<spui::ImagePixels>(*loaded.image.pixels); wrong->resource.group^=1;
            if (!check(!a.set_animation_pixels(icon,wrong,error),"wrong key cannot populate animation")) return 1;
            wrong=std::make_shared<spui::ImagePixels>(*loaded.image.pixels); wrong->bgra.pop_back();
            if (!check(!a.set_animation_pixels(icon,wrong,error),"malformed BGRA fails atomically")) return 1;
            if (!check(b.animation_frame(icon,frame,error)&&frame==1&&b.layout().runtime_animations.empty()&&source.runtime_animations.empty()&&
                a.layout().runtime_images.empty(),"frame/image mutations isolate sibling/template/fill drawable state")) return 1;
            auto sentinel=quad; sentinel.external=1234;
            if (!check(!spui::animation_quad(a.layout(),icon,{0,0,0,32},sentinel)&&sentinel.external==1234,
                "invalid quad leaves output unchanged")) return 1;
            if (fixtures==0&&icon==45) std::cout<<"original atlas record="<<matches[0]<<" pixels="<<loaded.image.pixels->width<<'x'<<loaded.image.pixels->height<<'\n';
        }
        StarUiConfiguration config;
        if (!check(configure_star_ui(entry,false,config,error)&&config.stage_icon_index&&*config.stage_icon_index==3&&
            a.set_animation_frame(45,(i32)*config.stage_icon_index,change,error)&&change==Change::Changed&&
            a.animation_frame(93,frame,error)&&frame==4,"existing DF9200 stage-index plan consumed via explicit object choice, other window unchanged")) return 1;
        if (!check(b.set_animation_frame(45,2,change,error)&&change==Change::Changed&&
            !spui::animation_quad(b.layout(),45,{0,0,32,32},unbound),"index can be observed without claiming image pixels available")) return 1;
        if (!check(a.animation(45,descriptor,error)&&b.set_animation_pixels(45,a.layout().runtime_animations.at(45).image.pixels,error)&&
            b.animation_frame(45,frame,error)&&frame==2,"late pixel attachment preserves previously assigned index")) return 1;
        // Controlled variants exercise the unsupported/malformed descriptor boundary.
        for (u32 id:{0xeec1d007u,0xeec1d008u,0xeec1d009u}) {
            auto invalid=source; scalar(invalid,45,id,0); StarUiInstance host;
            if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error)&&host.layout().runtime_animations.empty(),
                "zero count/grid requires native normalization, no ready pixels guessed")) return 1;
            scalar(invalid,45,id,0xffffffffu);
            if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"signed-overflow/malformed grid rejected")) return 1;
        }
        auto invalid=source; scalar(invalid,45,0xeec1d007u,6); StarUiInstance host;
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"frame count above cells is host unsupported, not clamped")) return 1;
        invalid=source; prop(invalid,45,0xeec1d005u)->raw[0]=1;
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"Auto Play blocked instead of freezing it silently")) return 1;
        invalid=source; scalar(invalid,45,0xeec1d006u,30);
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"obsolete Frame Width normalization unsupported")) return 1;
        invalid=source; prop(invalid,45,0xeec1d007u)->code=11;
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"malformed scalar type rejected")) return 1;
        invalid=source; auto copy=*prop(invalid,45,0xeec1d007u);
        for (auto& o:invalid.objects) if (o.index==45) o.properties.push_back(copy);
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"duplicate property not first-match")) return 1;
        invalid=source; prop(invalid,45,0xeec1d003u)->refs={0xffff};
        if (!check(host.instantiate(33,invalid,entry,error)&&!host.animation(45,descriptor,error),"missing image not observed loaded")) return 1;
        // Controlled 2x3 grid + full-image UV descriptor, six solid cells; draw frame0 and frame4.
        auto grid=source; scalar(grid,45,0xeec1d007u,6); scalar(grid,45,0xeec1d008u,2); scalar(grid,45,0xeec1d009u,3);
        spui::Rect full{0,0,1,1}; auto* uv=prop(grid,44,0x01be0002u); std::memcpy(uv->raw.data(),&full,sizeof(full));
        if (!check(host.instantiate(44,grid,entry,error)&&host.animation(45,descriptor,error),"controlled 2x3 grid descriptor")) return 1;
        auto colours=std::make_shared<spui::ImagePixels>(); colours->resource=descriptor.image.resource;
        colours->width=48; colours->height=32; colours->bgra.resize(48*32*4);
        colours->provenance="controlled 2x3 solid-colour cells, not shipped animation pixels";
        for (u32 y=0;y<32;++y) for (u32 x=0;x<48;++x) {
            const u32 cell=(y/16)*3+x/16,colour=cell==0?0xffff0000:cell==4?0xff0000ff:0xff00ff00;
            std::memcpy(colours->bgra.data()+(y*48+x)*4,&colour,4);
        }
        spui::ImageQuad first,last;
        if (!check(host.set_animation_pixels(45,colours,error)&&host.set_animation_frame(45,0,change,error)&&
            spui::animation_quad(host.layout(),45,{0,0,32,32},first)&&host.set_animation_frame(45,4,change,error)&&
            spui::animation_quad(host.layout(),45,{32,0,64,32},last)&&close_float(last.uv.left,1.f/3)&&close_float(last.uv.top,.5f),
            "multi-row quotient/remainder and static frame source")) return 1;
        if (fixtures==0&&!check(gpu(first,last,argv[3]),"D3D9 draws distinct row/column frames and explicit scissor pixels")) return 1;
        if (!check(a.instantiate(11,source,entry,error)&&a.layout().runtime_animations.empty()&&a.animation_frame(45,frame,error)&&
            frame==1&&!a.native_mount_complete(),"reinstantiate clears animation state, no native readiness")) return 1;
        a.clear();
        if (!check(!a.animation_frame(45,frame,error)&&!a.set_animation_pixels(45,colours,error),"cleared lifetime rejects animation work")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both real star layouts tested")) return 1;
    std::cout<<checks<<" Animated Icon static frame/atlas/isolation/boundary checks PASS; controlled GPU readback PASS; no playback or GGE scene readiness\n";
    return 0;
}
