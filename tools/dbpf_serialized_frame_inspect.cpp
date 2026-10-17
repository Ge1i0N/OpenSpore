// Explicit DBPF resource -> bounded serialized-frame inspection.
// No directory search, writes, object construction or catalogue/readiness claim.
#include "engine/pkg.h"
#include "engine/serialized_frame.h"
#include <charconv>
#include <iomanip>
#include <iostream>
static bool dec(const char* text,size_t& value){const auto r=std::from_chars(text,text+std::strlen(text),value,10);return r.ec==std::errc{}&&*r.ptr=='\0';}
int main(int argc,char** argv){
    if(argc!=3){std::cerr<<"usage: dbpf_serialized_frame_inspect <explicit-DBPF> <record-index-decimal>\n";return 2;}
    size_t index=0;if(!dec(argv[2],index)){std::cerr<<"invalid record index\n";return 2;}
    os::pkg::Package p;if(!p.open(argv[1])||index>=p.records().size()){std::cerr<<"DBPF/resource rejected\n";return 1;}
    std::vector<os::u8> bytes;if(!p.read_resource_decoded((os::u32)index,bytes)){std::cerr<<"resource decode failed\n";return 1;}
    os::serialized::Frame frame;std::string error;
    if(!os::serialized::parse_frame(bytes,0,os::serialized::kObjectTag,frame,error)){std::cerr<<"serialized frame rejected: "<<error<<'\n';return 1;}
    std::cout<<"EXPLICIT SERIALIZED FRAME ONLY path="<<argv[1]<<" record="<<index<<" bytes="<<bytes.size()<<" tag=0x"<<std::hex<<frame.tag<<std::dec<<" fields="<<frame.fields.size()<<" frame_end="<<frame.end<<" tail="<<(bytes.size()-frame.end)<<"\n";
    for(size_t i=0;i<frame.fields.size();++i){const auto& f=frame.fields[i];std::cout<<"["<<i<<"] id=0x"<<std::hex<<f.id<<std::dec<<" offset="<<f.offset<<" size="<<f.size<<" preview=";for(size_t n=0;n<f.size&&n<16;++n)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)bytes[f.offset+n]<<' ';std::cout<<std::dec<<'\n';}
    std::cout<<"Field semantics are not inferred; no catalogue/readiness claim\n";return 0;
}
