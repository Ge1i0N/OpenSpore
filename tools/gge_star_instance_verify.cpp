#include "engine/gge_star_instance.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <type_traits>
using namespace os;
using namespace os::gge;
static const spui::Window* object(const spui::Layout& layout,u16 index) {
    for (const auto& w:layout.windows) if (w.object_index==index) return &w;
    return nullptr;
}
static spui::Layout controlled_tree() {
    spui::Layout lay; lay.instance=kStarUiLayout; lay.sequential=true; lay.roots={0}; lay.windows.resize(3);
    auto& root=lay.windows[0]; root.object_index=7; root.control_id=kStarUiRoot; root.parent=-1;
    root.flags=0x413; root.has_area=true; root.area={0,0,50,50}; root.children={1,2};
    for (size_t i=1;i<3;++i) {
        auto& w=lay.windows[i]; w.object_index=(u16)(7+i); w.control_id=123; w.parent=0;
        w.flags=3; w.has_area=true; w.area={10,10,80,30};
    }
    return lay;
}
int main(int argc,char** argv) {
    static_assert(!std::is_copy_constructible_v<StarUiInstance>);
    if (argc!=2) { std::cerr<<"usage: gge_star_instance_verify <SPUI-fixture-dir>\n"; return 2; }
    size_t checks=0,fixtures=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    std::ifstream manifest(std::filesystem::path(argv[1])/"manifest.txt");
    std::string file,error; size_t bytes,objects,windows,props,links,roots;
    while (manifest>>file>>bytes>>objects>>windows>>props>>links>>roots) {
        if (file.find("21df01f2")==std::string::npos) continue;
        std::ifstream input(std::filesystem::path(argv[1])/file,std::ios::binary);
        std::vector<u8> data(std::istreambuf_iterator<char>(input),{});
        spui::Layout source;
        if (!check(spui::parse_binary(data,source,error)&&source.windows.size()==58&&source.objects.size()==152,
                   "real star layout parsed")) return 1;
        source.instance=kStarUiLayout;
        GameEntry entry; entry.star_id=999; entry.saved=true; entry.name=u"controlled metadata";
        StarUiInstance a,b;
        if (!check(a.instantiate(11,source,entry,error)&&b.instantiate(22,source,entry,error)&&a.active()&&b.active()&&
            !a.native_mount_complete()&&!b.native_mount_complete()&&a.id()!=b.id(),"separate host instances, no native mount claim")) return 1;
        const auto root_object=a.root_object(); const auto* initial=object(source,root_object);
        if (!check(initial && root_object==177 && a.control_objects(kStarUiRoot)==std::vector<u16>{177} &&
            source.roots.size()==1 && initial->parent==-1 && initial->class_id==0x4ec1b8d8u &&
            initial->area.left==76 && initial->area.top==167 && initial->area.w()==48 && initial->area.h()==48,
                    "per-card positioning root177 matches nonrecursive UILayout lookup, not a global service")) return 1;
        const auto repeated=a.control_objects(0x026e0002);
        if (!check(repeated.size()==2&&repeated[0]!=repeated[1],"REAL duplicate stage ControlID retained as two objects")) return 1;
        const float dx=250-initial->area.left,dy=300-initial->area.top;
        if (!check(a.move_root({250,300,true},error),"real graph root move")) return 1;
        for (const auto& before:source.windows) {
            const auto* after=object(a.layout(),before.object_index);
            const auto* other=object(b.layout(),before.object_index);
            if (!check(after&&other && after->placed_area.left==before.placed_area.left+dx &&
                after->placed_area.top==before.placed_area.top+dy && other->placed_area.left==before.placed_area.left &&
                other->placed_area.top==before.placed_area.top,"every real descendant translated, sibling/template isolated")) return 1;
        }
        const u32 original_flags=object(source,repeated[0])->flags;
        if (!check(a.set_window_flag(repeated[0],1,(original_flags&1u)==0,error)&&
            object(a.layout(),repeated[0])->flags==(original_flags^1u)&&
            object(b.layout(),repeated[0])->flags==original_flags&&object(source,repeated[0])->flags==original_flags&&
            object(a.layout(),repeated[1])->flags==object(source,repeated[1])->flags,"explicit duplicate target mutation isolated")) return 1;
        const auto caption=a.control_objects(0x03c4925e);
        if (!check(caption.size()==1&&a.set_caption_reference(caption[0],{0x2b7a0311,0x067735d5},error)&&
            object(a.layout(),caption[0])->caption_is_ref&&object(a.layout(),caption[0])->caption_key==0x067735d5&&
            object(b.layout(),caption[0])->caption_key==object(source,caption[0])->caption_key,"locale reference overlay isolated")) return 1;
        if (!check(a.move_root({999,999,false},error)&&object(a.layout(),root_object)->area.left==250,
                   "deadband suppresses actual geometry mutation")) return 1;
        if (!check(!a.set_window_flag(0xffff,1,true,error)&&!a.set_window_flag(root_object,0x80000000u,true,error)&&
            !a.set_caption_reference(0xffff,{},error)&&object(a.layout(),root_object)->area.left==250,"unsupported target/mask atomic failure")) return 1;
        auto bad=source; bad.windows[bad.roots[0]].children.push_back(bad.roots[0]);
        if (!check(!a.instantiate(33,bad,entry,error)&&a.id()==11&&object(a.layout(),root_object)->area.left==250,
                   "failed reinstantiate retains existing owned instance")) return 1;
        a.clear();
        if (!check(!a.active()&&a.root_object()==0xffff&&a.layout().windows.empty()&&b.active()&&source.windows.size()==58,
                   "clear does not remove sibling or source")) return 1;
        ++fixtures;
    }
    if (!check(fixtures==2,"both source-package star layouts tested")) return 1;
    auto source=controlled_tree(); GameEntry entry; StarUiInstance instance;
    if (!check(instance.instantiate(1,source,entry,error)&&instance.move_root({100,100,true},error),"controlled pixel geometry")) return 1;
    const spui::Rect viewport{0,0,800,600};
    auto picked=instance.pick(120,115,viewport);
    if (!check(picked&&picked->object_index==9,"pixel-space pick does NOT rescale moved root as whole-screen layout")) return 1;
    if (!check(!instance.pick(160,115,viewport)&&!instance.pick(150,115,viewport)&&instance.pick(149,115,viewport),
               "shared ancestor Clip half-open boundary")) return 1;
    if (!check(instance.set_window_flag(9,2,false,error)&&instance.pick(120,115,viewport)->object_index==8,
               "disabled overlapping child falls through, host reverse order")) return 1;
    if (!check(instance.set_window_flag(8,0x10,true,error)&&!instance.pick(120,115,viewport),"IgnoreMouse handled per object")) return 1;
    if (!check(instance.set_window_flag(8,0x10,false,error)&&instance.set_window_flag(7,1,false,error)&&
        !instance.pick(120,115,viewport)&&spui::paint_order(instance.layout()).empty(),"ancestor visibility recomputed for paint/pick")) return 1;
    if (!check(instance.set_window_flag(7,1,true,error)&&instance.set_window_flag(7,2,false,error)&&
        !instance.pick(120,115,viewport),"ancestor enabled recomputed")) return 1;
    if (!check(instance.set_window_flag(7,2,true,error)&&instance.set_window_flag(7,0x400,false,error)&&
        instance.pick(160,115,viewport)&&!instance.pick(160,115,{0,0,150,600}),"Clip mutation and explicit viewport clipping")) return 1;
    if (!check(!instance.pick(std::numeric_limits<float>::quiet_NaN(),0,viewport)&&
        !instance.pick(120,115,{0,0,0,0}),"invalid pixel/viewport safe")) return 1;
    const auto reject=[&](const spui::Layout& bad) {
        return !instance.instantiate(99,bad,entry,error)&&!error.empty()&&instance.id()==1&&
            object(instance.layout(),7)->area.left==100;
    };
    auto bad=source; bad.instance=0;
    if (!check(reject(bad),"wrong template identity")) return 1;
    bad=source; bad.sequential=false;
    if (!check(reject(bad),"nonsequential template")) return 1;
    bad=source; bad.roots={0,1};
    if (!check(reject(bad),"multiple roots unsupported")) return 1;
    bad=source; bad.windows[0].control_id=0;
    if (!check(reject(bad),"wrong root control")) return 1;
    bad=source; bad.roots={100};
    if (!check(reject(bad),"root outside window vector")) return 1;
    bad=source; bad.windows[0].children={99};
    if (!check(reject(bad),"child outside window vector")) return 1;
    bad=source; bad.windows[0].children.push_back(1);
    if (!check(reject(bad),"repeated child edge")) return 1;
    bad=source; bad.windows[1].parent=99;
    if (!check(reject(bad),"parent/child mismatch")) return 1;
    bad=source; bad.windows[2].object_index=8;
    if (!check(reject(bad),"duplicate serialized OBJECT identity is invalid, unlike duplicate ControlID")) return 1;
    bad=source; bad.windows[2].object_index=0xffff;
    if (!check(reject(bad),"null object index not window identity")) return 1;
    bad=source; bad.windows[0].children={1};
    if (!check(reject(bad),"unreachable graph not accepted")) return 1;
    bad=source; bad.windows[0].area.right=std::numeric_limits<float>::infinity();
    if (!check(reject(bad),"nonfinite root area")) return 1;
    bad=source; bad.windows[0].area.left=-std::numeric_limits<float>::max(); bad.windows[0].area.right=std::numeric_limits<float>::max();
    if (!check(reject(bad),"finite coordinates with overflowing extent rejected")) return 1;
    bad=source; bad.windows.resize(4097);
    if (!check(reject(bad),"host window cap")) return 1;
    if (!check(!instance.instantiate(0,source,entry,error)&&instance.id()==1,"zero host identity rejected")) return 1;
    auto oversized=entry; oversized.name=std::u16string(4097,u'x');
    if (!check(!instance.instantiate(2,source,oversized,error)&&instance.id()==1,"oversized entry text leaves instance unchanged")) return 1;
    if (!check(!instance.move_root({std::numeric_limits<float>::quiet_NaN(),0,true},error)&&
        object(instance.layout(),7)->area.left==100,"nonfinite move atomic")) return 1;
    if (!check(!instance.move_root({std::numeric_limits<float>::max(),0,true},error)&&
        object(instance.layout(),7)->area.left==100,"large finite move cannot collapse root width")) return 1;
    instance.clear();
    if (!check(!instance.pick(120,115,viewport)&&!instance.move_root({1,1,true},error)&&
        instance.control_objects(123).empty(),"cleared instance owns no graph or pick target")) return 1;
    std::cout<<checks<<" host star-instance ownership/geometry/flags/caption/picking checks PASS; "<<fixtures
        <<" real layouts; no native mount/plugin/event/camera readiness claimed\n";
    return 0;
}
