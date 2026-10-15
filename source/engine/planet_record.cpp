#include "engine/planet_record.h"
#include <utility>

namespace os::planet {
namespace {
constexpr size_t kMaxNodes=4096, kMaxName=4096;
u32 be32(const std::vector<u8>& b,size_t at) {
    return (u32(b[at])<<24)|(u32(b[at+1])<<16)|(u32(b[at+2])<<8)|b[at+3];
}
class Walker {
public:
    explicit Walker(const std::vector<u8>& bytes,std::string& why): b(bytes),error(why) {}
    bool count(u32& count) {
        if (b.size()-at<4) { error="truncated planet collection count"; return false; }
        count=be32(b,at); at+=4;
        if (count>kMaxNodes-nodes) { error="planet collections exceed host aggregate node limit 4096"; return false; }
        nodes+=count;
        return true;
    }
    bool frame() {
        serialized::Frame f;
        if (!serialized::parse_frame(b,at,serialized::kObjectTag,f,error)) return false;
        at=f.end; ++frames; return true;
    }
    bool civilization() {
        u32 cities=0,vehicles=0;
        if (!count(cities)) return false;
        for (u32 i=0;i<cities;++i) {
            if (b.size()-at<28) { error="truncated city fourteen-word prefix"; return false; }
            at+=28; // GA 0xFF0380: 14 * scalar u16, NOT a field frame or its length.
            if (!frame()) return false;
        }
        if (!count(vehicles)) return false;
        for (u32 i=0;i<vehicles;++i) if (!frame()) return false;
        return frame(); // civilization's own fields after its two collections
    }
    const std::vector<u8>& b;
    std::string& error;
    size_t at=0,nodes=0,frames=0;
};
bool bad_data(const std::u16string& text) {
    const auto end=text.find(char16_t{0});
    return text.compare(0,end==std::u16string::npos?text.size():end,u"BAD_DATA")==0;
}
}
bool read_record(const std::vector<u8>& bytes,Record& out,std::string& error) {
    Record result; Walker walk(bytes,error);
    if (!walk.count(result.civilizations)) return false;
    for (u32 i=0;i<result.civilizations;++i) if (!walk.civilization()) return false;
    for (u32* count:{&result.tribes,&result.events,&result.commodity_nodes}) {
        if (!walk.count(*count)) return false;
        for (u32 i=0;i<*count;++i) if (!walk.frame()) return false;
    }
    if (!serialized::parse_frame(bytes,walk.at,serialized::kObjectTag,result.fields,error)) return false;
    result.frames_walked=walk.frames+1;
    result.unvalidated_tail=bytes.size()-result.fields.end;
    for (const auto& field:result.fields.fields) {
        if (!field.size) continue;
        switch (field.id) {
        case 0x04164f8e: case 0x03d58b9e: case 0x03d58b9f: case 0x03d58b99: {
            if (field.size!=4) { error="planet key/type/stage/flags must contain one BE word"; return false; }
            auto& target=field.id==0x04164f8e?result.key:field.id==0x03d58b9e?result.type:
                         field.id==0x03d58b9f?result.tech_level:result.flags;
            target=be32(bytes,field.offset); break;
        }
        case 0x03d58b9a: {
            if (field.size<4) { error="truncated planet name length"; return false; }
            const size_t count=be32(bytes,field.offset);
            if (count>kMaxName || count!=(field.size-4)/2 || (field.size-4)%2) {
                error="planet name length/host limit mismatch"; return false;
            }
            std::u16string name; name.reserve(count);
            for (size_t i=0;i<count;++i) {
                const size_t at=field.offset+4+i*2;
                name.push_back((char16_t)(u32(bytes[at])|(u32(bytes[at+1])<<8)));
            }
            result.name=std::move(name); break;
        }
        case 0x03d58b9b:
            if (field.size!=12) { error="planet terrain key must contain three BE words"; return false; }
            result.generated_terrain_key={{be32(bytes,field.offset+8),be32(bytes,field.offset+4),be32(bytes,field.offset)}};
            break;
        default: break;
        }
    }
    out=std::move(result); error.clear(); return true;
}
bool resource_keys(const starsdb::StarRecord& star,std::vector<pkg::ResourceKey>& out,std::string& error) {
    if (!star.planet_count || (*star.planet_count && !star.key)) {
        error="star planet count/key unavailable"; return false;
    }
    std::vector<pkg::ResourceKey> result; result.reserve(*star.planet_count);
    for (u32 i=0;i<*star.planet_count;++i) result.push_back({*star.key+i*0x01000000u,kResourceType,kResourceGroup});
    out=std::move(result); error.clear(); return true;
}
bool select_save_stem(const starsdb::StarRecord& star,const std::vector<ResourceRecord>& records,
                      Selection& out,std::string& error) {
    std::vector<pkg::ResourceKey> expected;
    if (!resource_keys(star,expected,error)) return false;
    if (!star.flags || records.size()!=expected.size()) { error="star flags/complete counted planet observations unavailable"; return false; }
    // Validate key identity for every observation before selecting; don't return a partial first item.
    for (size_t i=0;i<records.size();++i) {
        if (records[i].record.unvalidated_tail) { error="counted planet observation has unsupported tail"; return false; }
        const auto& key=records[i].resource;
        if (key.instance!=expected[i].instance || key.type!=expected[i].type || key.group!=expected[i].group) {
            error="planet observations do not match original counted ResourceKey order"; return false;
        }
    }
    Selection result;
    for (size_t i=0;i<records.size();++i) {
        const auto& planet=records[i];
        if (*star.flags&0x40u) {
            if (!planet.record.flags) { error="planet filter flags unavailable"; return false; }
            if (*planet.record.flags&0x100u) continue;
        }
        if (!result.source_index) result.source_index=i;
        // Original filtered vector tests all planets; unknown later flags must not be ignored.
    }
    if (result.source_index) {
        const auto& source=records[*result.source_index];
        if (!source.record.name) { error="selected planet runtime name unobserved"; return false; }
        if (source.record.name->size()>kMaxName) { error="selected planet name exceeds host text limit"; return false; }
        result.save_stem=*source.record.name;
        if (bad_data(result.save_stem)) {
            if (!source.record.flags) { error="selected BAD_DATA planet flags unavailable"; return false; }
            if (!(*source.record.flags&1u)) {
                if (!source.generated_name) { error="BAD_DATA planet needs unimplemented name generation observation"; return false; }
                if (source.generated_name->size()>kMaxName) { error="generated planet name exceeds host text limit"; return false; }
                if (!source.generated_name->empty()) result.save_stem=*source.generated_name;
            }
        }
    }
    out=std::move(result); error.clear(); return true;
}
bool read_selected_records(starsdb::ExplicitDbpfReader& reader,const starsdb::StarRecord& star,
                           std::vector<ResourceRecord>& out,std::string& error) {
    std::vector<pkg::ResourceKey> keys;
    if (!resource_keys(star,keys,error)) return false;
    std::vector<ResourceRecord> result; result.reserve(keys.size());
    size_t aggregate=0;
    for (const auto& key:keys) {
        auto resource=reader.read_unique(key);
        if (resource.status!=starsdb::ResourceReadStatus::Found) { error=resource.error; return false; }
        if (resource.bytes.size()>64*1024*1024-aggregate) { error="planet batch exceeds 64 MiB host limit"; return false; }
        aggregate+=resource.bytes.size();
        ResourceRecord planet; planet.resource=key;
        if (!read_record(resource.bytes,planet.record,error)) return false;
        if (planet.record.unvalidated_tail) { error="planet resource has unsupported trailing bytes"; return false; }
        result.push_back(std::move(planet));
    }
    out=std::move(result); error.clear(); return true;
}
}  // namespace os::planet
