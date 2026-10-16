// Inspect only a caller-selected snapshot; no preview/state/profile/search side effects.
#include "engine/gge_catalog_snapshot.h"
#include <iostream>
#include <map>
using namespace os::gge;
static std::string ascii(const std::u16string& s){std::string o;for(char16_t c:s)o+=c<128?(char)c:'?';return o;}
int main(int argc,char** argv){
    if(argc!=2){std::cerr<<"usage: gge_catalog_snapshot_inspect <explicit-snapshot>\n";return 2;}
    CatalogInput input;std::string error;if(!load_catalog_snapshot(argv[1],input,error)){std::cerr<<"snapshot blocked: "<<error<<'\n';return 1;}
    std::map<os::u32,size_t> ids;for(const auto& s:input.saved)++ids[s.star.star_id];for(const auto& s:input.candidates)++ids[s.star_id];size_t duplicates=0;for(const auto& p:ids)if(p.second>1)duplicates+=p.second-1;
    auto ps=[](CatalogProducerStatus s){return s==CatalogProducerStatus::Available?"Available":s==CatalogProducerStatus::Failed?"Failed":"Unknown";};
    const bool ready=input.saved_status==CatalogAvailability::Available&&input.candidate_status==CatalogAvailability::Available&&input.producer.manager==CatalogProducerStatus::Available&&input.producer.starsdb==CatalogProducerStatus::Available&&input.producer.planet_records==CatalogProducerStatus::Available&&input.producer.candidate_refill==CatalogProducerStatus::Available;
    std::cout<<"snapshot=GGE_CATALOG_V1 path="<<argv[1]<<" source_label="<<input.source_label<<"\n"<<"saved_status="<<(input.saved_status==CatalogAvailability::Available?"Available":"Unknown")<<" candidate_status="<<(input.candidate_status==CatalogAvailability::Available?"Available":"Unknown")<<" manager_status="<<ps(input.producer.manager)<<" starsdb_status="<<ps(input.producer.starsdb)<<" planet_status="<<ps(input.producer.planet_records)<<" refill_status="<<ps(input.producer.candidate_refill)<<" producer_gate="<<(ready?"READY":"BLOCKED")<<" saved="<<input.saved.size()<<" candidates="<<input.candidates.size()<<" duplicate_star_id_rows="<<duplicates<<"\n";
    for(size_t i=0;i<input.saved.size();++i){const auto& s=input.saved[i];std::cout<<"saved["<<i<<"] id="<<s.star.star_id<<" pos="<<s.star.position.x<<','<<s.star.position.y<<','<<s.star.position.z<<" stage="<<s.stage<<" flags=0x"<<std::hex<<s.source_flags<<std::dec<<" name=\""<<ascii(s.star.name)<<"\" terrain="<<s.star.terrain_loaded<<" terrain_instance=0x"<<std::hex<<s.star.terrain_image<<std::dec<<"\n";}
    for(size_t i=0;i<input.candidates.size();++i){const auto& s=input.candidates[i];std::cout<<"candidate["<<i<<"] id="<<s.star_id<<" pos="<<s.position.x<<','<<s.position.y<<','<<s.position.z<<" name=\""<<ascii(s.name)<<"\" terrain="<<s.terrain_loaded<<" terrain_instance=0x"<<std::hex<<s.terrain_image<<std::dec<<"\n";}
    std::cout<<"source=caller-supplied typed metadata; no native/live catalogue claim\n";return 0;
}
