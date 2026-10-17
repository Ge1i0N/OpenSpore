#include "engine/starsdb_dbpf.h"
#include <charconv>
#include <cstring>
#include <algorithm>
#include <iostream>
static bool hex(const char* text,os::u32& value) {
    if (std::strlen(text)>2 && text[0]=='0' && (text[1]=='x'||text[1]=='X')) text+=2;
    const auto result=std::from_chars(text,text+std::strlen(text),value,16);
    return result.ec==std::errc{} && *result.ptr=='\0';
}
static void print_star(const os::starsdb::StarRecord& s) {
    if (s.key) std::cout<<" key=0x"<<std::hex<<*s.key<<std::dec; else std::cout<<" key=unknown";
    if (s.flags) std::cout<<" flags=0x"<<std::hex<<*s.flags<<std::dec; else std::cout<<" flags=unknown";
    if (s.position) std::cout<<" pos="<<(*s.position)[0]<<','<<(*s.position)[1]<<','<<(*s.position)[2];
    if (s.planet_count) std::cout<<" planets="<<(unsigned)*s.planet_count; else std::cout<<" planets=unknown";
    if (s.tech_level) std::cout<<" tech=0x"<<std::hex<<*s.tech_level<<std::dec;
    if (s.type) std::cout<<" type=0x"<<std::hex<<*s.type<<std::dec;
    std::cout<<" name="<<(s.name?"present":"absent");
    if(s.name){std::cout<<"_utf16=";for(char16_t c:*s.name)std::cout<<"\\u"<<std::hex<<(unsigned)c;std::cout<<std::dec;}
    std::cout<<" fields="<<s.frame.fields.size();
}
int main(int argc,char** argv) {
    using namespace os; using namespace os::starsdb;
    if (argc!=5&&argc!=6) { std::cerr<<"usage: starsdb_dbpf_inspect <explicit-package> <root-instance> <root-type> <root-group> [class-id-hex]\n"; return 2; }
    u32 instance=0,type=0,group=0;
    if (!hex(argv[2],instance)||!hex(argv[3],type)||!hex(argv[4],group)) { std::cerr<<"invalid ResourceKey component\n"; return 2; }
    u32 selected_class=0;if(argc==6&&!hex(argv[5],selected_class)){std::cerr<<"invalid class id\n";return 2;}
    PhysicalContextResult result; std::string error;
    if (!inspect_physical_context(argv[1],instance,type,group,result,error)) {
        std::cerr<<"explicit DBPF context inspection blocked: "<<error<<"\n"; return 1;
    }
    std::cout<<"EXPLICIT DBPF ONLY - no profile scan, priority merge, writes or gameplay readiness\n";
    std::cout<<"root bytes="<<result.root.bytes.size()<<" index bytes="<<result.index.bytes.size()
             <<" classes="<<result.decoded_classes.size()<<"\n";
    for (size_t i=0;i<result.decoded_classes.size();++i)
        std::cout<<"class 0x"<<std::hex<<result.decoded_classes[i].class_id<<std::dec
                 <<" objects="<<result.decoded_classes[i].objects.size()<<"\n";
    if (result.manager_links) {
        const auto count=[](const auto& v){return v?std::to_string(v->size()):std::string("unknown");};
        std::cout<<"manager_links starters="<<count(result.manager_links->starters)
                 <<" saved="<<count(result.manager_links->saved)
                 <<" black_holes="<<count(result.manager_links->black_holes)
                 <<" possible_starts="<<count(result.manager_links->possible_starts)
                 <<" grid="<<count(result.manager_links->grid)<<"\n";
        const auto print_refs=[&](const char* label,const std::optional<ReferenceList>& refs) {
            if (!refs) { std::cout<<label<<"=unknown\n"; return; }
            std::cout<<label<<"_refs="<<refs->size()<<"\n";
            size_t shown_refs=0;
            for (const auto& ref:*refs) {
                if (shown_refs++>=16) { std::cout<<"  (reference preview limited to 16)\n"; break; }
                std::cout<<"  ref token=0x"<<std::hex<<ref.token<<" class=0x"<<ref.class_id<<std::dec;
                const auto it=std::find_if(result.decoded_classes.begin(),result.decoded_classes.end(),[&](const ClassObjects& c){return c.class_id==ref.class_id;});
                if (it==result.decoded_classes.end()) { std::cout<<" resolution=missing-class\n"; continue; }
                const auto resolved=resolve_star_reference(ref,*it);
                if (resolved.status!=ReferenceResolution::StarFields || resolved.object_index>=it->objects.size() || !it->objects[resolved.object_index].star) { std::cout<<" resolution=not-star\n"; continue; }
                std::cout<<" resolution=star["<<resolved.object_index<<"]"; print_star(*it->objects[resolved.object_index].star); std::cout<<'\n';
            }
        };
        print_refs("starters",result.manager_links->starters);
        print_refs("saved",result.manager_links->saved);
        print_refs("possible_starts",result.manager_links->possible_starts);
    }
    size_t shown=0;
    for (const auto& cls:result.decoded_classes) for (const auto& object:cls.objects) {
        if (!object.star||shown++>=16) continue;
        const auto& s=*object.star;
        std::cout<<"star_record class=0x"<<std::hex<<cls.class_id<<" token=0x"<<object.token<<std::dec; print_star(s); std::cout<<'\n';
    }
    if(shown>16)std::cout<<"star_record preview limited to 16 decoded objects\n";
    if(argc==6){for(const auto& cls:result.decoded_classes)if(cls.class_id==selected_class){std::cout<<"selected_class=0x"<<std::hex<<selected_class<<std::dec<<" objects="<<cls.objects.size()<<"\n";for(size_t i=0;i<cls.objects.size()&&i<32;++i)std::cout<<"  object["<<i<<"] token=0x"<<std::hex<<cls.objects[i].token<<" crc=0x"<<cls.objects[i].crc<<std::dec<<" offset="<<cls.objects[i].offset<<" body_offset="<<cls.objects[i].body_offset<<" end="<<cls.objects[i].end<<"\n";if(cls.objects.size()>32)std::cout<<"  object preview limited to 32\n";}}
    std::cout<<"physical resources and structural objects checked; post-load fixups/catalogue publication remain disabled\n";
    return 0;
}
