// Explicit real-save -> GGE_CATALOG_V1 bridge.
// Read-only: every input/output path is caller selected; no profile search, discovery or writes to saves.
#include "engine/gge_saved_adapter.h"
#include "engine/starsdb_dbpf.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace os;
using namespace os::starsdb;

static std::string hex_utf8(const std::u16string& s) {
    std::ostringstream o; o<<std::hex<<std::setfill('0');
    for (char16_t c:s) {
        u32 cp=c;
        if(c>=0xd800&&c<=0xdbff) continue; // names from this adapter are BMP in the observed save
        if(cp<0x80)o<<std::setw(2)<<cp;
        else if(cp<0x800)o<<std::setw(2)<<(0xc0|(cp>>6))<<std::setw(2)<<(0x80|(cp&63));
        else o<<std::setw(2)<<(0xe0|(cp>>12))<<std::setw(2)<<(0x80|((cp>>6)&63))<<std::setw(2)<<(0x80|(cp&63));
    }
    return o.str();
}
static bool write_capture(const std::string& path,const std::vector<os::gge::SavedGameMetadata>& saved,std::string& error) {
    std::ofstream out(path,std::ios::binary); if(!out){error="cannot create explicitly selected capture output";return false;}
    out<<"GGE_CATALOG_V1\n# Derived from explicitly selected real save files; not native runtime replay.\n"
       <<"saved_status\tAvailable\ncandidate_status\tUnknown\n";
    out<<"manager_status\tAvailable\nstarsdb_status\tAvailable\nplanet_status\tAvailable\nrefill_status\tUnknown\n";
    out<<"resource_root_status\tAvailable\nresource_index_status\tAvailable\nresource_context_status\tAvailable\nresource_class_status\tAvailable\nrefill_append\tUnknown\n";
    out<<"saved_count\t"<<saved.size()<<"\ncandidate_count\t0\n";
    for(const auto& s:saved) out<<"saved\t0x"<<std::hex<<s.star.star_id<<std::dec<<'\t'
       <<s.star.position.x<<'\t'<<s.star.position.y<<'\t'<<s.star.position.z<<'\t'
       <<s.stage<<"\t0x"<<std::hex<<s.source_flags<<std::dec<<'\t'
       <<hex_utf8(s.star.name)<<'\t'<<hex_utf8(s.save_stem)<<'\t'
       <<(s.probe_result?1:0)<<'\t'<<s.header.first<<'\t'<<s.header.second<<"\t0\t0\n";
    out<<"END\n";
    if(!out){error="capture output write failed";return false;} return true;
}
int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"usage: starsdb_gge_capture <explicit-stars.db> <explicit-planetRecords.pkp> <explicit-save.spo> <NEW-capture.txt>\n";return 2;}
    const std::string stars=argv[1], planets_path=argv[2], save_path=argv[3], output=argv[4];
    std::string error; PhysicalContextResult context;
    if(!inspect_physical_context(stars,save::kRoot,save::kRoot,save::kRoot,context,error)){std::cerr<<"stars.db blocked: "<<error<<'\n';return 1;}
    if(!context.manager_links||!context.manager_links->saved||context.manager_links->saved->empty()){std::cerr<<"saved manager reference absent\n";return 1;}
    ExplicitDbpfReader planet_reader; if(!planet_reader.open(planets_path,error)){std::cerr<<"planetRecords blocked: "<<error<<'\n';return 1;}
    ExplicitDbpfReader save_reader; if(!save_reader.open(save_path,error)){std::cerr<<"save blocked: "<<error<<'\n';return 1;}
    save::FileHeader header; save::HeaderProbe probe; if(!save::read_selected_header(save_reader,header,error)||!save::header_probe(header,probe,error)){std::cerr<<"save header blocked: "<<error<<'\n';return 1;}
    gge::SavedEntryObservations observed; observed.terrain=gge::TerrainQueryObservation::QueryFailed; observed.header_probe=probe;
    std::vector<gge::SavedGameMetadata> saved_rows;
    for(const auto& ref:*context.manager_links->saved){const ClassObjects* cls=nullptr;for(const auto& c:context.decoded_classes)if(c.class_id==ref.class_id){cls=&c;break;}if(!cls){std::cerr<<"saved reference class stream missing\n";return 1;}const auto resolved=resolve_star_reference(ref,*cls);if(resolved.status!=ReferenceResolution::StarFields||resolved.object_index>=cls->objects.size()||!cls->objects[resolved.object_index].star){std::cerr<<"saved reference did not resolve to StarRecord\n";return 1;}const auto& star=*cls->objects[resolved.object_index].star;std::vector<planet::ResourceRecord> planets;if(!planet::read_selected_records(planet_reader,star,planets,error)){std::cerr<<"planetRecords blocked for saved key 0x"<<std::hex<<(star.key?*star.key:0)<<std::dec<<": "<<error<<'\n';return 1;}gge::SavedGameMetadata row;if(!gge::adapt_saved_metadata(star,planets,observed,row,error)){std::cerr<<"saved adapter blocked for saved key: "<<error<<'\n';return 1;}saved_rows.push_back(std::move(row));}
    if(!write_capture(output,saved_rows,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"real save capture written: "<<output<<"\n"
             <<"saved_rows="<<saved_rows.size()<<" header="<<probe.words.first<<','<<probe.words.second
             <<" candidates=Unknown refill=Unknown terrain=QueryFailed\n"
             <<"derived capture remains producer_gate=BLOCKED; no native/live catalogue claim\n";
    return 0;
}
