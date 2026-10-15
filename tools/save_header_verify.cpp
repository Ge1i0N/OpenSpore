// Controlled serializer bytes; never real saves or original runtime observations.
#include "engine/save_header.h"
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>
using namespace os;
using namespace os::save;
using Bytes=std::vector<u8>;
using Field=std::pair<u32,Bytes>;
static void put(Bytes& out,u32 value) { for (int s=24;s>=0;s-=8) out.push_back((u8)(value>>s)); }
static Bytes words(std::initializer_list<u32> values) { Bytes out; for (auto v:values) put(out,v); return out; }
static Bytes frame(const std::vector<Field>& fields) {
    u32 length=4+8*(u32)fields.size(); for (const auto& f:fields) length+=(u32)f.second.size();
    Bytes out=words({serialized::kObjectTag,length,(u32)fields.size()});
    for (const auto& f:fields) { put(out,f.first); put(out,(u32)f.second.size()); }
    for (const auto& f:fields) out.insert(out.end(),f.second.begin(),f.second.end());
    return out;
}
static void le(Bytes& out,size_t at,u32 v) { for (size_t i=0;i<4;++i) out[at+i]=(u8)(v>>(8*i)); }
static Bytes package(const Bytes& root) {
    // Independent synthetic DBPF kind4: one raw root resource, no context/class index needed.
    const size_t off=0x60+root.size(); Bytes out(off+36,0);
    le(out,0,0x46504244); le(out,4,3); le(out,0x24,1); le(out,0x2c,36); le(out,0x3c,3); le(out,0x40,(u32)off);
    std::copy(root.begin(),root.end(),out.begin()+0x60);
    le(out,off,4); le(out,off+8,kRoot); le(out,off+12,kRoot); le(out,off+16,kRoot);
    le(out,off+20,0x60); le(out,off+24,(u32)root.size()); le(out,off+28,(u32)root.size()); le(out,off+32,0x10000);
    return out;
}
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: save_header_verify [new-synthetic-save-header-DBPF]\n"; return 2; }
    size_t checks=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    // Literal schema IDs, endian and two words independent of the helper encoder.
    Bytes literal{1,0xa8,0x0d,0x26,0,0,0,0x1c,0,0,0,2,
        4,0x16,0x2d,0x88,0,0,0,4,4,0x16,0x2d,0x89,0,0,0,4,0,0,0,0x25,0,0,0,3};
    FileHeader header; HeaderProbe probe; std::string error;
    if (!check(read_header(literal,header,error) && header_probe(header,probe,error) && probe.predicate &&
               probe.words.first==37 && probe.words.second==3 && !header.unvalidated_tail,"literal header major/minor")) return 1;
    const auto reject=[&](const Bytes& bad) {
        FileHeader sentinel; sentinel.version_major=777; sentinel.unvalidated_tail=888;
        return !read_header(bad,sentinel,error) && !error.empty() && sentinel.version_major==777u && sentinel.unvalidated_tail==888;
    };
    for (size_t n=0;n<literal.size();++n)
        if (!check(reject(Bytes(literal.begin(),literal.begin()+n)),"every header truncation atomic")) return 1;
    std::vector<Field> all{{0x04162d88,words({37})},{0x04162d89,words({3})},{0x04162d8c,words({6})},
        {0x04162d8b,words({59,58,23,31,11,126,4,364,0})},{0x064be534,words({3,2,1})},
        {0x04162d8a,{0xde,0xad}},{0x064bc1cf,{1}},{0xdeadbeef,{7,8,9}}};
    auto bytes=frame(all); bytes.insert(bytes.end(),{0xaa,0xbb,0xcc});
    if (!check(read_header(bytes,header,error) && header.game_mode==6u && header.timestamp &&
        (*header.timestamp)[5]==126 && header.current_planet_key && (*header.current_planet_key)[0]==1 &&
        (*header.current_planet_key)[2]==3 && header.unvalidated_tail==3 && header.frame.fields.size()==8,
        "timestamp/mode/reversed resource triple/opaque map/snapshot/tail")) return 1;
    for (size_t i=0;i<5;++i) {
        auto fields=all; fields[i].second.push_back(0);
        if (!check(reject(frame(fields)),"known field exact size atomic")) return 1;
    }
    for (u32 major:{0u,1u,32u,33u,36u,37u,0x80000000u,0xffffffffu}) {
        for (u32 minor:{0u,3u,4u,0xffffffffu}) {
            if (!check(read_header(frame({{0x04162d88,words({major})},{0x04162d89,words({minor})}}),header,error) &&
                header_probe(header,probe,error) && probe.predicate==(major>=33u) && probe.words.first==major &&
                probe.words.second==minor,"unsigned major guard, not signed/version equality")) return 1;
            gge::CatalogInput input; input.saved_status=input.candidate_status=gge::CatalogAvailability::Available;
            gge::SavedGameMetadata saved; saved.probe_result=probe.predicate; saved.header=probe.words;
            saved.stage=5; saved.source_flags=0x80; saved.star.name=u"controlled";
            input.saved.push_back(saved); std::vector<gge::GameEntry> entries;
            const auto expected=major>=33u ? (major==37 && minor==3?gge::SaveStatus::HeaderMatches:gge::SaveStatus::HeaderDiffers)
                : ((major||minor)?gge::SaveStatus::ProbeFalseNonzeroHeader:gge::SaveStatus::ProbeFalseZeroHeader);
            if (!check(gge::build_catalog(input,entries,error) && entries.size()==1 && entries[0].save_status==expected &&
                entries[0].stage==0,"actual decoded header to controlled typed GGE predicate (flags override)")) return 1;
        }
    }
    for (const auto& omitted:std::vector<Bytes>{frame({}),frame({{0x04162d88,words({37})}}),
         frame({{0x04162d89,words({3})}}),frame({{0x04162d88,{}},{0x04162d89,words({3})}}),
         frame({{0x03f1389d,words({37})},{0x03f1389e,words({3})}}), // StarRecord versions are NOT file versions
         frame({{0x03f140b6,words({37})},{0x03f140b7,words({3})}})}) {
        probe.predicate=true; probe.words={777,888};
        if (!check(read_header(omitted,header,error) && !header_probe(header,probe,error) && !error.empty() &&
                   probe.predicate && probe.words.first==777 && probe.words.second==888,"omitted/other schema versions stay unknown not zero")) return 1;
    }
    auto repeat=frame({{0x04162d88,words({1})},{0x04162d88,{}},{0x04162d88,words({37})},{0x04162d89,words({3})}});
    if (!check(read_header(repeat,header,error) && header_probe(header,probe,error) && probe.words.first==37,"last nonzero duplicate word")) return 1;
    starsdb::ExplicitDbpfReader reader;
    auto physical=package(bytes);
    if (!check(reader.open_from_memory(physical,error) && read_selected_header(reader,header,error) &&
        header_probe(header,probe,error) && probe.words.first==37 && header.unvalidated_tail==3,
        "explicit DBPF -> first save header, no fabricated context required")) return 1;
    le(physical,0x60+bytes.size()+8,0); // root type different -> missing, not observed false/zero
    header.version_major=999;
    if (!check(reader.open_from_memory(physical,error) && !read_selected_header(reader,header,error) && header.version_major==999u,
               "missing root unchanged, not automatic probe-false")) return 1;
    if (argc==2) {
        if (std::filesystem::exists(argv[1])) { std::cerr<<"synthetic destination exists\n"; return 2; }
        const auto synthetic=package(bytes);
        std::ofstream file(argv[1],std::ios::binary);
        file.write((const char*)synthetic.data(),(std::streamsize)synthetic.size());
        if (!file) { std::cerr<<"synthetic fixture write failed\n"; return 1; }
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC header DBPF (not original save data)\n";
    }
    std::cout<<checks<<" constructed save-header/schema/probe/GGE-boundary checks PASS; no real save/path discovery\n";
    return 0;
}
