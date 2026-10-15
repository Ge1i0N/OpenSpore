#pragma once
#include "engine/planet_record.h"
#include "engine/save_header.h"

namespace os::gge {
enum class TerrainQueryObservation { Unknown, QueryFailed, Loaded };
struct SavedEntryObservations {
    TerrainQueryObservation terrain=TerrainQueryObservation::Unknown;
    // Loaded list requires explicit property result, including zero for observed missing/wrong type.
    std::optional<u32> terrain_image;
    std::optional<save::HeaderProbe> header_probe;
};
// Serialized fields -> typed saved metadata, only after required external observations are supplied.
// No save discovery, filename/path creation, header I/O, terrain/name service, candidate generation
// or live catalogue availability. Empty/all-filtered planet list deliberately blocks the adapter.
bool adapt_saved_metadata(const starsdb::StarRecord& star,const std::vector<planet::ResourceRecord>& planets,
                          const SavedEntryObservations& observed,SavedGameMetadata& out,std::string& error);
}  // namespace os::gge
