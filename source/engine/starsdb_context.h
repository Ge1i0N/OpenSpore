#pragma once
#include "engine/starsdb_links.h"

namespace os::starsdb {
// ResourceKey memory order is (instance, type, group), as passed to the GA manager.
constexpr u32 kContextInstance=0x0179d304u, kContextIndexType=0x0179d310u, kContextGroup=0u;
struct ClassCount { u32 class_id=0, count=0; };
struct ContextIndex {
    u32 version=0, declared_objects=0;
    std::vector<ClassCount> classes;
};
// GA 0x69F1D0 / 0x69FB90: three BE words, then class/count pairs.
// Exact payload, unique classes, count-sum equality and allocation caps are host validation policies.
bool read_context_index(const std::vector<u8>& bytes, ContextIndex& out, std::string& error);
struct ObjectBody {
    u32 token=0, crc=0;
    size_t offset=0, end=0, body_offset=0;
    std::optional<StarRecord> star; // only class 0x03E4353C; no resource/name post-load fixups
};
struct ClassObjects {
    u32 class_id=0;
    std::vector<ObjectBody> objects; // original stream order, identity is (class_id, token)
};
// Explicit decoded class resource, not an arbitrary file search. Header per object:
// BE total_bytes INCLUDING 12-byte header, BE CRC, BE token, serialized object body.
// CRC uses min(total_bytes-8,1,000,000) bytes beginning at token, NOT the common reflected CRC32.
bool read_class_objects(const std::vector<u8>& bytes, const ClassCount& expected,
                        ClassObjects& out, std::string& error);
// Caller supplies the exact already-bounded CRC span. Exposed for independent known-vector tests.
u32 object_crc(const u8* bytes, size_t size);
enum class ReferenceResolution { Null, MissingClassStream, MissingObject, UnsupportedClass, StarFields };
struct ResolvedReference {
    ReferenceResolution status=ReferenceResolution::MissingClassStream;
    size_t object_index=0; // valid only for StarFields/UnsupportedClass; never an address
};
// Structural lookup only. Does not declare a catalogue Available or run GA factories/fixups.
ResolvedReference resolve_star_reference(const ObjectReference& ref, const ClassObjects& objects);
}  // namespace os::starsdb
