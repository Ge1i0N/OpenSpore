// Explicit DBPF inventory only. No directory search, profile scan, writes or gameplay readiness.
#include "engine/pkg.h"
#include <map>
#include <iostream>
int main(int argc,char** argv){
    if(argc!=2&&argc!=3){std::cerr<<"usage: dbpf_inventory_inspect <explicit-DBPF> [--full]\n";return 2;}
    const bool full=argc==3&&std::string(argv[2])=="--full";if(argc==3&&!full){std::cerr<<"unknown option\n";return 2;}
    os::pkg::Package p;if(!p.open(argv[1])){std::cerr<<"DBPF rejected: "<<p.why()<<'\n';return 1;}
    std::map<std::pair<os::u32,os::u32>,size_t> groups;for(const auto& r:p.records())++groups[{r.type(),r.group()}];
    std::cout<<"EXPLICIT DBPF INVENTORY ONLY path="<<argv[1]<<" records="<<p.records().size()<<" kind="<<p.kind()<<" groups="<<groups.size()<<"\n";
    for(const auto& g:groups)std::cout<<"group type=0x"<<std::hex<<g.first.first<<" group=0x"<<g.first.second<<std::dec<<" records="<<g.second<<"\n";
    const size_t limit=full?p.records().size():std::min<size_t>(16,p.records().size());
    for(size_t i=0;i<limit;++i){const auto& r=p.records()[i];std::cout<<"["<<i<<"] instance=0x"<<std::hex<<r.instance()<<" type=0x"<<r.type()<<" group=0x"<<r.group()<<std::dec<<" packed="<<r.packed_size()<<" memory="<<r.mem_size()<<" compression=0x"<<std::hex<<r.compression()<<std::dec<<"\n";}
    if(!full&&p.records().size()>limit)std::cout<<"record preview limited to 16; pass --full for all records\n";
    std::cout<<"No resource payload was interpreted; no catalogue/readiness claim\n";return 0;
}
