#pragma once
// Resource name resolution: 32-bit ids -> human names.
//
// Group and instance ids in a DBPF index are FNV-1 (multiply-then-xor) hashes of the names,
// with tolower folding - a fact verified twice now:
//   * disassembly of the hash at 0x00932e50 (`imul` before `xor`, mode arg 1 = tolower);
//   * 14/14 of the cs-cz NameGeneration instance ids resolve to LexiconPeople_CS,
//     LexiconCities_CS, ... using this exact function against the registry.
// Type ids are NOT hashed (0/6 tested - IDEAS.md 5.20); they are looked up in a table, which
// is why the generated `typeids.inc` exists.
//
// The registries themselves are data, loaded at runtime so the executable stays small and the
// tables stay traceable to their source. If they are absent, everything still works and names
// simply fall back to hex - never to a guessed name.
#include <string>

#include "common.h"

namespace os::naming {

// Loads reg_file.txt (group/instance names) and reg_type.txt (type names) from the first
// location that exists. Safe to call once; returns the number of names loaded.
u32 load();

bool loaded();

// FNV-1 with tolower, as the original computes it (kept here so naming does not depend on the
// state machine's header).
u32 hash_lower(const char* s);

// Returns the name for an id, or nullptr when unknown. Never guesses.
const char* instance_name(u32 id);
const char* type_name(u32 id);

// Returns the tuning-property name for a property id (reg_property.txt), or nullptr. The cell
// stage's `prop` resource keys its curves by these hashed property ids.
const char* property_name(u32 id);

// `group/instance.type` style path, or a hex form when the id is unknown.
std::string resource_path(u32 type, u32 group, u32 instance);

std::string stats();

}  // namespace os::naming
