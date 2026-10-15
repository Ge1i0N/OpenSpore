#pragma once
#include "engine/serialized_frame.h"
#include <array>
#include <optional>

namespace os::starsdb {
struct Header {
    std::optional<u32> saved_game_version_on_save, saved_game_version_on_generate;
    std::optional<std::u16string> build_version_on_save, build_version_on_generate;
    // Nine raw 32-bit tm components; interpreting calendar validity is not required by the GA reader.
    std::optional<std::array<u32,9>> timestamp_on_save, timestamp_on_generate;
};
struct Prefix {
    serialized::Frame metadata, manager;
    Header header;
    size_t consumed = 0, remaining = 0; // rest of original file is NOT decoded/validated here
};

// Parse only the first two serialized frames identified by GA 0xBB8C90 / 0xBB4D40.
// Metadata fields from static descriptor constructor 0x1325DB0; manager fields are opaque spans.
// This is NOT a complete stars.db reader and cannot publish a ready galaxy/catalogue.
bool inspect_prefix(const std::vector<u8>& bytes, Prefix& out, std::string& error);
const char* manager_field_name(u32 id); // observed descriptor names; unknown returns nullptr
}  // namespace os::starsdb
