#pragma once
#include "engine/starsdb_dbpf.h"

namespace os::planet {
constexpr u32 kResourceType=0x05220cb8u, kResourceGroup=1u;
// The nested planet DBPF holds exactly one record, and the shipped file keys it
// (instance 1, type 1, group 1) - NOT the outer planet record type.
constexpr pkg::ResourceKey kNestedPlanetKey{1,1,1};
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
    struct Envelope {
        // Observed BE halves of 00 25 00 03. Not native version/compatibility validation.
        u16 prefix_first=0, prefix_second=0;
        pkg::ResourceKey inner_resource;
        size_t inner_container_bytes=0, inner_index_end=0, container_tail_bytes=0;
    };
    // Absent for a direct counted payload; present only after exact nested-key validation.
    std::optional<Envelope> envelope;
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
// Nested subset: observed prefix00250003 + DBPF3.0/index3, sole exact inner key(1,1,1).
// Bounds/read flags and transport+decoded batch caps enforced; container tail retained, not decoded.
bool read_selected_records(starsdb::ExplicitDbpfReader& reader,const starsdb::StarRecord& star,
                           std::vector<ResourceRecord>& out,std::string& error);
}  // namespace os::planet
