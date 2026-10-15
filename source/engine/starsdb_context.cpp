#include "engine/starsdb_context.h"
#include <unordered_set>
#include <utility>

namespace os::starsdb {
namespace {
constexpr size_t kMaxClasses=4096, kMaxObjects=1048576, kChecksumLimit=1000000;
u32 be32(const std::vector<u8>& bytes,size_t at) {
    return (u32(bytes[at])<<24) | (u32(bytes[at+1])<<16) | (u32(bytes[at+2])<<8) | bytes[at+3];
}
}
u32 object_crc(const u8* bytes,size_t size) {
    // GA 0x932CD0 / 0x932D70: normal (non-reflected) polynomial, init/final XOR all ones.
    u32 crc=0xffffffffu;
    for (size_t i=0; i<size; ++i) {
        crc^=u32(bytes[i])<<24;
        for (int bit=0; bit<8; ++bit) crc=(crc<<1)^((crc&0x80000000u)?0x04c11db7u:0u);
    }
    return ~crc;
}
bool read_context_index(const std::vector<u8>& bytes,ContextIndex& out,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (bytes.size()<12) return fail("truncated serializer context index");
    ContextIndex result;
    result.version=be32(bytes,0);
    const size_t classes=be32(bytes,4);
    result.declared_objects=be32(bytes,8);
    if (result.version!=1) return fail("unsupported serializer context index version");
    if (classes>kMaxClasses || result.declared_objects>kMaxObjects) return fail("context index exceeds host allocation limits");
    if (classes!=(bytes.size()-12)/8 || (bytes.size()-12)%8) return fail("context index count/size mismatch");
    std::unordered_set<u32> seen;
    size_t total=0;
    result.classes.reserve(classes);
    for (size_t i=0; i<classes; ++i) {
        ClassCount entry{be32(bytes,12+i*8),be32(bytes,16+i*8)};
        if (!seen.insert(entry.class_id).second) return fail("duplicate class in serializer context index");
        if (entry.count>kMaxObjects-total) return fail("context aggregate object count exceeds host limit");
        total+=entry.count;
        result.classes.push_back(entry);
    }
    if (total!=result.declared_objects) return fail("context declared object count differs from class sum");
    out=std::move(result);
    error.clear();
    return true;
}
bool read_class_objects(const std::vector<u8>& bytes,const ClassCount& expected,
                        ClassObjects& out,std::string& error) {
    const auto fail=[&](const char* why) { error=why; return false; };
    if (expected.count>kMaxObjects) return fail("class object count exceeds host limit");
    if (expected.count>bytes.size()/12) return fail("class object count exceeds minimum record size");
    ClassObjects result;
    result.class_id=expected.class_id;
    result.objects.reserve(expected.count);
    std::unordered_set<u32> seen;
    size_t at=0;
    for (u32 i=0; i<expected.count; ++i) {
        if (bytes.size()-at<12) return fail("truncated context object header");
        const size_t length=be32(bytes,at);
        if (length<12 || length>bytes.size()-at) return fail("context object length outside class stream");
        ObjectBody object;
        object.offset=at; object.end=at+length; object.body_offset=at+12;
        object.crc=be32(bytes,at+4); object.token=be32(bytes,at+8);
        if (!object.token) return fail("context object has null reference token");
        if (!seen.insert(object.token).second) return fail("duplicate token in class object stream");
        const size_t crc_size=length-8<kChecksumLimit ? length-8 : kChecksumLimit;
        if (object_crc(bytes.data()+at+8,crc_size)!=object.crc) return fail("context object checksum mismatch");
        if (expected.class_id==kStarRecordClass) {
            // Bound the shared-tag frame BEFORE its reader can inspect adjacent records.
            const size_t body_size=length-12;
            if (body_size<12 || be32(bytes,object.body_offset+4)!=body_size-8)
                return fail("StarRecord frame length differs from context object body");
            StarRecord star;
            if (!inspect_star_record(bytes,object.body_offset,star,error)) return false;
            object.star=std::move(star);
        }
        result.objects.push_back(std::move(object));
        at+=length;
    }
    if (at!=bytes.size()) return fail("class object stream has unconsumed bytes");
    out=std::move(result);
    error.clear();
    return true;
}
ResolvedReference resolve_star_reference(const ObjectReference& ref,const ClassObjects& objects) {
    if (ref.is_null()) return {ReferenceResolution::Null,0};
    if (ref.class_id!=objects.class_id) return {ReferenceResolution::MissingClassStream,0};
    for (size_t i=0; i<objects.objects.size(); ++i) {
        const auto& object=objects.objects[i];
        if (object.token==ref.token) {
            if (ref.class_id!=kStarRecordClass || !object.star) return {ReferenceResolution::UnsupportedClass,i};
            return {ReferenceResolution::StarFields,i};
        }
    }
    return {ReferenceResolution::MissingObject,0};
}
}  // namespace os::starsdb
