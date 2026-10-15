// Read-only explicit DECODED resource streams. Not a DBPF extractor or automatic save loader.
#include "engine/starsdb_context.h"
#include <fstream>
#include <iostream>
#include <charconv>
#include <cstring>

static bool load(const char* path,std::vector<os::u8>& out) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) return false;
    const auto size=file.tellg();
    if (size<0 || size>256*1024*1024) return false;
    out.resize((size_t)size); file.seekg(0);
    return out.empty() || (bool)file.read((char*)out.data(),(std::streamsize)out.size());
}
int main(int argc,char** argv) {
    using namespace os;
    using namespace os::starsdb;
    if (argc!=4) {
        std::cerr<<"usage: starsdb_context_inspect <decoded-index-stream> <class-id-hex> <decoded-class-stream>\n"; return 2;
    }
    const char* hex=argv[2];
    if (std::strlen(hex)>2 && hex[0]=='0' && (hex[1]=='x' || hex[1]=='X')) hex+=2;
    u32 class_id=0;
    const auto parsed=std::from_chars(hex,hex+std::strlen(hex),class_id,16);
    if (parsed.ec!=std::errc{} || *parsed.ptr) { std::cerr<<"invalid class ID\n"; return 2; }
    std::vector<u8> index_bytes,objects_bytes;
    if (!load(argv[1],index_bytes) || !load(argv[3],objects_bytes)) { std::cerr<<"cannot read explicit stream (256 MiB host cap)\n"; return 1; }
    ContextIndex index;
    ClassObjects objects;
    std::string error;
    if (!read_context_index(index_bytes,index,error)) { std::cerr<<"index rejected: "<<error<<'\n'; return 1; }
    const ClassCount* entry=nullptr;
    for (const auto& cls : index.classes) if (cls.class_id==class_id) entry=&cls;
    if (!entry) { std::cerr<<"requested class is not declared by index\n"; return 1; }
    if (!read_class_objects(objects_bytes,*entry,objects,error)) { std::cerr<<"objects rejected: "<<error<<'\n'; return 1; }
    std::cout<<"EXPLICIT DECODED STREAM SUBSET - not physical stars.db validation, resource fixups or galaxy readiness\n";
    std::cout<<"index version="<<index.version<<" classes="<<index.classes.size()<<" declared_objects="<<index.declared_objects
             <<"; checked class=0x"<<std::hex<<class_id<<std::dec<<" objects="<<objects.objects.size()<<" CRC=PASS\n";
    for (size_t i=0; i<objects.objects.size() && i<32; ++i) {
        const auto& object=objects.objects[i];
        std::cout<<"["<<i<<"] token=0x"<<std::hex<<object.token<<std::dec<<" offset="<<object.offset<<" end="<<object.end;
        if (!object.star) std::cout<<" body=OPAQUE";
        else if (object.star->key) std::cout<<" StarRecord.mKey="<<*object.star->key;
        else std::cout<<" StarRecord.mKey=ABSENT";
        std::cout<<'\n';
    }
    if (objects.objects.size()>32) std::cout<<"(preview limited to 32 records)\n";
    std::cout<<"Only the selected class stream was checked; other indexed classes are not claimed loaded\n";
    return 0;
}
