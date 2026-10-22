// Instruction-derived constructed fixtures only. No original saves or original game services.
#include "engine/gge_saved_adapter.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace os;
using Bytes=std::vector<u8>;
using Field=std::pair<u32,Bytes>;
static void put(Bytes& out,u32 v) { for (int s=24;s>=0;s-=8) out.push_back((u8)(v>>s)); }
static Bytes words(std::initializer_list<u32> values) { Bytes out; for (auto v:values) put(out,v); return out; }
static void append(Bytes& out,const Bytes& b) { out.insert(out.end(),b.begin(),b.end()); }
static Bytes frame(const std::vector<Field>& fields) {
    u32 length=4+8*(u32)fields.size(); for (const auto& f:fields) length+=(u32)f.second.size();
    auto out=words({serialized::kObjectTag,length,(u32)fields.size()});
    for (const auto& f:fields) { put(out,f.first); put(out,(u32)f.second.size()); }
    for (const auto& f:fields) append(out,f.second);
    return out;
}
static Bytes wide(const std::u16string& s) {
    auto out=words({(u32)s.size()}); for (char16_t c:s) { out.push_back((u8)c); out.push_back((u8)(c>>8)); } return out;
}
static Bytes record(const std::vector<Field>& fields) { auto out=words({0,0,0,0}); append(out,frame(fields)); return out; }
static Bytes named(const std::u16string& s,u32 flags=0) {
    return record({{0x04164f8e,words({123})},{0x03d58b9a,wide(s)},{0x03d58b99,words({flags})}});
}
static void le(Bytes& b,size_t at,u32 v) { for (size_t i=0;i<4;++i) b[at+i]=(u8)(v>>(i*8)); }
static Bytes package(const std::vector<std::pair<pkg::ResourceKey,Bytes>>& items) {
    size_t offset=0x60; for (const auto& item:items) offset+=item.second.size();
    Bytes out(offset+8+28*items.size(),0);
    le(out,0,0x46504244); le(out,4,3); le(out,0x24,(u32)items.size()); le(out,0x2c,(u32)(8+28*items.size()));
    le(out,0x3c,3); le(out,0x40,(u32)offset); le(out,offset,4);
    size_t data=0x60;
    for (size_t i=0;i<items.size();++i) {
        const auto& item=items[i]; const size_t at=offset+8+i*28;
        le(out,at,item.first.type); le(out,at+4,item.first.group); le(out,at+8,item.first.instance); le(out,at+12,(u32)data);
        le(out,at+16,(u32)item.second.size()); le(out,at+20,(u32)item.second.size()); le(out,at+24,0x10000);
        std::copy(item.second.begin(),item.second.end(),out.begin()+data); data+=item.second.size();
    }
    return out;
}
int main(int argc,char** argv) {
    if (argc>2) { std::cerr<<"usage: planet_record_verify [new-synthetic-planet-DBPF]\n"; return 2; }
    size_t checks=0;
    const auto check=[&](bool ok,const char* why) { ++checks; if (!ok) std::cerr<<why<<'\n'; return ok; };
    planet::Record parsed; std::string error;
    const Bytes literal{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        1,0xa8,0xd,0x26,0,0,0,0x14,0,0,0,1,3,0xd5,0x8b,0x9a,0,0,0,8,0,0,0,2,0x49,0,0x6f,0};
    if (!check(planet::read_record(literal,parsed,error) && parsed.name==std::u16string(u"Io") && parsed.frames_walked==1 &&
               parsed.fields.offset==16 && !parsed.flags && !parsed.unvalidated_tail,"independent literal planet prefix/name framing")) return 1;
    std::vector<Field> fields{{0x04164f8e,words({0x11223344})},{0x03d58b9a,wide(std::u16string{u'G',0,u'\u0418'})},
        {0x03d58b9e,words({2})},{0x03d58b9f,words({5})},{0x03d58b99,words({0x100})},
        {0x03d58b9b,words({3,2,1})},{0xdeadbeef,{1,2,3}}};
    auto rich=words({1,1}); // one civilization, one city
    rich.resize(rich.size()+28,0x33); append(rich,frame({}));
    put(rich,1); append(rich,frame({})); append(rich,frame({})); // one vehicle and civilization frame
    for (int i=0;i<3;++i) { put(rich,1); append(rich,frame({})); }
    append(rich,frame(fields)); const size_t consumed=rich.size();
    rich.push_back(0xff);
    if (!check(planet::read_record(rich,parsed,error) && parsed.fields.offset==124 && parsed.frames_walked==7 &&
        parsed.civilizations==1 && parsed.tribes==1 && parsed.events==1 && parsed.commodity_nodes==1 &&
        parsed.key==0x11223344u && parsed.type==2u && parsed.tech_level==5u && parsed.flags==0x100u &&
        parsed.generated_terrain_key && (*parsed.generated_terrain_key)[0]==1 && (*parsed.generated_terrain_key)[2]==3 &&
        parsed.name && parsed.name->size()==3 && (*parsed.name)[1]==0 && (*parsed.name)[2]==0x0418 && parsed.unvalidated_tail==1,
        "all nested collection routes + six fields + opaque tail")) return 1;
    const auto reject=[&](const Bytes& b) {
        planet::Record sentinel; sentinel.key=987; sentinel.unvalidated_tail=654;
        return !planet::read_record(b,sentinel,error) && !error.empty() && sentinel.key==987u && sentinel.unvalidated_tail==654;
    };
    for (size_t n=0;n<consumed;++n)
        if (!check(reject(Bytes(rich.begin(),rich.begin()+n)),"every prefix/field truncation atomic")) return 1;
    for (size_t i=0;i<6;++i) { auto f=fields; f[i].second.push_back(0);
        if (!check(reject(record(f)),"known field wrong size")) return 1;
    }
    for (const auto& bad:std::vector<Bytes>{words({4097}),words({1,4096}),words({1,0,4096}),
        words({0,4097}),words({0,0,4097}),words({0,0,0,4097}),record({{0x03d58b9a,words({0xffffffffu})}})})
        if (!check(reject(bad),"node budgets and invalid name count")) return 1;
    auto duplicate=record({{0x03d58b99,words({9})},{0x03d58b99,{}},{0x03d58b99,words({3})}});
    if (!check(planet::read_record(duplicate,parsed,error) && parsed.flags==3u,"last nonzero duplicate, missing name stays unknown")) return 1;
    auto large_name=record({{0x03d58b9a,wide(std::u16string(4096,u'x'))}});
    if (!check(planet::read_record(large_name,parsed,error) && parsed.name->size()==4096,"exact text cap")) return 1;
    if (!check(reject(record({{0x03d58b9a,wide(std::u16string(4097,u'x'))}})),"text over cap")) return 1;
    auto limit=words({0,4096});
    for (size_t i=0;i<4096;++i) append(limit,frame({}));
    put(limit,0); put(limit,0); append(limit,frame({}));
    if (!check(planet::read_record(limit,parsed,error) && parsed.tribes==4096 && parsed.frames_walked==4097,
               "exact aggregate node cap")) return 1;
    // Frame magic in the raw city prefix cannot redirect the sequential reader.
    auto magic=rich; const auto fake=frame({}); std::copy(fake.begin(),fake.end(),magic.begin()+8);
    if (!check(planet::read_record(magic,parsed,error) && parsed.fields.offset==124,"city prefix is not scanned for magic")) return 1;

    starsdb::StarRecord star;
    if (!check(starsdb::inspect_star_record(frame({{0x03d58bf2,words({0x11223344})},
        {0x03d58bf1,words({0x3f800000,0x40000000,0x40400000})},{0x03d58bf6,words({4})},
        {0x03d58bf3,words({0x40})},{0x051a1188,{3}},{0x03d58bf4,wide(u"Star is NOT save stem")}}),0,star,error),
        "decoded StarRecord supplies key/count/position/stage/flags")) return 1;
    std::vector<pkg::ResourceKey> keys;
    if (!check(planet::resource_keys(star,keys,error) && keys.size()==3 && keys[0].instance==0x11223344 &&
        keys[1].instance==0x12223344 && keys[2].instance==0x13223344 && keys[0].type==planet::kResourceType && keys[0].group==1,
        "counted planet keys use star.mKey not object token")) return 1;
    auto wrap_star=star; wrap_star.key=0xff223344;
    if (!check(planet::resource_keys(wrap_star,keys,error) && keys[1].instance==0x00223344,"original uint32 key wrap")) return 1;
    planet::resource_keys(star,keys,error);
    std::vector<std::pair<pkg::ResourceKey,Bytes>> resources{{keys[0],named(u"Excluded",0x100)},
        {keys[1],named(u"Planet Save")},{keys[2],named(u"Last")}};
    starsdb::ExplicitDbpfReader reader; std::vector<planet::ResourceRecord> planets;
    const auto physical=package(resources);
    if (!check(reader.open_from_memory(physical,error) && planet::read_selected_records(reader,star,planets,error) && planets.size()==3,
               "physical DBPF exact counted resources -> planet records")) return 1;
    planet::Selection selected;
    if (!check(planet::select_save_stem(star,planets,selected,error) && selected.source_index==size_t{1} &&
               selected.save_stem==u"Planet Save","filter bit0x100 and preserve original first selected index")) return 1;
    auto no_filter=star; no_filter.flags=0;
    if (!check(planet::select_save_stem(no_filter,planets,selected,error) && selected.source_index==size_t{0} &&
               selected.save_stem==u"Excluded","star flags without0x40 keeps unfiltered first")) return 1;
    const auto fail_selection=[&](const starsdb::StarRecord& s,const std::vector<planet::ResourceRecord>& p) {
        planet::Selection sentinel; sentinel.save_stem=u"unchanged"; sentinel.source_index=99;
        return !planet::select_save_stem(s,p,sentinel,error) && !error.empty() && sentinel.save_stem==u"unchanged" && sentinel.source_index==size_t{99};
    };
    auto changed=planets; changed[2].record.flags.reset();
    if (!check(fail_selection(star,changed),"later filter flags unknown cannot publish partial selection")) return 1;
    changed=planets; std::swap(changed[0],changed[1]);
    if (!check(fail_selection(star,changed),"unsorted/wrong keyed observations rejected")) return 1;
    changed=planets; changed[1].record.name.reset();
    if (!check(fail_selection(star,changed),"selected name missing not empty default")) return 1;
    changed=planets; changed[2].record.unvalidated_tail=1;
    if (!check(fail_selection(star,changed),"later unsupported tail not partially published")) return 1;
    auto unknown=star; unknown.flags.reset();
    if (!check(fail_selection(unknown,planets),"unknown star flags")) return 1;
    unknown=star; unknown.key.reset();
    if (!check(fail_selection(unknown,planets),"unknown star key")) return 1;
    unknown=star; unknown.planet_count.reset();
    if (!check(fail_selection(unknown,planets),"unknown count")) return 1;
    if (!check(fail_selection(star,{planets[0]}),"incomplete observed vector")) return 1;
    changed=planets; for (auto& p:changed) p.record.flags=0x100;
    if (!check(planet::select_save_stem(star,changed,selected,error) && !selected.source_index && selected.save_stem.empty(),"all filtered explicitly empty result")) return 1;
    auto empty=star; empty.planet_count=u8{0}; empty.key.reset();
    if (!check(planet::select_save_stem(empty,{},selected,error) && !selected.source_index,"observed zero planet count needs no invented key")) return 1;
    changed=planets; changed[1].record.name=u"BAD_DATA";
    if (!check(fail_selection(star,changed),"BAD_DATA name generation unknown blocks")) return 1;
    changed[1].generated_name=u"Generated";
    if (!check(planet::select_save_stem(star,changed,selected,error) && selected.save_stem==u"Generated" &&
               *changed[1].record.name==u"BAD_DATA","observed generator result without original record mutation")) return 1;
    changed[1].generated_name=u"";
    if (!check(planet::select_save_stem(star,changed,selected,error) && selected.save_stem==u"BAD_DATA","empty generated result leaves marker")) return 1;
    changed[1].record.flags=1; changed[1].generated_name.reset();
    if (!check(planet::select_save_stem(star,changed,selected,error) && selected.save_stem==u"BAD_DATA","planet bit0 prevents assignment, no fake generator output")) return 1;
    changed=planets; changed[1].record.name=u"";
    if (!check(planet::select_save_stem(star,changed,selected,error) && selected.save_stem.empty() && selected.source_index,
               "observed empty planet name not automatically regenerated")) return 1;
    changed=planets; changed[1].record.name=std::u16string(4097,u'x');
    if (!check(fail_selection(star,changed),"manual oversized name rejected before copy")) return 1;
    changed[1].record.name=u"BAD_DATA"; changed[1].generated_name=std::u16string(4097,u'x');
    if (!check(fail_selection(star,changed),"oversized generator observation rejected before copy")) return 1;
    const auto fail_read=[&](const Bytes& b) {
        std::vector<planet::ResourceRecord> sentinel(1); sentinel[0].resource.instance=999;
        return reader.open_from_memory(b,error) && !planet::read_selected_records(reader,star,sentinel,error) &&
            !error.empty() && sentinel.size()==1 && sentinel[0].resource.instance==999;
    };
    if (!check(fail_read(package({resources[0],resources[1]})),"missing later resource is atomic error")) return 1;
    auto bad_resources=resources; bad_resources[2].second.push_back(1);
    if (!check(fail_read(package(bad_resources)),"resource tail rejected not full parity")) return 1;
    bad_resources=resources; bad_resources.push_back(resources[1]);
    if (!check(fail_read(package(bad_resources)),"duplicate resource rejected")) return 1;

    // Observed envelope transport, independently constructed nested DBPF fixtures.
    // Direct counted fixtures above stay supported and are never confused with a native wrapper.
    const pkg::ResourceKey inner_key{1,1,1};
    const auto wrap=[](const Bytes& inner) { Bytes out{0,0x25,0,3};append(out,inner);return out; };
    const auto inner=package({{inner_key,named(u"Wrapped planet")}});
    const auto wrapper=wrap(inner);
    auto wrapped_resources=resources;wrapped_resources[1].second=wrapper;
    if(!check(reader.open_from_memory(package(wrapped_resources),error)&&
        planet::read_selected_records(reader,star,planets,error)&&planets[1].record.name==std::u16string(u"Wrapped planet")&&
        !planets[0].envelope&&planets[1].envelope&&planets[1].envelope->prefix_first==37&&
        planets[1].envelope->prefix_second==3&&planets[1].envelope->inner_resource.instance==1&&
        planets[1].envelope->inner_resource.type==1&&planets[1].envelope->inner_resource.group==1&&
        planets[1].envelope->inner_container_bytes==inner.size()&&
        planets[1].envelope->inner_index_end==inner.size()&&!planets[1].envelope->container_tail_bytes,
        "exact observed prefix/nested key provenance; direct payload remains distinct"))return 1;
    auto with_tail=inner;append(with_tail,Bytes{0xde,0xad,0xbe,0xef});
    wrapped_resources[1].second=wrap(with_tail);
    if(!check(reader.open_from_memory(package(wrapped_resources),error)&&
        planet::read_selected_records(reader,star,planets,error)&&planets[1].envelope->container_tail_bytes==4&&
        planets[1].envelope->inner_index_end==inner.size()&&planets[1].envelope->inner_container_bytes==with_tail.size(),
        "inner index need not end at EOF; transport tail retained, not called decoded"))return 1;
    const auto reject_wrapper=[&](const Bytes& bytes) {
        auto batch=resources;batch[2].second=bytes;return fail_read(package(batch));
    };
    for(size_t n=0;n<wrapper.size();++n)
        if(!check(reject_wrapper(Bytes(wrapper.begin(),wrapper.begin()+n)),"every wrapper truncation atomic; no partial planet batch"))return 1;
    for(const auto& key:std::vector<pkg::ResourceKey>{{2,1,1},{1,2,1},{1,1,2}})
        if(!check(reject_wrapper(wrap(package({{key,named(u"Wrong key")}}))),"sole wrong inner key is not substituted"))return 1;
    if(!check(reject_wrapper(wrap(package({}))),"empty nested directory not a planet"))return 1;
    if(!check(reject_wrapper(wrap(package({{inner_key,named(u"A")},{inner_key,named(u"B")}}))),
        "duplicate exact inner key never first-match"))return 1;
    if(!check(reject_wrapper(wrap(package({{inner_key,named(u"A")},{{2,2,2},named(u"Extra")}}))),
        "extra inner records outside observed transport subset"))return 1;
    for(const auto& mutation:std::vector<std::pair<size_t,u32>>{{0,0},{4,2},{8,1},{0x3c,0},{0x24,65537},
            {0x2c,4},{0x40,0xffffffffu},{inner.size()-36,0x80000004u},
            {inner.size()-4,0},{inner.size()-4,0x00010001u},
            {inner.size()-8,0xffffffffu},{inner.size()-16,1},{inner.size()-32,1}}) {
        auto changed_inner=inner;le(changed_inner,mutation.first,mutation.second);
        if(!check(reject_wrapper(wrap(changed_inner)),"nested version/index/flags/compression/size/high-word/bounds rejected"))return 1;
    }
    auto wrong_prefix=wrapper;wrong_prefix[1]=0x24;
    if(!check(reject_wrapper(wrong_prefix),"wrong wrapper prefix not accepted from DBPF magic alone"))return 1;
    wrong_prefix=wrapper;wrong_prefix[3]=4;
    if(!check(reject_wrapper(wrong_prefix),"unsupported prefix second half"))return 1;
    auto wrong_magic=wrapper;wrong_magic[4]='X';
    if(!check(reject_wrapper(wrong_magic),"recognized prefix with bad nested magic blocks"))return 1;
    auto inner_payload_tail=named(u"Tail");inner_payload_tail.push_back(1);
    if(!check(reject_wrapper(wrap(package({{inner_key,inner_payload_tail}}))),
        "decoded inner PlanetRecord tail rejected, unlike opaque container tail"))return 1;
    // RefPack literal-only stream; nested output traverses the same bounded record walker.
    const auto literal_payload=named(u"Packed");Bytes packed{0x10,0xfb,0,0,(u8)literal_payload.size()};
    const size_t literals=(literal_payload.size()/4)*4;
    packed.push_back((u8)(0xe0+(literals-4)/4));
    packed.insert(packed.end(),literal_payload.begin(),literal_payload.begin()+literals);
    packed.push_back((u8)(0xfc+literal_payload.size()-literals));
    packed.insert(packed.end(),literal_payload.begin()+literals,literal_payload.end());
    auto packed_dbpf=package({{inner_key,packed}});
    le(packed_dbpf,packed_dbpf.size()-8,(u32)literal_payload.size());le(packed_dbpf,packed_dbpf.size()-4,0x1ffff);
    wrapped_resources=resources;wrapped_resources[1].second=wrap(packed_dbpf);
    if(!check(reader.open_from_memory(package(wrapped_resources),error)&&planet::read_selected_records(reader,star,planets,error)&&
        planets[1].record.name==std::u16string(u"Packed"),"nested RefPack exact output size and planet framing"))return 1;
    le(packed_dbpf,packed_dbpf.size()-8,64*1024*1024+1);
    if(!check(reject_wrapper(wrap(packed_dbpf)),"nested decompression declaration exceeds host bound before allocation"))return 1;
    // Restore original direct source rows for the separate controlled metadata adapter checks.
    if(!check(reader.open_from_memory(physical,error)&&planet::read_selected_records(reader,star,planets,error),
        "direct transport regression after wrapper checks"))return 1;

    // Decoded StarRecord + planets + decoded file header + controlled terrain query -> GGE entry.
    save::FileHeader header; save::HeaderProbe probe;
    if (!check(save::read_header(frame({{0x04162d88,words({37})},{0x04162d89,words({3})}}),header,error) &&
               save::header_probe(header,probe,error),"actual file version probe input")) return 1;
    gge::SavedEntryObservations observed; observed.header_probe=probe; observed.terrain=gge::TerrainQueryObservation::QueryFailed;
    gge::SavedGameMetadata metadata;
    if (!check(gge::adapt_saved_metadata(star,planets,observed,metadata,error) && metadata.star.name==u"Planet Save" &&
        metadata.save_stem==u"Planet Save" && metadata.star.star_id==*star.key && metadata.stage==4,"saved typed adapter uses selected PLANET name")) return 1;
    gge::CatalogInput input; input.saved_status=input.candidate_status=gge::CatalogAvailability::Available; input.saved.push_back(metadata);
    std::vector<gge::GameEntry> entries;
    if (!check(gge::build_catalog(input,entries,error) && entries.size()==1 && entries[0].name==u"Planet Save" &&
        entries[0].position.x==1 && entries[0].stage==4 && entries[0].image.instance==0x06837044 &&
        entries[0].save_status==gge::SaveStatus::HeaderMatches,"typed adapter -> actual GGE projector, no scene readiness claim")) return 1;
    const auto fail_adapter=[&](const starsdb::StarRecord& s,const std::vector<planet::ResourceRecord>& p,const gge::SavedEntryObservations& o) {
        gge::SavedGameMetadata sentinel; sentinel.save_stem=u"unchanged";
        return !gge::adapt_saved_metadata(s,p,o,sentinel,error) && !error.empty() && sentinel.save_stem==u"unchanged";
    };
    auto missing_observation=observed; missing_observation.header_probe.reset();
    if (!check(fail_adapter(star,planets,missing_observation),"header observation unknown not false/zero")) return 1;
    missing_observation=observed; missing_observation.terrain=gge::TerrainQueryObservation::Unknown;
    if (!check(fail_adapter(star,planets,missing_observation),"terrain unavailable not failed/empty")) return 1;
    missing_observation.terrain=(gge::TerrainQueryObservation)99;
    if (!check(fail_adapter(star,planets,missing_observation),"invalid terrain enum not mapped to observed query failure")) return 1;
    observed.terrain=gge::TerrainQueryObservation::Loaded;
    if (!check(fail_adapter(star,planets,observed),"loaded terrain property result must be observed")) return 1;
    observed.terrain_image=0;
    if (!check(gge::adapt_saved_metadata(star,planets,observed,metadata,error) && metadata.star.terrain_loaded &&
        metadata.star.terrain_image==0,"explicit missing property zero distinct from failed list")) return 1;
    auto flagged=star; flagged.flags=0x80; flagged.tech_level.reset();
    if (!check(gge::adapt_saved_metadata(flagged,planets,observed,metadata,error) && metadata.stage==0,
               "flag0x80 bypasses unknown tech-level read")) return 1;
    unknown=star; unknown.tech_level.reset();
    if (!check(fail_adapter(unknown,planets,observed),"unflagged unknown stage")) return 1;
    unknown=star; unknown.position=std::array<float,3>{std::numeric_limits<float>::quiet_NaN(),0,0};
    if (!check(fail_adapter(unknown,planets,observed),"nonfinite star position")) return 1;
    if (!check(fail_adapter(empty,{},observed),"empty selection blocked at metadata boundary")) return 1;
    // Real planetRecords.pkp transport: 4-byte prefix + nested single-record DBPF.
    auto wrapped_star=star; wrapped_star.planet_count=(u8)1;
    planet::resource_keys(wrapped_star,keys,error);
    const auto inner_pkg=package({{planet::kNestedPlanetKey,named(u"Wrapped World")}});
    Bytes wrapped_rec={0x00,0x25,0x00,0x03};
    append(wrapped_rec,inner_pkg);
    const auto outer_pkg=package({{keys[0],wrapped_rec}});
    if (!check(reader.open_from_memory(outer_pkg,error) && planet::read_selected_records(reader,wrapped_star,planets,error) &&
               planets.size()==1 && planets[0].record.name==u"Wrapped World" &&
               planets[0].record.unvalidated_tail==0,"wrapped nested planet transport decodes")) return 1;
    planet::Selection wrapped_sel;
    if (!check(planet::select_save_stem(wrapped_star,planets,wrapped_sel,error) && wrapped_sel.source_index==size_t{0} &&
               wrapped_sel.save_stem==u"Wrapped World","wrapped planet selects save stem")) return 1;
    auto bad_prefix=wrapped_rec; bad_prefix[0]^=1;
    const auto bad_pkg=package({{keys[0],bad_prefix}});
    if (!check(reader.open_from_memory(bad_pkg,error) && !planet::read_selected_records(reader,wrapped_star,planets,error) &&
               !error.empty(),"wrong prefix rejected, never silently direct-parsed")) return 1;
    const auto two_inner=package({{pkg::ResourceKey{1,planet::kResourceType,planet::kResourceGroup},named(u"A")},
                                  {pkg::ResourceKey{2,planet::kResourceType,planet::kResourceGroup},named(u"B")}});
    Bytes two_rec={0x00,0x25,0x00,0x03};
    append(two_rec,two_inner);
    const auto two_pkg=package({{keys[0],two_rec}});
    if (!check(reader.open_from_memory(two_pkg,error) && !planet::read_selected_records(reader,wrapped_star,planets,error) &&
               !error.empty(),"nested planet DBPF must hold exactly 1 record")) return 1;
    if (argc==2) {
        if (std::filesystem::exists(argv[1])) { std::cerr<<"requires new synthetic destination\n"; return 2; }
        std::ofstream file(argv[1],std::ios::binary); file.write((const char*)physical.data(),(std::streamsize)physical.size());
        if (!file) { std::cerr<<"synthetic write failed\n"; return 1; }
        std::cout<<"wrote CALLER-REQUESTED SYNTHETIC planet DBPF, NOT original game/save data\n";
    }
    std::cout<<checks<<" constructed planet-prefix/selection/DBPF/saved-adapter checks PASS; no live catalogue/galaxy claim\n";
    return 0;
}
