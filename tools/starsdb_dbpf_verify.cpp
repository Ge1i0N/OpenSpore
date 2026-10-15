// Synthetic DBPF key lookup fixtures only; never original save data.
#include "engine/starsdb_dbpf.h"
#include "engine/refpack.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>

using namespace os;
using namespace os::pkg;
using namespace os::starsdb;
using Bytes=std::vector<u8>;
struct Item { ResourceKey key; Bytes bytes; };
static void be(Bytes& out,u32 value) { for (int s=24;s>=0;s-=8) out.push_back((u8)(value>>s)); }
static Bytes words(std::initializer_list<u32> values) { Bytes out; for (auto v:values) be(out,v); return out; }
static Bytes frame(const std::vector<std::pair<u32,Bytes>>& fields) {
    u32 length=4+8*(u32)fields.size(); for (const auto& f:fields) length+=(u32)f.second.size();
    Bytes out=words({serialized::kObjectTag,length,(u32)fields.size()});
    for (const auto& f:fields) { be(out,f.first); be(out,(u32)f.second.size()); }
    for (const auto& f:fields) out.insert(out.end(),f.second.begin(),f.second.end());
    return out;
}
static void set(Bytes& out,size_t at,u32 value) { for (int s=0;s<32;s+=8) out[at+s/8]=(u8)(value>>s); }
static Bytes make_package(const std::vector<Item>& items) {
    constexpr u32 header=0x60, kind=4, prefix=8, stride=28;
    u32 data_end=header;
    for (const auto& item:items) data_end+=(u32)item.bytes.size();
    const u32 index_offset=data_end, index_size=prefix+stride*(u32)items.size();
    Bytes out(data_end+index_size,0);
    set(out,0,0x46504244); set(out,4,3); set(out,0x24,(u32)items.size());
    set(out,0x2c,index_size); set(out,0x3c,3); set(out,0x40,index_offset);
    u32 data=header;
    for (const auto& item:items) {
        std::copy(item.bytes.begin(),item.bytes.end(),out.begin()+data);
        data+=(u32)item.bytes.size();
    }
    set(out,index_offset,kind); // shared unknown field follows the kind.
    set(out,index_offset+4,0);
    for (size_t i=0;i<items.size();++i) {
        const auto& item=items[i]; const size_t at=index_offset+prefix+i*stride;
        u32 prior=header; for (size_t n=0;n<i;++n) prior+=(u32)items[n].bytes.size();
        set(out,at+0,item.key.type); set(out,at+4,item.key.group); set(out,at+8,item.key.instance);
        set(out,at+12,prior); set(out,at+16,(u32)item.bytes.size()); set(out,at+20,(u32)item.bytes.size());
        set(out,at+24,0x00010000);
    }
    return out;
}
static bool same(const std::vector<u8>& a,const Bytes& b) { return a==b; }
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: starsdb_dbpf_verify [new-synthetic-package-path]\n"; return 2; }
    size_t checks=0;
    const auto require=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<"DBPF key adapter: "<<why<<'\n'; return ok; };
    const ResourceKey root{0x1897c18,0x1897c18,0x1897c18};
    const ResourceKey index{kContextInstance,kContextIndexType,kContextGroup};
    const ResourceKey cls{kContextInstance,kStarRecordClass,kContextGroup};
    const ResourceKey absent{7,8,9};
    const std::vector<Item> items{{root,{1,2,3,4}},{index,{5,6}},{cls,{7,8,9}},{absent,{10}}};
    auto bytes=make_package(items);
    Package package;
    if (!require(package.open_from_memory(bytes.data(),bytes.size(),"synthetic-starsdb-dbpf") && package.index_parsed(),"synthetic DBPF parsed")) return 1;
    if (!require(package.find_records(root)==std::vector<u32>{0} && package.find_records(index)==std::vector<u32>{1} &&
                 package.find_records(cls)==std::vector<u32>{2} && package.find_records({99,99,99}).empty(),"instance/type/group exact mapping")) return 1;
    std::vector<u8> payload;
    if (!require(package.read_resource_decoded(2,payload) && same(payload,{7,8,9}),"exact payload read")) return 1;
    auto duplicate=make_package({items[0],items[0]});
    Package duplicate_package;
    if (!require(duplicate_package.open_from_memory(duplicate.data(),duplicate.size(),"duplicate") &&
                 duplicate_package.find_records(root)==std::vector<u32>{0,1},"duplicate matches are preserved")) return 1;
    auto malformed=bytes; set(malformed,0x40,(u32)malformed.size()+1);
    Package rejected;
    if (!require(!rejected.open_from_memory(malformed.data(),malformed.size(),"bad") && !rejected.ok(),"package bounds rejection")) return 1;
    auto missing=bytes; set(missing,0x40,0x60+4*(u32)items.size()+8+28*(u32)items.size());
    Package not_indexed;
    if (!require(!not_indexed.open_from_memory(missing.data(),missing.size(),"bad-index") && !not_indexed.ok(),"index offset rejection")) return 1;
    ExplicitDbpfReader reader; std::string error;
    if (!require(reader.read_unique(root).status==ResourceReadStatus::PackageRejected,"closed reader explicit failure")) return 1;
    if (!require(reader.open_from_memory(bytes,error) && reader.read_unique(cls).bytes==Bytes{7,8,9} &&
                 reader.read_unique({index.type,index.instance,index.group}).status==ResourceReadStatus::NotFound,
                 "adapter exact key order, not transposed")) return 1;
    if (!require(reader.open_from_memory(duplicate,error) && reader.read_unique(root).status==ResourceReadStatus::Ambiguous,
                 "adapter refuses duplicate records")) return 1;
    const auto reject=[&](const Bytes& bad) {
        return !reader.open_from_memory(bad,error) && !error.empty() &&
            reader.read_unique(root).status==ResourceReadStatus::PackageRejected;
    };
    for (size_t n=0;n<bytes.size();++n)
        if (!require(reject(Bytes(bytes.begin(),bytes.begin()+n)),"DBPF every truncation rejected/closed")) return 1;
    for (const auto& mutation:std::vector<std::pair<size_t,u32>>{
            {0,0},{4,2},{4,4},{8,1},{0x3c,0},{0x24,65537},{0x2c,4},{0x40,0xffffffffu}}) {
        auto changed=bytes; set(changed,mutation.first,mutation.second);
        if (!require(reject(changed),"version/size/count preflight")) return 1;
    }
    const size_t directory=0x60+4+2+3+1;
    auto unknown_kind=bytes; set(unknown_kind,directory,0x80000004u);
    if (!require(reject(unknown_kind),"unknown high shared mask rejected")) return 1;
    // Relevant-record validation happens before allocating packed/decoded payloads.
    for (const auto& mutation:std::vector<std::pair<size_t,u32>>{
            {directory+4,1},{directory+8+24,0x00010001},{directory+8+24,0},
            {directory+8+20,0xffffffffu},{directory+8+20,3},{directory+8+12,0},
            {directory+8+12,0xffffffffu},{directory+8+16,0},{directory+8+24,0x0001ffff}}) {
        auto changed=bytes; set(changed,mutation.first,mutation.second);
        if (!require(reader.open_from_memory(changed,error) && reader.read_unique(root).status==ResourceReadStatus::ReadFailure,
                     "resource size/compression/flags/high-key/bounds failure")) return 1;
    }
    // Full physical synthetic path: DBPF -> exact root/index/class resources -> StarRecord fields.
    const Bytes star_literal{0,0,0,0x24,0x5f,0xbe,0x5e,0x5e,0x12,0x34,0x56,0x78,
        1,0xa8,0x0d,0x26,0,0,0,0x10,0,0,0,1,3,0xd5,0x8b,0xf2,0,0,0,4,0x11,0x22,0x33,0x44};
    auto root_bytes=frame({{0x03f140b6,words({37})}});
    auto manager=frame({{0x03ea8676,words({1,0x12345678,kStarRecordClass})},{0x03ea8679,words({0})}});
    root_bytes.insert(root_bytes.end(),manager.begin(),manager.end());
    const auto context=words({1,1,1,kStarRecordClass,1});
    const std::vector<Item> complete{{root,root_bytes},{index,context},{cls,star_literal}};
    auto physical=make_package(complete);
    PhysicalContextResult result;
    if (!require(reader.open_from_memory(physical,error) && inspect_dbpf_context(reader,root,result,error) &&
        result.root_prefix && result.root_prefix->header.saved_game_version_on_save==37u && result.context_index &&
        result.manager_links && result.manager_links->starters && result.manager_links->saved &&
        result.manager_links->saved->empty() && result.decoded_classes.size()==1 &&
        result.decoded_classes[0].objects[0].star && result.decoded_classes[0].objects[0].star->key==0x11223344u &&
        resolve_star_reference((*result.manager_links->starters)[0],result.decoded_classes[0]).status==ReferenceResolution::StarFields,
        "physical DBPF root/context/class/reference structural integration")) return 1;
    auto compressed_items=complete;
    Bytes compressed{0x10,0xfb,0,0,36,0xe8}; // 36 literal bytes, then explicit tail
    compressed.insert(compressed.end(),star_literal.begin(),star_literal.end()); compressed.push_back(0xfc);
    compressed_items[2].bytes=compressed;
    auto compressed_dbpf=make_package(compressed_items);
    const size_t compressed_dir=compressed_dbpf.size()-(8+28*3);
    set(compressed_dbpf,compressed_dir+8+2*28+20,36);
    set(compressed_dbpf,compressed_dir+8+2*28+24,0x0001ffff);
    if (!require(reader.open_from_memory(compressed_dbpf,error) && inspect_dbpf_context(reader,root,result,error) &&
                 result.classes[0].bytes==star_literal,"RefPack decoded class enters same envelope/CRC path")) return 1;
    std::vector<u8> decoded;
    const std::vector<Bytes> expansions{
        {0x10,0xfb,0,0,1,0xe0,1,2,3,4,0xfc}, // literals exceed declared decoded length
        {0x10,0xfb,0,0,1,0xff,1,2,3},       // tail exceeds declared decoded length
        {0x10,0xfb,0,0,4,0xe0,1,2,3,4,0,0,0xfc}, // backreference exceeds remaining output
        {0x10,0xfb,0,0,1,1,0,65,0xfc}}; // inline literal fits, match does not
    for (const auto& expansion:expansions)
        if (!require(refpack::decode(expansion.data(),(u32)expansion.size(),0,decoded)==(u32)-1 && decoded.size()<=4,
                     "RefPack refuses expansion beyond declared size before appending")) return 1;
    auto unsupported=complete; unsupported[2].bytes.back()^=1;
    auto bad_crc=make_package(unsupported);
    const auto atomic_failure=[&](const Bytes& input) {
        PhysicalContextResult sentinel; sentinel.error="UNCHANGED";
        return reader.open_from_memory(input,error) && !inspect_dbpf_context(reader,root,sentinel,error) &&
            !error.empty() && sentinel.error=="UNCHANGED" && sentinel.decoded_classes.empty() && !sentinel.root_prefix;
    };
    if (!require(atomic_failure(bad_crc),"bad CRC leaves all physical output unchanged")) return 1;
    if (!require(atomic_failure(make_package({complete[0],complete[1]})),"missing class not empty/ready")) return 1;
    if (!require(atomic_failure(make_package({complete[0],complete[2]})),"missing context index not empty/ready")) return 1;
    if (!require(atomic_failure(make_package({complete[1],complete[2]})),"missing root is error")) return 1;
    if (!require(atomic_failure(make_package({complete[0],complete[0],complete[1],complete[2]})),"ambiguous root is error")) return 1;
    unsupported=complete; unsupported[0].bytes.push_back(1);
    if (!require(atomic_failure(make_package(unsupported)),"root unsupported tail is error")) return 1;
    unsupported=complete; unsupported[1].bytes=words({2,0,0});
    if (!require(atomic_failure(make_package(unsupported)),"unsupported context version is error")) return 1;
    if (argc==2) {
        const std::filesystem::path path=argv[1];
        if (std::filesystem::exists(path)) { std::cerr<<"synthetic destination already exists\n"; return 2; }
        std::ofstream file(path,std::ios::binary); file.write((const char*)physical.data(),(std::streamsize)physical.size());
        if (!file) { std::cerr<<"failed writing synthetic DBPF\n"; return 1; }
        file.close();
        if (!require(reader.open(path.string(),error),"explicit DBPF adapter open")) return 1;
        auto found=reader.read_unique(cls); auto none=reader.read_unique({99,99,99});
        if (!require(found.status==ResourceReadStatus::Found && found.bytes==star_literal,"adapter found payload")) return 1;
        if (!require(none.status==ResourceReadStatus::NotFound,"adapter missing is distinct")) return 1;
        if (!require(inspect_physical_context(path.string(),root.instance,root.type,root.group,result,error),"physical explicit-path integration")) return 1;
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC DBPF (not a save): "<<path.string()<<'\n';
    }
    std::cout<<checks<<" constructed DBPF ResourceKey/adapter checks PASS; no real stars.db claim\n";
    return 0;
}
