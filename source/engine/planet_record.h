#pragma once
#include "engine/starsdb_dbpf.h"

namespace os::planet {
constexpr u32 kResourceType=0x05220cb8u, kResourceGroup=1u;
struct Record {
    serialized::Frame fields;
    std::optional<u32> key, type, tech_level, flags;
    std::optional<std::u16string> name;
    std::optional<std::array<u32,3>> generated_terrain_key;
    // Counts are observed, but payloads are only structurally walked, not gameplay-decoded.
    u32 civilizations=0, tribes=0, events=0, commodity_nodes=0;
    size_t frames_walked=0, unvalidated_tail=0;
};
// GA 0xB8EBF0: four counted structural collections, then PlanetRecord field frame.
// City nested prefix is 14 BE u16 words. No magic search, allocation of game objects or name generation.
bool read_record(const std::vector<u8>& bytes,Record& out,std::string& error);
struct ResourceRecord {
    pkg::ResourceKey resource;
    Record record;
    // An explicitly observed result from native-style name generation, not a replacement name.
    // Only needed for selected BAD_DATA when flags bit0 permits assignment; empty result keeps marker.
    std::optional<std::u16string> generated_name;
};
struct Selection {
    std::optional<size_t> source_index; // absent for an observed empty/all-filtered list
    std::u16string save_stem;
};
// Original lazy materialization uses (star.mKey+i*0x01000000, type, group), uint32 wrap.
// Host requires all counted observations and keys to match, never substitutes Unknown with empty.
bool resource_keys(const starsdb::StarRecord& star,std::vector<pkg::ResourceKey>& out,std::string& error);
bool select_save_stem(const starsdb::StarRecord& star,const std::vector<ResourceRecord>& records,
                      Selection& out,std::string& error);
// One explicitly selected planet-record DBPF; no fallback across packages or save/profile scans.
// Materializes counted records only; generation, selection and live readiness are separate.
bool read_selected_records(starsdb::ExplicitDbpfReader& reader,const starsdb::StarRecord& star,
                           std::vector<ResourceRecord>& out,std::string& error);
// Real planetRecords.pkp transport: each resource is a 4-byte prefix (constant
// 0x00250003 on every observed record; meaning unestablished) followed by a nested
// DBPF v3 holding exactly 1 record whose bytes are the read_record input.
// Direct (unwrapped) bytes keep parsing as before, so constructed fixtures stay valid.
bool read_transport_record(const std::vector<u8>& bytes,Record& out,std::string& error);
}  // namespace os::planet
