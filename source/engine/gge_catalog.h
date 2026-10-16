#pragma once
#include "common.h"
#include <string>
#include <vector>

namespace os::gge {
struct StarPosition { float x = 0, y = 0, z = 0; };
struct ImageKey { u32 instance = 0, type = 0, group = 0; };
// GA save-header descriptor 0x1568B08: file major/minor, NOT StarRecord saved versions or identity.
struct SaveHeaderWords { u32 first = 0, second = 0; };

// Typed metadata supplied by a real catalogue adapter, NOT pointers into a serialized 0xBC record.
// Resource/properties are already resolved by the producer; false terrain_loaded means a real
// failed property-list query, not permission to silently substitute an unavailable producer.
struct StarMetadata {
    u32 star_id = 0;               // source record +0x70, not the per-list source index
    StarPosition position;         // source +0x3C, copied unchanged into GGE record +0x80
    std::u16string name;
    bool terrain_loaded = false;
    u32 terrain_image = 0;         // loaded list: missing/wrong-type property 0x2BD4F5AC yields 0
};
struct SavedGameMetadata {
    StarMetadata star;
    std::u16string save_stem;      // supplied by save catalogue; no filesystem is touched here
    u32 stage = 0;                 // source +0x4C, unless flags +0x5C includes 0x80
    u32 source_flags = 0;
    // Observed probe at 0xB27580; two output header words, NOT a save identity key.
    // Successful bounded header decode supplies unsigned file major>=33; full probe I/O failure
    // behavior and missing constructor defaults are NOT inferred by a serialized-field reader.
    bool probe_result = false;
    SaveHeaderWords header;
};

enum class CatalogAvailability { Unknown, Available };
enum class CatalogProducerStatus { Unknown, Available, Failed };
struct CatalogProducerObservations {
    CatalogProducerStatus manager=CatalogProducerStatus::Unknown;
    CatalogProducerStatus starsdb=CatalogProducerStatus::Unknown;
    CatalogProducerStatus planet_records=CatalogProducerStatus::Unknown;
    CatalogProducerStatus candidate_refill=CatalogProducerStatus::Unknown;
};
struct CatalogInput {
    CatalogAvailability saved_status = CatalogAvailability::Unknown;
    CatalogAvailability candidate_status = CatalogAvailability::Unknown;
    std::string source_label; // caller-supplied provenance label; empty means producer not identified
    CatalogProducerObservations producer;
    std::vector<SavedGameMetadata> saved; // manager +0xF0/+0xF4
    std::vector<StarMetadata> candidates; // manager +0xDC/+0xE0, already selected by StarManager
};

enum class SaveStatus : u32 { HeaderMatches = 0, HeaderDiffers = 1,
                            ProbeFalseNonzeroHeader = 2, ProbeFalseZeroHeader = 3, NotSaved = 4 };
struct GameEntry {
    bool saved = false, visible = true; // GGE +0xAC/+0xAD; no UI objects/effects allocated
    u32 source_index = 0;               // reset for each of the two source lists, GGE +0xB0
    u32 star_id = 0;
    u32 stage = 5;                      // GGE +0xA8, new entries use 5
    StarPosition position;
    std::u16string name, save_stem;
    ImageKey image;                     // GGE +0x30 triple (not the separate save thumbnail)
    SaveStatus save_status = SaveStatus::NotSaved;
};

// Field projection/order subset of GA 0xDF95A0, 0xDF7CB0, 0xDF7F30. Transactional publication:
// unknown inputs, invalid coordinates or host limits return false with output unchanged.
// No discovery/generation, save I/O, scene readiness or original ABI compatibility is implied.
bool build_catalog(const CatalogInput& input, std::vector<GameEntry>& out, std::string& error);
}  // namespace os::gge
