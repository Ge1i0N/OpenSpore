#include "engine/gge_catalog.h"
#include <cmath>
#include <utility>

namespace os::gge {
namespace {
constexpr size_t kMaxEntries = 4096; // host bound, NOT recovered StarManager quota
constexpr size_t kMaxText = 4096;
constexpr u32 kPng = 0x2f7d0004u, kTerrainImages = 0x02e9a2cau;

bool valid(const StarMetadata& star) {
    return std::isfinite(star.position.x) && std::isfinite(star.position.y) &&
           std::isfinite(star.position.z) && star.name.size() <= kMaxText;
}

GameEntry common(const StarMetadata& star, size_t index) {
    GameEntry result;
    result.source_index = (u32)index;
    result.star_id = star.star_id;
    result.position = star.position;
    result.name = star.name;
    return result;
}
}  // namespace

bool build_catalog(const CatalogInput& input, std::vector<GameEntry>& out, std::string& error) {
    if (input.saved_status != CatalogAvailability::Available ||
        input.candidate_status != CatalogAvailability::Available) {
        error = "catalog source unavailable: both saved and candidate lists need explicit observations";
        return false;
    }
    if (input.saved.size() > kMaxEntries || input.candidates.size() > kMaxEntries - input.saved.size()) {
        error = "catalog exceeds host limit (4096 entries)";
        return false;
    }
    // Validate everything before allocating/copying the projected entries.
    for (const auto& source : input.saved) {
        if (!valid(source.star) || source.save_stem.size() > kMaxText) {
            error = "invalid saved metadata: non-finite position or oversized text";
            return false;
        }
    }
    for (const auto& source : input.candidates) {
        if (!valid(source)) {
            error = "invalid candidate metadata: non-finite position or oversized text";
            return false;
        }
    }
    std::vector<GameEntry> result;
    result.reserve(input.saved.size() + input.candidates.size());
    // 0xDF95A0 populates candidates first, but into the tail AFTER the saved-game count.
    // Published order is saved then candidates; never sort/dedupe by star id or by source index.
    for (size_t i = 0; i < input.saved.size(); ++i) {
        const auto& source = input.saved[i];
        auto entry = common(source.star, i);
        entry.saved = true;
        entry.save_stem = source.save_stem;
        entry.stage = (source.source_flags & 0x80) ? 0 : source.stage;
        entry.image = source.star.terrain_loaded
            ? ImageKey{source.star.terrain_image, kPng, kTerrainImages}
            : ImageKey{0x06837044u, kPng, 0x02605ba3u}; // GA saved placeholder before property load
        // 0xDF8180..0xDF81D6: compare both output words with .rdata 0x145F844/48 = 0x25,3.
        // This is a header predicate, NOT equality with a currently selected game's ID.
        entry.save_status = source.probe_result
            ? (source.header.first == 0x25 && source.header.second == 3
                ? SaveStatus::HeaderMatches : SaveStatus::HeaderDiffers)
            : ((source.header.first || source.header.second)
                ? SaveStatus::ProbeFalseNonzeroHeader : SaveStatus::ProbeFalseZeroHeader);
        result.push_back(std::move(entry));
    }
    for (size_t i = 0; i < input.candidates.size(); ++i) {
        const auto& source = input.candidates[i];
        auto entry = common(source, i);
        if (source.terrain_loaded) entry.image = {source.terrain_image, kPng, kTerrainImages};
        result.push_back(std::move(entry));
    }
    out = std::move(result);
    error.clear();
    return true;
}
}  // namespace os::gge
