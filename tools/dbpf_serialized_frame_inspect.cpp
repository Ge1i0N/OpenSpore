// Explicit DBPF resource -> bounded serialized-frame inspection.
// No directory search, writes, object construction or catalogue/readiness claim.
#include "engine/pkg.h"
#include "engine/serialized_frame.h"
#include <charconv>
#include <iomanip>
#include <iostream>
#include <algorithm>
static bool dec(const char* text,size_t& value){const auto r=std::from_chars(text,text+std::strlen(text),value,10);return r.ec==std::errc{}&&*r.ptr=='\0';}
static bool hex(const char* text,os::u32& value){if(std::strlen(text)>=2&&text[0]=='0'&&(text[1]=='x'||text[1]=='X'))text+=2;const auto r=std::from_chars(text,text+std::strlen(text),value,16);return r.ec==std::errc{}&&r.ptr==text+std::strlen(text);}
int main(int argc,char** argv){
    if(argc!=3&&argc!=4&&argc!=5){std::cerr<<"usage: dbpf_serialized_frame_inspect <explicit-DBPF> <record-index-decimal> [nested-field-id-hex] [--dump]\n";return 2;}
    size_t index=0;if(!dec(argv[2],index)){std::cerr<<"invalid record index\n";return 2;}
    os::pkg::Package p;os::pkg::ReadLimits limits;limits.file_bytes=256*1024*1024;limits.records=65536;limits.index_bytes=4*1024*1024;
    if(!p.open_bounded(argv[1],limits)||!p.index_parsed()||index>=p.records().size()){std::cerr<<"DBPF/resource rejected\n";return 1;}
    if(p.records()[index].mem_size()>64*1024*1024||p.records()[index].packed_size()>64*1024*1024){std::cerr<<"resource exceeds 64MiB inspection bound\n";return 1;}
    std::vector<os::u8> bytes;if(!p.read_resource_decoded((os::u32)index,bytes)){std::cerr<<"resource decode failed\n";return 1;}
    os::serialized::Frame frame;std::string error;os::u32 nested_id=0;const bool nested_requested=argc>=4&&hex(argv[3],nested_id);if(argc>=4&&!nested_requested){std::cerr<<"invalid nested field id\n";return 2;}const bool dump=argc==5&&std::string(argv[4])=="--dump";if(argc==5&&!dump){std::cerr<<"unknown option\n";return 2;}
    if(!os::serialized::parse_frame(bytes,0,os::serialized::kObjectTag,frame,error)){std::cerr<<"serialized frame rejected: "<<error<<'\n';return 1;}
    std::cout<<"EXPLICIT SERIALIZED FRAME ONLY path="<<argv[1]<<" record="<<index<<" bytes="<<bytes.size()<<" tag=0x"<<std::hex<<frame.tag<<std::dec<<" fields="<<frame.fields.size()<<" frame_end="<<frame.end<<" tail="<<(bytes.size()-frame.end)<<"\n";
    for(size_t i=0;i<frame.fields.size();++i){const auto& f=frame.fields[i];std::cout<<"["<<i<<"] id=0x"<<std::hex<<f.id<<std::dec<<" offset="<<f.offset<<" size="<<f.size<<" preview=";for(size_t n=0;n<f.size&&n<16;++n)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)bytes[f.offset+n]<<' ';std::cout<<std::dec<<'\n';}
    if(nested_requested){
        const auto it=std::find_if(frame.fields.begin(),frame.fields.end(),[&](const auto& f){return f.id==nested_id;});
        if(it==frame.fields.end()){std::cerr<<"nested field not found\n";return 1;}
        if(std::count_if(frame.fields.begin(),frame.fields.end(),[&](const auto& f){return f.id==nested_id;})!=1){std::cerr<<"nested field ambiguous\n";return 1;}
        if(it->size>65536){std::cerr<<"nested field exceeds 64KiB inspection bound\n";return 1;}
        // A child must fit its own field, not merely the enclosing resource.
        const std::vector<os::u8> payload(bytes.begin()+it->offset,bytes.begin()+it->offset+it->size);
        os::serialized::Frame child;if(!os::serialized::parse_frame(payload,0,os::serialized::kObjectTag,child,error)){std::cerr<<"nested frame rejected: "<<error<<'\n';return 1;}
        std::cout<<"nested_field=0x"<<std::hex<<nested_id<<std::dec<<" tag=0x"<<std::hex<<child.tag<<std::dec<<" fields="<<child.fields.size()<<" frame_end="<<it->offset+child.end<<" tail="<<payload.size()-child.end<<"\n";
        for(size_t i=0;i<child.fields.size()&&i<64;++i){const auto& f=child.fields[i];std::cout<<"  nested["<<i<<"] id=0x"<<std::hex<<f.id<<std::dec<<" offset="<<it->offset+f.offset<<" size="<<f.size<<" preview=";for(size_t n=0;n<f.size&&n<16;++n)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)payload[f.offset+n]<<' ';std::cout<<std::dec<<'\n';}
        if(child.fields.size()>64)std::cout<<"  nested field preview limited to 64\n";
        if(dump){std::cout<<"nested_raw_hex=";for(const auto b:payload)std::cout<<std::hex<<std::setw(2)<<std::setfill('0')<<(unsigned)b;std::cout<<std::dec<<"\n";}
    }
    std::cout<<"Field semantics are not inferred; no catalogue/readiness claim\n";return 0;
}
