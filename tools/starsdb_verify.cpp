// Constructed fixtures from GA instruction-level framing. NOT real save files or original replay.
#include "engine/starsdb.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

using namespace os;
using Field=std::pair<u32,std::vector<u8>>;
static void put(std::vector<u8>& out,u32 value) {
    for (int shift=24; shift>=0; shift-=8) out.push_back((u8)(value>>shift));
}
static std::vector<u8> word(u32 value) { std::vector<u8> out; put(out,value); return out; }
static std::vector<u8> frame(const std::vector<Field>& fields) {
    std::vector<u8> out;
    u32 size=4+(u32)fields.size()*8;
    for (const auto& f : fields) size+=(u32)f.second.size();
    put(out,serialized::kObjectTag); put(out,size); put(out,(u32)fields.size());
    for (const auto& f : fields) { put(out,f.first); put(out,(u32)f.second.size()); }
    for (const auto& f : fields) out.insert(out.end(),f.second.begin(),f.second.end());
    return out;
}
static std::vector<u8> wide(const std::u16string& text) {
    auto out=word((u32)text.size());
    for (char16_t c : text) { out.push_back((u8)c); out.push_back((u8)(c>>8)); }
    return out;
}
static void setword(std::vector<u8>& out,size_t at,u32 value) {
    for (size_t i=0;i<4;++i) out[at+i]=(u8)(value>>(24-8*i));
}
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: starsdb_verify [new-synthetic-fixture-path]\n"; return 2; }
    size_t checks=0;
    const auto require=[&](bool ok,const char* why) {
        ++checks;
        if (!ok) std::cerr<<"serialized prefix: "<<why<<'\n';
        return ok;
    };
    // Literal known-byte fixture independent of the frame writer helper.
    const std::vector<u8> literal{
        0x01,0xa8,0x0d,0x26, 0,0,0,0x10, 0,0,0,1, 0x03,0xf1,0x40,0xb6, 0,0,0,4, 0,0,0,0x25,
        0x01,0xa8,0x0d,0x26, 0,0,0,4, 0,0,0,0};
    starsdb::Prefix result;
    std::string error;
    if (!require(starsdb::inspect_prefix(literal,result,error) && error.empty() &&
                 result.header.saved_game_version_on_save==37u && result.metadata.end==24 &&
                 result.consumed==36 && !result.header.saved_game_version_on_generate,"literal BE framing")) return 1;
    std::vector<u8> tm;
    for (u32 value : {59u,58u,23u,31u,11u,126u,4u,364u,0u}) put(tm,value);
    const auto first=frame({{0x03f140b6,word(37)},{0x03f140b7,word(36)},
        {0x03f140b8,wide(u"3.1.0.29\u0418")},{0x03f140b9,wide(u"")},
        {0x03f140ba,tm},{0x03f140bb,tm},{0xdeadbeef,{1,2,3}}});
    const auto second=frame({{0x03ea8676,{1,2,3,4}},{0x03ea8679,{}},{0xaaaaaaaa,{5,6}}});
    auto bytes=first; bytes.insert(bytes.end(),second.begin(),second.end());
    const auto consumed=bytes.size();
    bytes.insert(bytes.end(),{0xde,0xad,0xff}); // deliberately opaque trailing data, not validated
    if (!require(starsdb::inspect_prefix(bytes,result,error) && result.consumed==consumed &&
        result.remaining==3 && result.manager.fields.size()==3 &&
        result.header.saved_game_version_on_generate==36u &&
        result.header.build_version_on_save==std::u16string(u"3.1.0.29\u0418") &&
        result.header.build_version_on_generate==std::u16string{} &&
        result.header.timestamp_on_save && (*result.header.timestamp_on_save)[0]==59 &&
        (*result.header.timestamp_on_save)[5]==126,"all six metadata fields/mixed endian/tail")) return 1;
    if (!require(result.metadata.fields.back().id==0xdeadbeef && result.metadata.fields.back().size==3 &&
        result.manager.fields[1].size==0 && starsdb::manager_field_name(0x03ea8676) &&
        !starsdb::manager_field_name(0xaaaaaaaa),"opaque fields/zero sizes/names")) return 1;
    const auto reject=[&](const std::vector<u8>& bad) {
        starsdb::Prefix sentinel; sentinel.consumed=12345; sentinel.header.saved_game_version_on_save=987;
        return !starsdb::inspect_prefix(bad,sentinel,error) && !error.empty() &&
            sentinel.consumed==12345 && sentinel.header.saved_game_version_on_save==987u;
    };
    for (size_t n=0;n<consumed;++n)
        if (!require(reject(std::vector<u8>(bytes.begin(),bytes.begin()+n)),"truncation changed output/accepted")) return 1;
    auto bad=bytes; bad[0]=0x26;
    if (!require(reject(bad),"wrong tag")) return 1;
    for (u32 length : {0u,3u,0xffffffffu}) {
        bad=bytes; setword(bad,4,length);
        if (!require(reject(bad),"impossible/wrapped length")) return 1;
    }
    bad=bytes; setword(bad,8,129);
    if (!require(reject(bad),"field-table limit")) return 1;
    bad=bytes; setword(bad,16,0xffffffffu);
    if (!require(reject(bad),"field size overflow")) return 1;
    bad=bytes; setword(bad,16,3);
    if (!require(reject(bad),"underconsumed payload")) return 1;
    bad=bytes; bad[first.size()]=0xff;
    if (!require(reject(bad),"second frame tag/atomic output")) return 1;
    for (const auto& payload : std::vector<std::vector<u8>>{{0,0,0,1,0x41},{0xff,0xff,0xff,0xff},
                                                          {0,0,0,1,0x41,0,0}}) {
        bad=frame({{0x03f140b8,payload}}); bad.insert(bad.end(),second.begin(),second.end());
        if (!require(reject(bad),"malformed string size")) return 1;
    }
    bad=frame({{0x03f140b6,{1,2}},{0x03f140ba,tm}}); bad.insert(bad.end(),second.begin(),second.end());
    if (!require(reject(bad),"nonword version size")) return 1;
    bad=frame({{0x03f140ba,{1,2,3,4}}}); bad.insert(bad.end(),second.begin(),second.end());
    if (!require(reject(bad),"short timestamp")) return 1;
    auto repeated=frame({{0x03f140b6,word(1)},{0x03f140b6,{}},{0x03f140b6,word(2)}});
    repeated.insert(repeated.end(),second.begin(),second.end());
    if (!require(starsdb::inspect_prefix(repeated,result,error) &&
                 result.header.saved_game_version_on_save==2u,"last nonzero duplicate field")) return 1;
    serialized::Frame generic;
    std::vector<Field> many(128,Field{123,{}});
    auto edge=frame(many);
    if (!require(serialized::parse_frame(edge,0,serialized::kObjectTag,generic,error) &&
                 generic.fields.size()==128,"exact table limit")) return 1;
    generic.tag=0x12345678;
    if (!require(!serialized::parse_frame(edge,edge.size()+1,serialized::kObjectTag,generic,error) &&
                 generic.tag==0x12345678,"invalid offset/unchanged frame")) return 1;
    if (!require(!serialized::parse_frame(edge,(size_t)-1,serialized::kObjectTag,generic,error),"offset wrap")) return 1;
    // A valid embedded frame is not automatically a complete file; offset is explicit.
    auto embedded=std::vector<u8>{0,1,2}; embedded.insert(embedded.end(),edge.begin(),edge.end());
    if (!require(serialized::parse_frame(embedded,3,serialized::kObjectTag,generic,error) &&
                 generic.end==embedded.size(),"explicit embedded offset")) return 1;
    if (argc==2) {
        if (std::filesystem::exists(argv[1])) { std::cerr<<"synthetic destination already exists\n"; return 2; }
        std::ofstream output(argv[1],std::ios::binary);
        output.write((const char*)bytes.data(),(std::streamsize)bytes.size());
        if (!output) { std::cerr<<"failed writing requested synthetic fixture\n"; return 1; }
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC fixture (not original save data): "<<argv[1]<<'\n';
    }
    std::cout<<checks<<" constructed serialized-prefix checks PASS (not real stars.db corpus validation)\n";
    return 0;
}
