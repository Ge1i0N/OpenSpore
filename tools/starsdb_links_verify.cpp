// Constructed instruction-derived wire payloads only. No real save or original game execution.
#include "engine/starsdb_links.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>

using namespace os;
using namespace os::starsdb;
using Bytes=std::vector<u8>;
using Field=std::pair<u32,Bytes>;
static void put(Bytes& out,u32 value) {
    for (int shift=24; shift>=0; shift-=8) out.push_back((u8)(value>>shift));
}
static Bytes words(std::initializer_list<u32> values) { Bytes out; for (u32 v : values) put(out,v); return out; }
static Bytes frame(const std::vector<Field>& fields) {
    u32 size=4+(u32)fields.size()*8;
    for (const auto& f : fields) size+=(u32)f.second.size();
    Bytes out=words({serialized::kObjectTag,size,(u32)fields.size()});
    for (const auto& f : fields) { put(out,f.first); put(out,(u32)f.second.size()); }
    for (const auto& f : fields) out.insert(out.end(),f.second.begin(),f.second.end());
    return out;
}
static Bytes wide(const std::u16string& s) {
    Bytes out=words({(u32)s.size()});
    for (char16_t c : s) { out.push_back((u8)c); out.push_back((u8)(c>>8)); }
    return out;
}
static bool same(const ReferenceList& a,const ReferenceList& b) {
    if (a.size()!=b.size()) return false;
    for (size_t i=0; i<a.size(); ++i) if (a[i].token!=b[i].token || a[i].class_id!=b[i].class_id) return false;
    return true;
}
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: starsdb_links_verify [new-synthetic-fixture-directory]\n"; return 2; }
    size_t checks=0;
    const auto require=[&](bool ok,const char* why) {
        ++checks;
        if (!ok) std::cerr<<"starsdb wire subset: "<<why<<'\n';
        return ok;
    };
    std::string error;
    // Literal bytes do not depend on the BE writer helper. Null consumes only FOUR bytes.
    const Bytes literal{0,0,0,4, 0,0,0,0, 0x12,0x34,0x56,0x78, 0x03,0xe4,0x35,0x3c,
                        0x12,0x34,0x56,0x78, 0x03,0xe4,0x35,0x3c, 0,0,0,1, 0,0,0,0};
    ReferenceList refs;
    const ReferenceList expected{{0,0},{0x12345678,kStarRecordClass},{0x12345678,kStarRecordClass},{1,0}};
    if (!require(read_reference_list(literal,0,literal.size(),refs,error) && error.empty() &&
                 same(refs,expected) && refs[0].is_null() && !refs[3].is_null(),"literal variable-width refs/order/duplicates/unknown class")) return 1;
    const auto reject_list=[&](const Bytes& bad,size_t offset,size_t size) {
        ReferenceList sentinel{{99,88}};
        return !read_reference_list(bad,offset,size,sentinel,error) && !error.empty() && same(sentinel,{{99,88}});
    };
    for (size_t n=0; n<literal.size(); ++n)
        if (!require(reject_list(Bytes(literal.begin(),literal.begin()+n),0,n),"list truncation/atomic rejection")) return 1;
    auto extra=literal; extra.push_back(0);
    if (!require(reject_list(extra,0,extra.size()),"list trailing bytes")) return 1;
    if (!require(reject_list(literal,(size_t)-1,4) && reject_list(literal,0,(size_t)-1),"list offset/size overflow")) return 1;
    auto count=words({0xffffffffu});
    if (!require(reject_list(count,0,count.size()),"list allocation cap")) return 1;
    auto empty=words({0});
    if (!require(read_reference_list(empty,0,empty.size(),refs,error) && refs.empty(),"observed empty list")) return 1;
    auto embedded=words({0xdeadbeef}); embedded.insert(embedded.end(),literal.begin(),literal.end());
    if (!require(read_reference_list(embedded,4,literal.size(),refs,error) && same(refs,expected),"explicit list slice")) return 1;

    auto grid_bytes=words({3,0}); // empty cell then 4 refs then one null
    grid_bytes.insert(grid_bytes.end(),literal.begin(),literal.end()); put(grid_bytes,1); put(grid_bytes,0);
    ReferenceGrid grid;
    if (!require(read_reference_grid(grid_bytes,0,grid_bytes.size(),grid,error) && grid.size()==3 &&
                 grid[0].empty() && same(grid[1],expected) && grid[2].size()==1 && grid[2][0].is_null(),"grid cell order/nested counts")) return 1;
    const auto reject_grid=[&](const Bytes& bad) {
        ReferenceGrid sentinel{{{99,88}}};
        return !read_reference_grid(bad,0,bad.size(),sentinel,error) && !error.empty() &&
            sentinel.size()==1 && same(sentinel[0],{{99,88}});
    };
    for (size_t n=0; n<grid_bytes.size(); ++n)
        if (!require(reject_grid(Bytes(grid_bytes.begin(),grid_bytes.begin()+n)),"grid truncation/atomic rejection")) return 1;
    extra=grid_bytes; extra.push_back(0);
    if (!require(reject_grid(extra),"grid trailing bytes")) return 1;
    if (!require(reject_grid(words({(u32)kMaxGridCells+1})) && reject_grid(words({2,0})),"grid cell count cap/minimum bytes")) return 1;
    if (!require(read_reference_grid(empty,0,4,grid,error) && grid.empty(),"observed empty grid")) return 1;
    auto many_cells=words({(u32)kMaxGridCells}); many_cells.resize(4+4*kMaxGridCells,0);
    if (!require(read_reference_grid(many_cells,0,many_cells.size(),grid,error) && grid.size()==kMaxGridCells,"exact grid cell cap")) return 1;
    auto many=words({(u32)kMaxReferences}); many.resize(4+4*kMaxReferences,0);
    if (!require(read_reference_list(many,0,many.size(),refs,error) && refs.size()==kMaxReferences,"exact reference cap")) return 1;
    auto budget=words({2}); budget.insert(budget.end(),many.begin(),many.end());
    auto second=words({1,0}); budget.insert(budget.end(),second.begin(),second.end());
    if (!require(reject_grid(budget),"aggregate grid reference cap across cells")) return 1;

    const auto manager=frame({{0x03fabad0,grid_bytes},{0x03ea8676,literal},{0x03ea8679,empty},
                              {0x055ceb62,empty},{0x0563c450,empty},{0xdeadbeef,{1,2,3}}});
    ManagerLinks links;
    if (!require(inspect_manager_links(manager,0,links,error) && links.starters && same(*links.starters,expected) &&
                 links.saved && links.saved->empty() && links.black_holes && links.possible_starts &&
                 links.grid && links.grid->size()==3,"manager dispatch/empty vs absent/opaque unknown")) return 1;
    auto absent=frame({{0x03ea8676,{}}});
    if (!require(inspect_manager_links(absent,0,links,error) && !links.starters && !links.saved && !links.grid,"zero-size unknown is not empty-ready")) return 1;
    auto duplicate=frame({{0x03ea8676,empty},{0x03ea8676,{}},{0x03ea8676,literal}});
    if (!require(inspect_manager_links(duplicate,0,links,error) && links.starters && same(*links.starters,expected),"last nonzero duplicate manager field")) return 1;
    const auto invalid_manager=frame({{0x03ea8676,literal},{0x03ea8679,words({1,1})}});
    links.saved=ReferenceList{{77,66}};
    if (!require(!inspect_manager_links(invalid_manager,0,links,error) && links.saved && same(*links.saved,{{77,66}}),"manager atomicity after earlier valid list")) return 1;

    const auto stamp=words({59,58,23,31,11,126,4,364,0});
    std::vector<Field> fields{{0x03d58bf2,words({0x11223344})},{0x03d58bf1,words({0x3fc00000,0xc0100000,0x80000000})},
        {0x03e95e04,words({2})},{0x03d58bf6,words({5})},{0x03d58bf4,wide(std::u16string{u'A',0,u'\u0418',0xd800})},
        {0x03d58bf3,words({0x80})},{0x03d58bf5,words({0x01020304,0x05060708,0x090a0b0c})},
        {0x03d58bf7,words({123})},{0x03f1389b,words({0xffffffffu})},{0x03f1389a,words({99})},
        {0x03f1389c,stamp},{0x03f1389d,words({37})},{0x03f1389e,words({3})},{0x051a1188,{255}},
        {0xdeadbeef,{1,2,3}}};
    const auto star_bytes=frame(fields);
    StarRecord star;
    if (!require(inspect_star_record(star_bytes,0,star,error) && star.frame.fields.size()==15 &&
        star.key==0x11223344u && star.type==2u && star.tech_level==5u && star.flags==0x80u &&
        star.starter_world_id==123u && star.empire_id==0xffffffffu && star.last_observed_time==99u &&
        star.saved_game_version_major==37u && star.saved_game_version_minor==3u && star.planet_count==255u &&
        star.position && (*star.position)[0]==1.5f && (*star.position)[1]==-2.25f &&
        std::bit_cast<u32>((*star.position)[2])==0x80000000u && star.name && star.name->size()==4 &&
        (*star.name)[1]==0 && (*star.name)[2]==0x0418 && (*star.name)[3]==0xd800 &&
        star.citizen_species_key && (*star.citizen_species_key)[0]==0x090a0b0cu &&
        (*star.citizen_species_key)[2]==0x01020304u && star.saved_game_timestamp &&
        (*star.saved_game_timestamp)[5]==126u,"14 StarRecord fields/mixed endian/float bits/resource order")) return 1;
    const auto reject_star=[&](const Bytes& bad,size_t offset=0) {
        StarRecord sentinel; sentinel.key=0xabcdefu; sentinel.frame.end=777;
        return !inspect_star_record(bad,offset,sentinel,error) && !error.empty() &&
            sentinel.key==0xabcdefu && sentinel.frame.end==777;
    };
    for (size_t n=0; n<star_bytes.size(); ++n)
        if (!require(reject_star(Bytes(star_bytes.begin(),star_bytes.begin()+n)),"StarRecord truncation/atomic rejection")) return 1;
    // Every fixed-size known field gets a wrong-size negative case, not only the first scalar.
    for (const auto& field : fields) {
        if (field.first==0xdeadbeef || field.first==0x03d58bf4) continue;
        auto wrong=field.second; wrong.push_back(0);
        if (!require(reject_star(frame({{field.first,wrong}})),"known field wrong size")) return 1;
    }
    for (const auto& bad_string : std::vector<Bytes>{{1,2},{0xff,0xff,0xff,0xff},{0,0,0,1,0x41}})
        if (!require(reject_star(frame({{0x03d58bf4,bad_string}})),"StarRecord bad UTF16 field")) return 1;
    auto nonfinite=frame({{0x03d58bf1,words({0x7fc12345,0x7f800000,0xff800000})}});
    if (!require(inspect_star_record(nonfinite,0,star,error) && star.position &&
        std::bit_cast<u32>((*star.position)[0])==0x7fc12345u && !star.key && !star.flags,"preserve NaN/Inf words, no fabricated defaults")) return 1;
    auto repeated=frame({{0x03d58bf2,words({1})},{0x03d58bf2,{}},{0x03d58bf2,words({2})}});
    if (!require(inspect_star_record(repeated,0,star,error) && star.key==2u,"last nonzero StarRecord duplicate")) return 1;
    auto offset=words({0xaabbccdd}); offset.insert(offset.end(),star_bytes.begin(),star_bytes.end()); offset.push_back(1);
    if (!require(inspect_star_record(offset,4,star,error) && star.frame.end==offset.size()-1 && star.key==0x11223344u,
                 "explicit record slice, trailing bytes unvalidated")) return 1;
    if (!require(reject_star(star_bytes,(size_t)-1),"StarRecord offset overflow")) return 1;
    if (argc==2) {
        const std::filesystem::path dir=argv[1];
        if (!std::filesystem::create_directory(dir)) { std::cerr<<"requires a NEW synthetic fixture directory\n"; return 2; }
        auto prefix=frame({}); prefix.insert(prefix.end(),manager.begin(),manager.end());
        for (const auto& output : std::vector<std::pair<const char*,Bytes>>{
                 {"SYNTHETIC-manager-prefix.bin",prefix},{"SYNTHETIC-StarRecord-frame.bin",star_bytes}}) {
            std::ofstream file(dir/output.first,std::ios::binary);
            file.write((const char*)output.second.data(),(std::streamsize)output.second.size());
            if (!file) { std::cerr<<"failed writing requested synthetic fixture\n"; return 1; }
        }
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC fixtures (not original saves)\n";
    }
    std::cout<<checks<<" constructed reference/grid/StarRecord checks PASS; no real-save/object-resolution claim\n";
    return 0;
}
