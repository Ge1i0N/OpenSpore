// Explicit read-only package, never a directory/profile search or full save loader.
#include "engine/save_header.h"
#include <iostream>
int main(int argc,char** argv) {
    if (argc!=2) { std::cerr<<"usage: save_header_inspect <explicit-save-DBPF>\n"; return 2; }
    os::starsdb::ExplicitDbpfReader reader; os::save::FileHeader header; os::save::HeaderProbe probe;
    std::string error;
    if (!reader.open(argv[1],error) || !os::save::read_selected_header(reader,header,error) ||
        !os::save::header_probe(header,probe,error)) { std::cerr<<"save header blocked: "<<error<<'\n'; return 1; }
    std::cout<<"EXPLICIT FIRST HEADER SUBSET - not a full save/compatibility/ownership/gameplay check\n"
        <<"mFileVersionMajor="<<probe.words.first<<" mFileVersionMinor="<<probe.words.second
        <<" unsigned_major_ge_33="<<probe.predicate<<" unvalidated_root_tail="<<header.unvalidated_tail<<'\n';
    return 0;
}
