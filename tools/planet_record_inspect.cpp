// Caller-selected planet-record DBPF only. Does not discover saves or produce a ready GGE scene.
#include "engine/planet_record.h"
#include <charconv>
#include <cstring>
#include <iostream>
static bool number(const char* text,int base,os::u32& value) {
    if (base==16 && std::strlen(text)>2 && text[0]=='0' && (text[1]=='x'||text[1]=='X')) text+=2;
    const auto parsed=std::from_chars(text,text+std::strlen(text),value,base);
    return parsed.ec==std::errc{} && *parsed.ptr==0;
}
int main(int argc,char** argv) {
    using namespace os;
    if (argc!=5 && argc!=6) { std::cerr<<"usage: planet_record_inspect <explicit-planet-DBPF> <star-key-hex> <planet-count-decimal> <star-flags-hex> [--resource-counts]\n"; return 2; }
    const bool counts_only=argc==6&&std::string(argv[5])=="--resource-counts";
    if (argc==6&&!counts_only) { std::cerr<<"unknown option\n"; return 2; }
    u32 key=0,count=0,flags=0;
    if (!number(argv[2],16,key)||!number(argv[3],10,count)||count>255||!number(argv[4],16,flags)) {
        std::cerr<<"invalid key/count/flags\n"; return 2;
    }
    starsdb::StarRecord star; star.key=key; star.planet_count=(u8)count; star.flags=flags;
    starsdb::ExplicitDbpfReader reader; std::vector<planet::ResourceRecord> records; std::string error;
    if (!reader.open(argv[1],error)) {
        std::cerr<<"planet resource read blocked: "<<error<<'\n'; return 1;
    }
    if(counts_only){std::vector<pkg::ResourceKey> keys;if(!planet::resource_keys(star,keys,error))return 1;for(size_t i=0;i<keys.size();++i){auto r=reader.read_unique(keys[i]);if(r.status!=starsdb::ResourceReadStatus::Found){std::cerr<<"["<<i<<"] resource blocked: "<<r.error<<'\n';return 1;}std::cout<<"["<<i<<"] resource=0x"<<std::hex<<keys[i].instance<<std::dec<<" bytes="<<r.bytes.size()<<" first=";for(size_t n=0;n<r.bytes.size()&&n<16;++n)std::cout<<std::hex<<(unsigned)r.bytes[n]<<' ';std::cout<<std::dec;pkg::Package nested;if(r.bytes.size()>4&&r.bytes[4]=='D'&&r.bytes[5]=='B'&&r.bytes[6]=='P'&&r.bytes[7]=='F'&&nested.open_from_memory(r.bytes.data()+4,r.bytes.size()-4,"nested-planet")){std::cout<<" nested_records="<<nested.records().size();for(size_t j=0;j<nested.records().size()&&j<8;++j)std::cout<<" key["<<j<<"]=0x"<<std::hex<<nested.records()[j].instance()<<std::dec;}std::cout<<'\n';}return 0;}
    if (!planet::read_selected_records(reader,star,records,error)) {
        std::cerr<<"planet resource read blocked: "<<error<<'\n'; return 1;
    }
    std::cout<<"EXPLICIT PLANET STRUCTURAL SUBSET - no services, save discovery or galaxy readiness\n";
    for (size_t i=0;i<records.size();++i) {
        const auto& p=records[i];
        std::cout<<"["<<i<<"] resource=0x"<<std::hex<<p.resource.instance<<std::dec
                 <<" structural_frames="<<p.record.frames_walked<<" name_units=";
        if (p.record.name) std::cout<<p.record.name->size(); else std::cout<<"UNKNOWN";
        std::cout<<'\n';
    }
    planet::Selection selected;
    if (!planet::select_save_stem(star,records,selected,error)) { std::cerr<<"selection blocked: "<<error<<'\n'; return 1; }
    if (!selected.source_index) std::cout<<"no selected planet; not a ready saved metadata entry\n";
    else {
        std::cout<<"selected source_index="<<*selected.source_index<<" save_stem=\"";
        for (char16_t c:selected.save_stem) {
            if (c>=32 && c<127 && c!='"' && c!='\\') std::cout<<(char)c;
            else std::cout<<"\\u"<<std::hex<<(unsigned)c<<std::dec;
        }
        std::cout<<"\" (no path was constructed/opened)\n";
    }
    return 0;
}
