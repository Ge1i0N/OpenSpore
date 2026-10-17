// Explicit DBPF resource payload preview. No directory search, writes or schema/readiness claim.
#include "engine/pkg.h"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
int main(int argc,char** argv){
    if(argc!=3&&argc!=4){std::cerr<<"usage: dbpf_resource_inspect <explicit-DBPF> <record-index-decimal> [--dump]\n";return 2;}
    const bool dump=argc==4&&std::string(argv[3])=="--dump";if(argc==4&&!dump){std::cerr<<"unknown option\n";return 2;}
    os::u32 index=0;const auto parsed=std::from_chars(argv[2],argv[2]+std::strlen(argv[2]),index,10);
    if(parsed.ec!=std::errc{}||parsed.ptr!=argv[2]+std::strlen(argv[2])){std::cerr<<"invalid record index\n";return 2;}
    os::pkg::Package p;os::pkg::ReadLimits limits;limits.file_bytes=512ull*1024*1024;limits.records=262144;limits.index_bytes=16*1024*1024;
    if(!p.open_bounded(argv[1],limits)||!p.index_parsed()||index>=p.records().size()){std::cerr<<"DBPF/resource rejected\n";return 1;}
    const auto& record=p.records()[index];const size_t cap=dump?1024*1024:64*1024*1024;
    if(record.mem_size()>cap||record.packed_size()>cap){std::cerr<<"resource exceeds inspection bound\n";return 1;}
    std::vector<os::u8> bytes;if(!p.read_resource_decoded((os::u32)index,bytes)){std::cerr<<"resource decode failed\n";return 1;}
    const auto& r=p.records()[index];std::cout<<"EXPLICIT DBPF RESOURCE ONLY path="<<argv[1]<<" index="<<index<<" instance=0x"<<std::hex<<r.instance()<<" type=0x"<<r.type()<<" group=0x"<<r.group()<<std::dec<<" bytes="<<bytes.size()<<"\n";
    const size_t n=std::min<size_t>(bytes.size(),256);std::cout<<"hex=";for(size_t i=0;i<n;++i)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)bytes[i]<<' ';std::cout<<std::dec<<"\nascii=";for(size_t i=0;i<n;++i)std::cout<<(bytes[i]>=32&&bytes[i]<127?(char)bytes[i]:'.');std::cout<<"\npreview_limit=256; payload schema not interpreted\n";
    if(dump){std::cout<<"resource_raw_hex=";for(const auto b:bytes)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)b;std::cout<<std::dec<<'\n';}return 0;
}
