#pragma once
#include "engine/pkg.h"
#include "engine/starsdb_context.h"

namespace os::starsdb {
enum class ResourceReadStatus { Found, NotFound, Ambiguous, PackageRejected, ReadFailure };
struct ResourceReadResult {
    ResourceReadStatus status=ResourceReadStatus::NotFound;
    pkg::ResourceKey key;
    std::vector<u8> bytes;
    std::vector<u32> matching_record_indices;
    std::string error;
};
class ExplicitDbpfReader {
public:
    // Explicit path uses UTF-8; Windows consumers needing non-ASCII paths must supply Unicode argv.
    bool open(const std::string& explicit_path, std::string& error);
    bool open_from_memory(const std::vector<u8>& bytes, std::string& error);
    ResourceReadResult read_unique(const pkg::ResourceKey& key);
    const pkg::Package& package() const { return package_; }
private:
    pkg::Package package_;
    bool opened_=false;
};
struct PhysicalContextResult {
    ResourceReadResult root, index;
    std::vector<ResourceReadResult> classes;
    std::optional<Prefix> root_prefix;
    std::optional<ManagerLinks> manager_links;
    std::optional<ContextIndex> context_index;
    std::vector<ClassObjects> decoded_classes;
    std::string error;
};
// Read only the exact GA key tuple from one caller-selected DBPF. No package directory traversal,
// priority merge or writes. Root and context resources use ResourceKey (instance,type,group).
bool inspect_physical_context(const std::string& explicit_path, u32 root_instance, u32 root_type,
                             u32 root_group, PhysicalContextResult& out, std::string& error);
// Same structural path for already supplied package bytes, including constructed test fixtures.
bool inspect_dbpf_context(ExplicitDbpfReader& reader, const pkg::ResourceKey& root,
                          PhysicalContextResult& out, std::string& error);
}  // namespace os::starsdb
