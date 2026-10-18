#include "engine/starsdb_dbpf.h"
#include <fstream>
#include <filesystem>
#include <utility>

namespace os::starsdb {
namespace {
constexpr size_t kMaxPackageBytes=256*1024*1024, kMaxResourceBytes=64*1024*1024;
u32 le32(const std::vector<u8>& bytes,size_t at) {
    return u32(bytes[at]) | (u32(bytes[at+1])<<8) | (u32(bytes[at+2])<<16) | (u32(bytes[at+3])<<24);
}
}
bool ExplicitDbpfReader::open(const std::string& explicit_path,std::string& error) {
    opened_=false; package_.close();
    const std::u8string utf8_path(reinterpret_cast<const char8_t*>(explicit_path.data()),explicit_path.size());
    std::ifstream file(std::filesystem::path(utf8_path),std::ios::binary|std::ios::ate);
    if (!file) { error="cannot open explicitly selected DBPF"; return false; }
    const auto size=file.tellg();
    if (size<0 || size>kMaxPackageBytes) { error="DBPF exceeds 256 MiB host package limit"; return false; }
    std::vector<u8> bytes((size_t)size);
    file.seekg(0);
    if (!bytes.empty() && !file.read((char*)bytes.data(),(std::streamsize)bytes.size())) {
        error="short read of selected DBPF"; return false;
    }
    return open_from_memory(bytes,error);
}
bool ExplicitDbpfReader::open_from_memory(const std::vector<u8>& bytes,std::string& error) {
    opened_=false; package_.close();
    const auto fail=[&](const char* why) { error=why; return false; };
    if (bytes.size()<0x60 || bytes.size()>kMaxPackageBytes) return fail("DBPF header/host package size limit");
    // Narrow versioned subset: do not let Package's broad header acceptance imply every old
    // save index layout is understood. Validate directory sizing BEFORE generic allocations.
    if (le32(bytes,0)!=0x46504244) return fail("selected file is not DBPF");
    if (le32(bytes,4)!=3 || le32(bytes,8)!=0 || le32(bytes,0x3c)!=3)
        return fail("unsupported DBPF version/index version (requires 3.0 / index 3)");
    const size_t count=le32(bytes,0x24), size=le32(bytes,0x2c);
    const size_t offset=le32(bytes,0x40)?le32(bytes,0x40):le32(bytes,0x28);
    if (count>65536) return fail("DBPF record count exceeds host limit 65536");
    if (offset<0x60 || offset>bytes.size() || size<4 || size>bytes.size()-offset)
        return fail("DBPF directory outside input");
    const u32 kind=le32(bytes,offset);
    if (kind>7) return fail("unsupported DBPF shared-field mask");
    size_t shared=0;
    for (u32 bits=kind; bits; bits>>=1) shared+=bits&1u;
    if (size!=4+4*shared+(32-4*shared)*count) return fail("DBPF directory count/stride mismatch");
    opened_=package_.open_from_memory(bytes.data(),bytes.size(),"explicit-starsdb-dbpf");
    if (!opened_) { error=package_.why(); return false; }
    if (!package_.index_parsed()) { error="DBPF index kind is unknown/unparsed"; opened_=false; return false; }
    error.clear();
    return true;
}
ResourceReadResult ExplicitDbpfReader::read_unique(const pkg::ResourceKey& key) {
    ResourceReadResult result; result.key=key;
    if (!opened_) { result.status=ResourceReadStatus::PackageRejected; result.error="reader is not open"; return result; }
    result.matching_record_indices=package_.find_records(key);
    if (result.matching_record_indices.empty()) { result.status=ResourceReadStatus::NotFound; result.error="exact ResourceKey not found"; return result; }
    if (result.matching_record_indices.size()!=1) { result.status=ResourceReadStatus::Ambiguous; result.error="exact ResourceKey has duplicate DBPF records"; return result; }
    const auto& record=package_.records()[result.matching_record_indices[0]];
    if (record.unknown()!=0 || record.flags()!=1 || (record.compression()!=0 && !record.refpacked()) ||
        !record.packed_size() || !record.mem_size() || record.packed_size()>kMaxResourceBytes || record.mem_size()>kMaxResourceBytes ||
        (record.compression()==0 && record.packed_size()!=record.mem_size()) || record.offset()<0x60) {
        result.status=ResourceReadStatus::ReadFailure;
        result.error="unsupported resource key-high-word/flags/compression/size or host resource limit"; return result;
    }
    if (!package_.read_resource_decoded(result.matching_record_indices[0],result.bytes)) {
        result.status=ResourceReadStatus::ReadFailure; result.error="matching DBPF resource could not be decoded"; return result;
    }
    if (result.bytes.size()!=record.mem_size()) {
        result.status=ResourceReadStatus::ReadFailure; result.bytes.clear(); result.error="decoded resource size mismatch"; return result;
    }
    result.status=ResourceReadStatus::Found;
    return result;
}

bool inspect_physical_context(const std::string& explicit_path,u32 root_instance,u32 root_type,
                              u32 root_group,PhysicalContextResult& out,std::string& error) {
    ExplicitDbpfReader reader;
    if (!reader.open(explicit_path,error)) return false;
    const pkg::ResourceKey root{root_instance,root_type,root_group};
    return inspect_dbpf_context(reader,root,out,error);
}
bool inspect_dbpf_context(ExplicitDbpfReader& reader,const pkg::ResourceKey& root,
                          PhysicalContextResult& out,std::string& error) {
    PhysicalContextResult result;
    result.root=reader.read_unique(root);
    if (result.root.status!=ResourceReadStatus::Found) { error=result.root.error; return false; }
    Prefix prefix;
    if (!inspect_prefix(result.root.bytes,prefix,error)) return false;
    if (prefix.remaining) { error="root stream has unsupported trailing bytes"; return false; }
    ManagerLinks links;
    if (!inspect_manager_links(result.root.bytes,prefix.manager.offset,links,error)) return false;
    result.manager_links=std::move(links);
    result.root_prefix=std::move(prefix);
    const pkg::ResourceKey index{kContextInstance,kContextIndexType,kContextGroup};
    result.index=reader.read_unique(index);
    if (result.index.status!=ResourceReadStatus::Found) { error=result.index.error; return false; }
    ContextIndex context;
    if (!read_context_index(result.index.bytes,context,error)) return false;
    result.context_index=context;
    size_t aggregate_bytes=result.root.bytes.size()+result.index.bytes.size();
    for (const auto& entry : context.classes) {
        const pkg::ResourceKey key{kContextInstance,entry.class_id,kContextGroup};
        auto resource=reader.read_unique(key);
        if (resource.status!=ResourceReadStatus::Found) { error=resource.error; return false; }
        if (resource.bytes.size()>kMaxPackageBytes-aggregate_bytes) { error="decoded context aggregate exceeds host limit"; return false; }
        aggregate_bytes+=resource.bytes.size();
        ClassObjects objects;
        if (!read_class_objects(resource.bytes,entry,objects,error)) return false;
        result.classes.push_back(std::move(resource));
        result.decoded_classes.push_back(std::move(objects));
    }
    out=std::move(result);
    error.clear();
    return true;
}
} // namespace os::starsdb
