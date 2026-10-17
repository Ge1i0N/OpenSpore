// Explicit DBPF resource payload preview. No directory search, writes or schema/readiness claim.
#include "engine/pkg.h"
#include <iomanip>
#include <iostream>
#include <sstream>
int main(int argc,char** argv){
    if(argc!=3){std::cerr<<"usage: dbpf_resource_inspect <explicit-DBPF> <record-index-decimal>\n";return 2;}
    size_t index=0;try{index=std::stoull(argv[2]);}catch(...){std::cerr<<"invalid record index\n";return 2;}
    os::pkg::Package p;if(!p.open(argv[1])||index>=p.records().size()){std::cerr<<"DBPF/resource rejected\n";return 1;}
    std::vector<os::u8> bytes;if(!p.read_resource_decoded((os::u32)index,bytes)){std::cerr<<"resource decode failed\n";return 1;}
    const auto& r=p.records()[index];std::cout<<"EXPLICIT DBPF RESOURCE ONLY path="<<argv[1]<<" index="<<index<<" instance=0x"<<std::hex<<r.instance()<<" type=0x"<<r.type()<<" group=0x"<<r.group()<<std::dec<<" bytes="<<bytes.size()<<"\n";
    const size_t n=std::min<size_t>(bytes.size(),256);std::cout<<"hex=";for(size_t i=0;i<n;++i)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)bytes[i]<<' ';std::cout<<std::dec<<"\nascii=";for(size_t i=0;i<n;++i)std::cout<<(bytes[i]>=32&&bytes[i]<127?(char)bytes[i]:'.');std::cout<<"\npreview_limit=256; payload schema not interpreted\n";return 0;
}
