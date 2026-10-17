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
static bool write_capture(const std::string& path,const os::gge::SavedGameMetadata& saved,std::string& error) {
    std::ofstream out(path,std::ios::binary); if(!out){error="cannot create explicitly selected capture output";return false;}
    out<<"GGE_CATALOG_V1\n# Derived from explicitly selected real save files; not native runtime replay.\n"
       <<"saved_status\tAvailable\n candidate_status\tUnknown\n";
    out<<"manager_status\tAvailable\n starsdb_status\tAvailable\n planet_status\tAvailable\n refill_status\tUnknown\n";
    out<<"resource_root_status\tAvailable\nresource_index_status\tAvailable\nresource_context_status\tAvailable\nresource_class_status\tAvailable\nrefill_append\tUnknown\n";
    out<<"saved_count\t1\ncandidate_count\t0\n";
    out<<"saved\t0x"<<std::hex<<saved.star.star_id<<std::dec<<'\t'
       <<saved.star.position.x<<'\t'<<saved.star.position.y<<'\t'<<saved.star.position.z<<'\t'
       <<saved.stage<<"\t0x"<<std::hex<<saved.source_flags<<std::dec<<'\t'
       <<hex_utf8(saved.star.name)<<'\t'<<hex_utf8(saved.save_stem)<<'\t'
       <<(saved.probe_result?1:0)<<'\t'<<saved.header.first<<'\t'<<saved.header.second<<"\t0\t0\nEND\n";
    if(!out){error="capture output write failed";return false;} return true;
}
int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"usage: starsdb_gge_capture <explicit-stars.db> <explicit-planetRecords.pkp> <explicit-save.spo> <NEW-capture.txt>\n";return 2;}
    const std::string stars=argv[1], planets_path=argv[2], save_path=argv[3], output=argv[4];
    std::string error; PhysicalContextResult context;
    if(!inspect_physical_context(stars,save::kRoot,save::kRoot,save::kRoot,context,error)){std::cerr<<"stars.db blocked: "<<error<<'\n';return 1;}
    if(!context.manager_links||!context.manager_links->saved||context.manager_links->saved->size()!=1){std::cerr<<"saved manager reference is absent/ambiguous\n";return 1;}
    const auto& ref=(*context.manager_links->saved)[0]; const ClassObjects* cls=nullptr;
    for(const auto& c:context.decoded_classes)if(c.class_id==ref.class_id){cls=&c;break;}
    if(!cls){std::cerr<<"saved reference class stream missing\n";return 1;}
    const auto resolved=resolve_star_reference(ref,*cls); if(resolved.status!=ReferenceResolution::StarFields||resolved.object_index>=cls->objects.size()||!cls->objects[resolved.object_index].star){std::cerr<<"saved reference did not resolve to StarRecord\n";return 1;}
    const auto& star=*cls->objects[resolved.object_index].star;
    ExplicitDbpfReader planet_reader; if(!planet_reader.open(planets_path,error)){std::cerr<<"planetRecords blocked: "<<error<<'\n';return 1;}
    std::vector<planet::ResourceRecord> planets; if(!planet::read_selected_records(planet_reader,star,planets,error)){std::cerr<<"planetRecords blocked: "<<error<<'\n';return 1;}
    ExplicitDbpfReader save_reader; if(!save_reader.open(save_path,error)){std::cerr<<"save blocked: "<<error<<'\n';return 1;}
    save::FileHeader header; save::HeaderProbe probe; if(!save::read_selected_header(save_reader,header,error)||!save::header_probe(header,probe,error)){std::cerr<<"save header blocked: "<<error<<'\n';return 1;}
    gge::SavedEntryObservations observed; observed.terrain=gge::TerrainQueryObservation::QueryFailed; observed.header_probe=probe;
    gge::SavedGameMetadata saved; if(!gge::adapt_saved_metadata(star,planets,observed,saved,error)){std::cerr<<"saved adapter blocked: "<<error<<'\n';return 1;}
    if(!write_capture(output,saved,error)){std::cerr<<error<<'\n';return 1;}
    std::cout<<"real save capture written: "<<output<<"\n"
             <<"saved key=0x"<<std::hex<<saved.star.star_id<<std::dec<<" planets="<<planets.size()
             <<" stage="<<saved.stage<<" header="<<saved.header.first<<','<<saved.header.second
             <<" candidates=Unknown refill=Unknown terrain=QueryFailed\n"
             <<"derived capture remains producer_gate=BLOCKED; no native/live catalogue claim\n";
    return 0;
}
