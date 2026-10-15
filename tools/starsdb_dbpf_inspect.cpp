#include "engine/starsdb_dbpf.h"
#include <charconv>
#include <cstring>
#include <iostream>
static bool hex(const char* text,os::u32& value) {
    if (std::strlen(text)>2 && text[0]=='0' && (text[1]=='x'||text[1]=='X')) text+=2;
    const auto result=std::from_chars(text,text+std::strlen(text),value,16);
    return result.ec==std::errc{} && *result.ptr=='\0';
}
int main(int argc,char** argv) {
    using namespace os; using namespace os::starsdb;
    if (argc!=5) { std::cerr<<"usage: starsdb_dbpf_inspect <explicit-package> <root-instance> <root-type> <root-group>\n"; return 2; }
    u32 instance=0,type=0,group=0;
    if (!hex(argv[2],instance)||!hex(argv[3],type)||!hex(argv[4],group)) { std::cerr<<"invalid ResourceKey component\n"; return 2; }
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
    std::cout<<"physical resources and structural objects checked; post-load fixups/catalogue publication remain disabled\n";
    return 0;
}
