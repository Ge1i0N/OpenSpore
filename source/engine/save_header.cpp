#include "engine/save_header.h"
#include <utility>

namespace os::save {
namespace {
u32 be32(const std::vector<u8>& bytes,size_t at) {
    return (u32(bytes[at])<<24)|(u32(bytes[at+1])<<16)|(u32(bytes[at+2])<<8)|bytes[at+3];
}
}
bool read_header(const std::vector<u8>& bytes,FileHeader& out,std::string& error) {
    FileHeader result;
    if (!serialized::parse_frame(bytes,0,serialized::kObjectTag,result.frame,error)) return false;
    for (const auto& field:result.frame.fields) {
        if (!field.size) continue;
        switch (field.id) {
        case 0x04162d88: case 0x04162d89: case 0x04162d8c: {
            if (field.size!=4) { error="save header version/mode field must be one BE word"; return false; }
            auto& value=field.id==0x04162d88?result.version_major:
                        field.id==0x04162d89?result.version_minor:result.game_mode;
            value=be32(bytes,field.offset); break;
        }
        case 0x04162d8b: {
            if (field.size!=36) { error="save header timestamp must contain nine BE words"; return false; }
            std::array<u32,9> words{};
            for (size_t i=0;i<9;++i) words[i]=be32(bytes,field.offset+i*4);
            result.timestamp=words; break;
        }
        case 0x064be534:
            if (field.size!=12) { error="save header planet key must contain three BE words"; return false; }
            result.current_planet_key={{be32(bytes,field.offset+8),be32(bytes,field.offset+4),be32(bytes,field.offset)}};
            break;
        default: break;
        }
    }
    result.unvalidated_tail=bytes.size()-result.frame.end;
    out=std::move(result); error.clear(); return true;
}
bool header_probe(const FileHeader& header,HeaderProbe& out,std::string& error) {
    if (!header.version_major || !header.version_minor) { error="save header version words unobserved"; return false; }
    HeaderProbe result;
    result.words={*header.version_major,*header.version_minor};
    result.predicate=*header.version_major>=0x21u;
    out=result; error.clear(); return true;
}
bool read_selected_header(starsdb::ExplicitDbpfReader& reader,FileHeader& out,std::string& error) {
    auto resource=reader.read_unique({kRoot,kRoot,kRoot});
    if (resource.status!=starsdb::ResourceReadStatus::Found) { error=resource.error; return false; }
    return read_header(resource.bytes,out,error);
}
}  // namespace os::save
