// Constructed, explicit decoded streams derived from GA instructions, NOT original save fixtures.
#include "engine/starsdb_context.h"
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace os;
using namespace os::starsdb;
using Bytes=std::vector<u8>;
static void put(Bytes& out,u32 word) {
    for (int shift=24; shift>=0; shift-=8) out.push_back((u8)(word>>shift));
}
static void set(Bytes& out,size_t at,u32 word) {
    for (size_t i=0; i<4; ++i) out[at+i]=(u8)(word>>(24-i*8));
}
static Bytes words(std::initializer_list<u32> values) {
    Bytes out; for (u32 v : values) put(out,v); return out;
}
static Bytes record(u32 token,const Bytes& body) {
    auto out=words({(u32)body.size()+12,0,token});
    out.insert(out.end(),body.begin(),body.end());
    set(out,4,object_crc(out.data()+8,out.size()-8<1000000?out.size()-8:1000000));
    return out;
}
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: starsdb_context_verify [new-synthetic-fixture-directory]\n"; return 2; }
    size_t checks=0;
    const auto require=[&](bool ok,const char* why) {
        ++checks;
        if (!ok) std::cerr<<"serializer context: "<<why<<'\n';
        return ok;
    };
    const u8 standard[]={'1','2','3','4','5','6','7','8','9'};
    if (!require(object_crc(standard,9)==0xfc891918u && object_crc(nullptr,0)==0,"independent CRC-32/BZIP2 check vector and empty span")) return 1;
    // Independently literal wire object, including precomputed checksum over token + body.
    const Bytes literal{0,0,0,0x24, 0x5f,0xbe,0x5e,0x5e, 0x12,0x34,0x56,0x78,
        0x01,0xa8,0x0d,0x26, 0,0,0,0x10, 0,0,0,1, 0x03,0xd5,0x8b,0xf2, 0,0,0,4, 0x11,0x22,0x33,0x44};
    const Bytes index_bytes{0,0,0,1, 0,0,0,1, 0,0,0,1, 0x03,0xe4,0x35,0x3c, 0,0,0,1};
    std::string error;
    ContextIndex index;
    ClassObjects objects;
    if (!require(read_context_index(index_bytes,index,error) && index.version==1 && index.declared_objects==1 &&
        index.classes.size()==1 && index.classes[0].class_id==kStarRecordClass,"literal context index")) return 1;
    if (!require(read_class_objects(literal,index.classes[0],objects,error) && error.empty() && objects.objects.size()==1 &&
        objects.objects[0].token==0x12345678 && objects.objects[0].body_offset==12 && objects.objects[0].end==36 &&
        objects.objects[0].star && objects.objects[0].star->key==0x11223344u,"literal object length/CRC/token/frame")) return 1;
    const ObjectReference ref{0x12345678,kStarRecordClass};
    auto resolved=resolve_star_reference(ref,objects);
    if (!require(resolved.status==ReferenceResolution::StarFields && resolved.object_index==0 &&
                 objects.objects[resolved.object_index].star->key!=ref.token,"token is not mKey; class-scoped resolution")) return 1;
    // Link reader -> index -> class envelope -> StarRecord, without a fabricated catalogue producer.
    auto encoded_links=words({2,0,ref.token,ref.class_id});
    ReferenceList links;
    if (!require(read_reference_list(encoded_links,0,encoded_links.size(),links,error) && links.size()==2 &&
        resolve_star_reference(links[0],objects).status==ReferenceResolution::Null &&
        resolve_star_reference(links[1],objects).status==ReferenceResolution::StarFields,
        "wire list to indexed StarRecord structural integration")) return 1;
    if (!require(resolve_star_reference({0,0},objects).status==ReferenceResolution::Null &&
        resolve_star_reference({0x11223344,kStarRecordClass},objects).status==ReferenceResolution::MissingObject &&
        resolve_star_reference({ref.token,kStarRecordInterface},objects).status==ReferenceResolution::MissingClassStream,
        "null/missing/wrong class and interface distinct")) return 1;
    const auto reject_index=[&](const Bytes& bad) {
        ContextIndex sentinel; sentinel.version=999; sentinel.declared_objects=888; sentinel.classes={{77,66}};
        return !read_context_index(bad,sentinel,error) && !error.empty() && sentinel.version==999 &&
            sentinel.declared_objects==888 && sentinel.classes.size()==1 && sentinel.classes[0].class_id==77;
    };
    for (size_t n=0; n<index_bytes.size(); ++n)
        if (!require(reject_index(Bytes(index_bytes.begin(),index_bytes.begin()+n)),"index truncation/atomic output")) return 1;
    auto bad=index_bytes; bad.push_back(0);
    if (!require(reject_index(bad),"index trailing bytes")) return 1;
    for (const auto& malformed : std::vector<Bytes>{words({2,0,0}),words({1,0,1}),words({1,4097,0}),
            words({1,0,0xffffffffu}),words({1,2,0,77,0,77,0}),words({1,1,0,77,1}),
            words({1,2,1048576,77,1048576,88,1})})
        if (!require(reject_index(malformed),"index version/caps/duplicate/count-sum policy")) return 1;
    if (!require(read_context_index(words({1,0,0}),index,error) && index.classes.empty(),"explicit empty index")) return 1;
    auto unsorted=words({1,2,1,99,0,kStarRecordClass,1});
    if (!require(read_context_index(unsorted,index,error) && index.classes[0].class_id==99 && index.classes[1].count==1,
                 "preserve declared class order and empty class")) return 1;
    const auto reject_objects=[&](const Bytes& input,const ClassCount& expected=ClassCount{kStarRecordClass,1}) {
        ClassObjects sentinel; sentinel.class_id=777; ObjectBody object; object.token=888;
        sentinel.objects.push_back(object);
        return !read_class_objects(input,expected,sentinel,error) && !error.empty() && sentinel.class_id==777 &&
            sentinel.objects.size()==1 && sentinel.objects[0].token==888;
    };
    for (size_t n=0; n<literal.size(); ++n)
        if (!require(reject_objects(Bytes(literal.begin(),literal.begin()+n)),"object truncation/atomic output")) return 1;
    // Every single-byte change in this record is caught: length, stored CRC or token/body checksum.
    for (size_t i=0; i<literal.size(); ++i) {
        bad=literal; bad[i]^=1;
        if (!require(reject_objects(bad),"object single-byte mutation")) return 1;
    }
    for (u32 size : {0u,8u,11u,0xffffffffu}) {
        bad=literal; set(bad,0,size);
        if (!require(reject_objects(bad),"record total length bounds")) return 1;
    }
    bad=literal; bad.push_back(0);
    if (!require(reject_objects(bad),"class stream trailing bytes")) return 1;
    if (!require(reject_objects(literal,{kStarRecordClass,2}) && reject_objects(literal,{kStarRecordClass,1048577}),"object count/cap")) return 1;
    if (!require(read_class_objects({}, {kStarRecordClass,0},objects,error) && objects.objects.empty(),"explicit empty class")) return 1;
    if (!require(reject_objects(record(0,{}),{88,1}),"zero token is not an object identity")) return 1;
    auto repeated=literal; repeated.insert(repeated.end(),literal.begin(),literal.end());
    if (!require(reject_objects(repeated,{kStarRecordClass,2}),"duplicate body token within class")) return 1;
    const Bytes star_frame(literal.begin()+12,literal.end());
    auto next=record(0x76543210,star_frame);
    auto pair=literal; pair.insert(pair.end(),next.begin(),next.end());
    if (!require(read_class_objects(pair,{kStarRecordClass,2},objects,error) && objects.objects.size()==2 &&
        objects.objects[1].offset==36 && objects.objects[1].star && objects.objects[1].star->frame.offset==48 &&
        resolve_star_reference({0x76543210,kStarRecordClass},objects).object_index==1,
        "sequential objects use size not magic search; same mKey different tokens")) return 1;
    pair.back()^=1;
    if (!require(reject_objects(pair,{kStarRecordClass,2}),"bad second object leaves first unpublished")) return 1;
    auto wrong_frame=star_frame; set(wrong_frame,4,20); // frame exceeds envelope despite available next bytes
    auto crossing=record(123,wrong_frame); crossing.insert(crossing.end(),next.begin(),next.end());
    if (!require(reject_objects(crossing,{kStarRecordClass,2}),"frame cannot consume adjacent record")) return 1;
    wrong_frame=star_frame; wrong_frame[0]=0;
    if (!require(reject_objects(record(123,wrong_frame)),"CRC-correct invalid frame tag")) return 1;
    wrong_frame=star_frame; set(wrong_frame,16,3);
    if (!require(reject_objects(record(123,wrong_frame)),"CRC-correct invalid field directory")) return 1;
    auto opaque=record(ref.token,Bytes{1,2,3});
    if (!require(read_class_objects(opaque,{88,1},objects,error) && !objects.objects[0].star &&
        resolve_star_reference({ref.token,88},objects).status==ReferenceResolution::UnsupportedClass &&
        resolve_star_reference(ref,objects).status==ReferenceResolution::MissingClassStream,
        "opaque unsupported class, same token not a StarRecord")) return 1;
    // GA CRC only covers the first min(total-8,1M) bytes. Tail corruption is not detected by it.
    Bytes large_body(1000010,0x5a);
    auto large=record(555,large_body);
    if (!require(read_class_objects(large,{88,1},objects,error),"large record bounded CRC")) return 1;
    large[8+1000000]^=1;
    if (!require(read_class_objects(large,{88,1},objects,error),"CRC boundary excludes unchecked tail; not full integrity")) return 1;
    large[8+999999]^=1;
    if (!require(reject_objects(large,{88,1}),"last checked byte included by CRC")) return 1;
    if (argc==2) {
        const std::filesystem::path dir=argv[1];
        if (!std::filesystem::create_directory(dir)) { std::cerr<<"requires NEW synthetic fixture directory\n"; return 2; }
        for (const auto& output : std::vector<std::pair<const char*,Bytes>>{
                 {"SYNTHETIC-context-index.bin",index_bytes},{"SYNTHETIC-star-class.bin",literal}}) {
            std::ofstream file(dir/output.first,std::ios::binary);
            file.write((const char*)output.second.data(),(std::streamsize)output.second.size());
            if (!file) { std::cerr<<"failed writing requested synthetic stream\n"; return 1; }
        }
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC decoded streams (not physical saves)\n";
    }
    std::cout<<checks<<" constructed context-index/object-envelope/CRC/lookup checks PASS; no real-save/fixup/runtime claim\n";
    return 0;
}
