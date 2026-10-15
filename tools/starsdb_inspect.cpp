// Read-only, caller-selected file inspector. Never writes to a Spore save or scans a user profile.
#include "engine/starsdb_links.h"
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
static void scalar(const char* label,const std::optional<os::u32>& value) {
    std::cout<<label<<'=';
    if (value) std::cout<<*value; else std::cout<<"(absent)";
    std::cout<<'\n';
}
static void refs(const char* label,const std::optional<os::starsdb::ReferenceList>& list) {
    std::cout<<label<<'=';
    if (!list) { std::cout<<"(absent/unknown)\n"; return; }
    std::cout<<list->size()<<" wire references (UNRESOLVED)\n";
    // The decoder validates all references; console preview is deliberately bounded.
    for (size_t i=0; i<list->size() && i<16; ++i) {
        const auto& ref=(*list)[i];
        std::cout<<"  ["<<i<<"] token=0x"<<std::hex<<ref.token<<" class=0x"<<ref.class_id<<std::dec
                 <<(ref.is_null()?" NULL":" UNRESOLVED")<<'\n';
    }
    if (list->size()>16) std::cout<<"  (preview limited to 16 entries)\n";
}
int main(int argc,char** argv) {
    using namespace os;
    const std::string mode=argc==3 ? argv[2] : "";
    if ((argc!=2 && argc!=3) || (!mode.empty() && mode!="--links" && mode!="--star-record")) {
        std::cerr<<"usage: starsdb_inspect <explicit-local-file> [--links|--star-record] (read-only)\n"; return 2;
    }
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    if (!input) { std::cerr<<"cannot open requested file\n"; return 1; }
    const auto size=input.tellg();
    if (size<0 || size>256*1024*1024) { std::cerr<<"file exceeds 256 MiB host inspection limit\n"; return 1; }
    std::vector<u8> bytes((size_t)size);
    input.seekg(0);
    if (!bytes.empty() && !input.read((char*)bytes.data(),(std::streamsize)bytes.size())) {
        std::cerr<<"short file read\n"; return 1;
    }
    std::string error;
    if (mode=="--star-record") {
        starsdb::StarRecord star;
        if (!starsdb::inspect_star_record(bytes,0,star,error)) { std::cerr<<"StarRecord fields rejected: "<<error<<'\n'; return 1; }
        std::cout<<"CALLER-SELECTED STARRECORD FRAME ONLY - class/identity NOT verified, no object graph or galaxy readiness\n";
        std::cout<<"fields="<<star.frame.fields.size()<<" unvalidated_tail="<<bytes.size()-star.frame.end<<'\n';
        scalar("key",star.key); scalar("type",star.type); scalar("techLevel",star.tech_level); scalar("flags",star.flags);
        scalar("starterWorldID",star.starter_world_id); scalar("empireID",star.empire_id);
        scalar("savedGameVersionMajor",star.saved_game_version_major); scalar("savedGameVersionMinor",star.saved_game_version_minor);
        std::cout<<"name="; text(star.name); std::cout<<'\n';
        if (star.position) std::cout<<"position="<<(*star.position)[0]<<','<<(*star.position)[1]<<','<<(*star.position)[2]<<'\n';
        return 0;
    }
    starsdb::Prefix prefix;
    if (!starsdb::inspect_prefix(bytes,prefix,error)) { std::cerr<<"prefix rejected: "<<error<<'\n'; return 1; }
    std::cout<<"STRUCTURAL PREFIX ONLY - not full stars.db validation, original-runtime parity or galaxy readiness\n";
    std::cout<<"metadata fields="<<prefix.metadata.fields.size()<<" manager fields="<<prefix.manager.fields.size()
             <<" consumed="<<prefix.consumed<<" unvalidated_tail="<<prefix.remaining<<'\n';
    scalar("savedGameVersionOnSave",prefix.header.saved_game_version_on_save);
    scalar("savedGameVersionOnGenerate",prefix.header.saved_game_version_on_generate);
    std::cout<<"buildVersionOnSave="; text(prefix.header.build_version_on_save); std::cout<<'\n';
    std::cout<<"buildVersionOnGenerate="; text(prefix.header.build_version_on_generate); std::cout<<'\n';
    for (const auto& field : prefix.manager.fields) {
        const auto* name=starsdb::manager_field_name(field.id);
        std::cout<<"manager field 0x"<<std::hex<<field.id<<std::dec<<" "<<(name?name:"UNKNOWN")
                 <<" offset="<<field.offset<<" bytes="<<field.size<<" (opaque)\n";
    }
    if (mode=="--links") {
        starsdb::ManagerLinks links;
        if (!starsdb::inspect_manager_links(bytes,prefix.manager.offset,links,error)) {
            std::cerr<<"manager wire links rejected: "<<error<<'\n'; return 1;
        }
        refs("mStarterStarRecords",links.starters); refs("mSavedGameStarRecords",links.saved);
        refs("mBlackHoles",links.black_holes); refs("mPossibleStartLocations",links.possible_starts);
        if (!links.grid) std::cout<<"mStarRecordGrid=(absent/unknown)\n";
        else {
            size_t count=0;
            for (const auto& cell : *links.grid) count+=cell.size();
            std::cout<<"mStarRecordGrid="<<links.grid->size()<<" serialized cells, "<<count<<" UNRESOLVED references\n";
        }
        std::cout<<"No reference was dereferenced, no record catalogue was published\n";
    }
    return 0;
}
