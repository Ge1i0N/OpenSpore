#pragma once
#include "engine/starsdb_dbpf.h"
#include "engine/gge_catalog.h"

namespace os::save {
constexpr u32 kRoot=0x01897c18u;
struct FileHeader {
    serialized::Frame frame;
    std::optional<u32> version_major, version_minor, game_mode;
    std::optional<std::array<u32,9>> timestamp;
    std::optional<std::array<u32,3>> current_planet_key; // memory order instance,type,group
    size_t unvalidated_tail=0;
    // object-count map and snapshot flag remain opaque spans, not guessed schema/defaults.
};
// GA descriptor 0x1568B08 built at 0x131B210. One first frame, NOT the stars.db metadata schema.
bool read_header(const std::vector<u8>& bytes,FileHeader& out,std::string& error);
struct HeaderProbe { bool predicate=false; gge::SaveHeaderWords words; };
// Explicit decoded version words required; no invented constructor defaults on omitted fields.
// Native 0xB27838 CMP major,0x21 / SETAE is unsigned, not success or ownership validation.
bool header_probe(const FileHeader& header,HeaderProbe& out,std::string& error);
// Explicit already-selected DBPF only. Does NOT load context/classes, derive save stems or scan paths.
bool read_selected_header(starsdb::ExplicitDbpfReader& reader,FileHeader& out,std::string& error);
}  // namespace os::save
