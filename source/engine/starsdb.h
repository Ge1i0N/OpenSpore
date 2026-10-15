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

// GA 0x13277D0 / StarRecord vtable 0x1466458. Only serialized fields, no constructor defaults,
// context identity, planet/citizen lookup or post-load side effects from 0xBBA6E0.
constexpr u32 kStarRecordInterface = 0x03e4353bu;
constexpr u32 kStarRecordClass = 0x03e4353cu;
struct StarRecord {
    serialized::Frame frame;
    std::optional<u32> key, type, tech_level, flags, starter_world_id, empire_id;
    std::optional<u32> last_observed_time, saved_game_version_major, saved_game_version_minor;
    std::optional<std::array<float,3>> position; // BE float bit patterns, not transformed/normalized
    std::optional<std::u16string> name;
    std::optional<std::array<u32,3>> citizen_species_key; // memory-word order; stream reverses the three words
    std::optional<std::array<u32,9>> saved_game_timestamp;
    std::optional<u8> planet_count;
};
// A standalone serializer frame at an EXPLICIT offset, not a search for embedded magic.
// A successful parse does not prove that the frame belongs to this class (tag is shared).
bool inspect_star_record(const std::vector<u8>& bytes, size_t offset, StarRecord& out, std::string& error);
}  // namespace os::starsdb
