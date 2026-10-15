// Read-only, caller-selected file inspector. Never writes to a Spore save or scans a user profile.
#include "engine/starsdb.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

static void text(const std::optional<std::u16string>& s) {
    if (!s) { std::cout<<"(absent)"; return; }
    std::cout<<'"';
    for (char16_t c : *s) {
        if (c>=32 && c<127 && c!='"' && c!='\\') std::cout<<(char)c;
        else std::cout<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<(unsigned)c<<std::dec;
    }
    std::cout<<'"';
}
int main(int argc,char** argv) {
    using namespace os;
    if (argc!=2) { std::cerr<<"usage: starsdb_inspect <explicit-local-file> (read-only prefix inspection)\n"; return 2; }
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if (!input) { std::cerr<<"cannot open requested file\n"; return 1; }
    const auto size=input.tellg();
    if (size<0 || size>256*1024*1024) { std::cerr<<"file exceeds 256 MiB host inspection limit\n"; return 1; }
    std::vector<u8> bytes((size_t)size);
    input.seekg(0);
    if (!bytes.empty() && !input.read((char*)bytes.data(),(std::streamsize)bytes.size())) {
        std::cerr<<"short file read\n"; return 1;
    }
    starsdb::Prefix prefix;
    std::string error;
    if (!starsdb::inspect_prefix(bytes,prefix,error)) { std::cerr<<"prefix rejected: "<<error<<'\n'; return 1; }
    std::cout<<"STRUCTURAL PREFIX ONLY - not full stars.db validation, original-runtime parity or galaxy readiness\n";
    std::cout<<"metadata fields="<<prefix.metadata.fields.size()<<" manager fields="<<prefix.manager.fields.size()
             <<" consumed="<<prefix.consumed<<" unvalidated_tail="<<prefix.remaining<<'\n';
    const auto version=[](const char* label,const std::optional<u32>& value) {
        std::cout<<label<<'=';
        if (value) std::cout<<*value; else std::cout<<"(absent)";
        std::cout<<'\n';
    };
    version("savedGameVersionOnSave",prefix.header.saved_game_version_on_save);
    version("savedGameVersionOnGenerate",prefix.header.saved_game_version_on_generate);
    std::cout<<"buildVersionOnSave="; text(prefix.header.build_version_on_save); std::cout<<'\n';
    std::cout<<"buildVersionOnGenerate="; text(prefix.header.build_version_on_generate); std::cout<<'\n';
    for (const auto& field : prefix.manager.fields) {
        const auto* name=starsdb::manager_field_name(field.id);
        std::cout<<"manager field 0x"<<std::hex<<field.id<<std::dec<<" "<<(name?name:"UNKNOWN")
                 <<" offset="<<field.offset<<" bytes="<<field.size<<" (opaque)\n";
    }
    return 0;
}
