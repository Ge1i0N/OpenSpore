#pragma once
#include "engine/starsdb.h"

namespace os::starsdb {
// Wire references, NOT addresses in this process and NOT star IDs. The writer emits its original
// object-pointer word as token (0x6A05F0); the reader treats it as a class-scoped map key.
// Identity resolution is missing.
// GA context maps by class + token; null is a single BE zero word, nonnull is two BE words.
struct ObjectReference {
    u32 token=0, class_id=0;
    bool is_null() const { return token==0; }
};
using ReferenceList=std::vector<ObjectReference>;
using ReferenceGrid=std::vector<ReferenceList>; // cells in serialized order, no invented grid dimensions
// Explicit host allocation limits, not reconstructed limits of a real galaxy.
constexpr size_t kMaxReferences=1048576, kMaxGridCells=65536;
bool read_reference_list(const std::vector<u8>& bytes, size_t offset, size_t size,
                         ReferenceList& out, std::string& error);
bool read_reference_grid(const std::vector<u8>& bytes, size_t offset, size_t size,
                         ReferenceGrid& out, std::string& error);
struct ManagerLinks {
    // Absent/zero-size fields remain unknown, distinct from a count=0 encoded list/grid.
    std::optional<ReferenceList> starters, saved, black_holes, possible_starts;
    std::optional<ReferenceGrid> grid;
};
// Revalidates the manager frame at an explicit offset. Unknown fields stay opaque; does not
// allocate original objects, check interface compatibility or resolve any reference.
bool inspect_manager_links(const std::vector<u8>& bytes, size_t offset, ManagerLinks& out, std::string& error);
}  // namespace os::starsdb
