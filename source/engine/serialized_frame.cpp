#include "engine/serialized_frame.h"
#include <utility>

namespace os::serialized {
namespace {
u32 be32(const std::vector<u8>& bytes, size_t at) {
    return (u32(bytes[at])<<24) | (u32(bytes[at+1])<<16) | (u32(bytes[at+2])<<8) | bytes[at+3];
}
}
bool parse_frame(const std::vector<u8>& bytes, size_t offset, u32 expected, Frame& out, std::string& error) {
    const auto fail = [&](const char* why) { error = why; return false; };
    if (offset > bytes.size() || bytes.size()-offset < 12) return fail("truncated serialized frame header");
    Frame result;
    result.offset = offset;
    result.tag = be32(bytes, offset);
    if (result.tag != expected) return fail("serialized frame tag mismatch");
    const size_t length = be32(bytes, offset+4);
    if (length < 4 || length > bytes.size()-offset-8) return fail("serialized frame length outside input");
    result.end = offset+8+length;
    const size_t count = be32(bytes, offset+8);
    // Original descriptor registry is bounded at 128; reject larger input tables instead of
    // reproducing the original reader's unchecked fixed stack table. This is a host safety policy.
    if (count > 128) return fail("serialized field table exceeds host limit 128");
    if (count > (length-4)/8) return fail("truncated serialized field directory");
    size_t payload = offset+12+count*8;
    result.fields.reserve(count);
    for (size_t i=0; i<count; ++i) {
        const size_t directory = offset+12+i*8;
        const u32 id = be32(bytes,directory);
        const size_t size = be32(bytes,directory+4);
        if (size > result.end-payload) return fail("serialized field payload exceeds frame");
        result.fields.push_back({id,payload,size});
        payload += size;
    }
    if (payload != result.end) return fail("serialized directory sizes do not consume frame");
    out = std::move(result);
    error.clear();
    return true;
}
}  // namespace os::serialized
