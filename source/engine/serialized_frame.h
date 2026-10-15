#pragma once
#include "common.h"
#include <string>
#include <vector>

namespace os::serialized {
constexpr u32 kObjectTag = 0x01a80d26u;
struct FieldSpan {
    u32 id = 0;
    size_t offset = 0, size = 0;
};
struct Frame {
    u32 tag = 0;
    size_t offset = 0, end = 0;
    std::vector<FieldSpan> fields;
};

// GA 0x693DE0 / 0x6928C0: BE tag, BE byte length AFTER the first 8 bytes, BE field count,
// then count {BE id, BE size} pairs and sequential payloads. Offsets refer to caller-owned bytes.
// Bounded structural subset; unknown/duplicate IDs and zero-length fields remain inspectable.
// No pointer resolution or original object allocation. On failure out is unchanged.
bool parse_frame(const std::vector<u8>& bytes, size_t offset, u32 expected_tag, Frame& out, std::string& error);
}  // namespace os::serialized
