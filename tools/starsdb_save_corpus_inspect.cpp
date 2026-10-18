// Explicit save-corpus inspector. Every input path is caller-selected; no directory search or writes.
#include "engine/starsdb_dbpf.h"
#include "engine/planet_record.h"
#include "engine/save_header.h"
#include <iostream>
using namespace os; using namespace os::starsdb;
static void name(const std::optional<std::u16string>& n){if(!n){std::cout<<"unknown";return;}std::cout<<"\\u"<<std::hex;for(char16_t c:*n)std::cout<<(unsigned)c<<" ";std::cout<<std::dec;}
int main(int argc,char** argv){
    if(argc!=3&&argc!=4){std::cerr<<"usage: starsdb_save_corpus_inspect <explicit-stars.db> <explicit-planetRecords.pkp> [explicit-save.spo]\n";return 2;}
    PhysicalContextResult ctx;std::string error;if(!inspect_physical_context(argv[1],save::kRoot,save::kRoot,save::kRoot,ctx,error)){std::cerr<<"stars.db blocked: "<<error<<'\n';return 1;}
    std::cout<<"EXPLICIT SAVE CORPUS ONLY - no profile scan, writes, post-load fixups or readiness claim\n";
    std::cout<<"root_bytes="<<ctx.root.bytes.size()<<" classes="<<ctx.decoded_classes.size()<<" star_objects=";
    size_t stars=0;for(const auto& c:ctx.decoded_classes)if(c.class_id==kStarRecordClass)stars+=c.objects.size();std::cout<<stars<<"\n";
    if(ctx.manager_links){auto count=[](const auto& v){return v?std::to_string(v->size()):std::string("unknown");};std::cout<<"links starters="<<count(ctx.manager_links->starters)<<" saved="<<count(ctx.manager_links->saved)<<" black_holes="<<count(ctx.manager_links->black_holes)<<" possible_starts="<<count(ctx.manager_links->possible_starts)<<" grid="<<count(ctx.manager_links->grid)<<"\n";}
    ExplicitDbpfReader planets;if(!planets.open(argv[2],error)){std::cerr<<"planetRecords blocked: "<<error<<'\n';return 1;}
    if(ctx.manager_links&&ctx.manager_links->saved){size_t index=0;for(const auto& ref:*ctx.manager_links->saved){std::cout<<"saved["<<index++<<"] token=0x"<<std::hex<<ref.token<<" class=0x"<<ref.class_id<<std::dec;const ClassObjects* cls=nullptr;for(const auto& c:ctx.decoded_classes)if(c.class_id==ref.class_id){cls=&c;break;}if(!cls){std::cout<<" resolution=missing-class\n";continue;}auto rr=resolve_star_reference(ref,*cls);if(rr.status!=ReferenceResolution::StarFields||rr.object_index>=cls->objects.size()||!cls->objects[rr.object_index].star){std::cout<<" resolution=not-star\n";continue;}const auto& star=*cls->objects[rr.object_index].star;std::cout<<" key="<<(star.key?std::to_string(*star.key):"unknown")<<" flags="<<(star.flags?std::to_string(*star.flags):"unknown")<<" tech="<<(star.tech_level?std::to_string(*star.tech_level):"unknown")<<" planets="<<(star.planet_count?std::to_string(*star.planet_count):"unknown")<<" pos=";if(star.position)std::cout<<(*star.position)[0]<<','<<(*star.position)[1]<<','<<(*star.position)[2];else std::cout<<"unknown";std::cout<<" name=";name(star.name);std::vector<planet::ResourceRecord> recs;if(planet::read_selected_records(planets,star,recs,error)){planet::Selection sel;if(planet::select_save_stem(star,recs,sel,error)&&sel.source_index)std::cout<<" planet_records="<<recs.size()<<" save_stem=";else std::cout<<" planet_records="<<recs.size()<<" save_stem=unknown";if(sel.source_index){name(std::optional<std::u16string>(sel.save_stem));}}else std::cout<<" planet_records=blocked("<<error<<")";std::cout<<'\n';}}
    if(argc==4){ExplicitDbpfReader save_reader;save::FileHeader h;save::HeaderProbe p;if(!save_reader.open(argv[3],error)||!save::read_selected_header(save_reader,h,error)||!save::header_probe(h,p,error))std::cout<<"save_header=blocked("<<error<<")\n";else std::cout<<"save_header="<<p.words.first<<','<<p.words.second<<" predicate="<<p.predicate<<"\n";}
    return 0;
}
