#include "engine/pkg.h"

#include <cstring>
#include <filesystem>

#include "engine/log.h"
#include "engine/refpack.h"

namespace os::pkg {
namespace {

constexpr u32 kMagic = 0x46504244u;  // "DBPF", from the validator's immediate at 0x8d90d6
constexpr u32 kHdrSize = 0x60;

struct KindInfo {
    u32 prefix, stride, nwords;
    u32 i_type, i_group, i_unknown, i_instance, i_offset, i_size, i_usize, i_flags;
    bool has_type, has_group, has_unknown, has_instance;
};

// K is a bit mask of which fields are SHARED (hoisted into the prefix):
//   bit 0 -> type, bit 1 -> group, bit 2 -> unknown.
// A full record is type,group,unknown,instance,offset,packedSize,memSize,compression:u16,
// flags:u16 = 32 bytes, so prefix = 4 + 4*popcount(K) and stride = 32 - 4*popcount(K).
// Observed on all 57 shipped packages: K=4 -> (8,28), K=6 -> (12,24), K=7 -> (16,20).
bool kind_info(u32 k, KindInfo& ki) {
    auto popcount = [](u32 v) {
        u32 n = 0;
        for (; v; v >>= 1) n += v & 1;
        return n;
    };
    const u32 shared = popcount(k & 7);
    const u32 prefix = 4 + 4 * shared;
    const u32 stride = 32 - 4 * shared;
    if (stride < 20 || stride > 32 || (stride % 4) != 0) return false;
    ki.prefix = prefix;
    ki.stride = stride;
    ki.nwords = stride / 4;
    // Fields appear in the fixed order type,group,unknown,instance,offset,packedSize,memSize,flags;
    // the shared ones are skipped in the record and read from the prefix instead.
    u32 w = 0;
    ki.i_type = w;
    ki.has_type = !(k & 1);
    if (ki.has_type) ++w;
    ki.i_group = w;
    ki.has_group = !(k & 2);
    if (ki.has_group) ++w;
    ki.i_unknown = w;
    ki.has_unknown = !(k & 4);
    if (ki.has_unknown) ++w;
    ki.i_instance = w;
    ki.has_instance = true;  // instance is never masked in the observed kinds
    ++w;
    ki.i_offset = w++;
    ki.i_size = w++;
    ki.i_usize = w++;
    ki.i_flags = w++;
    return w == ki.nwords;  // the layout must account for every word, or the kind is unknown
}

u32 le32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }

}  // namespace

bool Package::open(const std::string& path) {
    close();
    m_path = path;
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
        m_why = "cannot open file";
        LOG_ERROR("pkg", "open(%s): %s", path.c_str(), m_why.c_str());
        return false;
    }
    fseek(f, 0, SEEK_END);
    long long fsz = ftell(f);
    fseek(f, 0, SEEK_SET);
    m_hdr.file_size = (u64)fsz;
    m_file = f;
    return parse((u64)fsz);
}

bool Package::open_from_memory(const u8* data, size_t size, const std::string& label) {
    close();
    m_path = label;
    if (!data || size == 0) {
        m_why = "empty nested package";
        return false;
    }
    m_mem.assign(data, data + size);
    m_hdr.file_size = (u64)size;
    return parse((u64)size);
}

bool Package::read_at(u64 off, void* dst, size_t n) {
    if (!m_mem.empty()) {
        if (off + n > m_mem.size()) return false;
        std::memcpy(dst, m_mem.data() + off, n);
        return true;
    }
    if (!m_file) return false;
    if (fseek(m_file, (long)off, SEEK_SET) != 0) return false;
    return fread(dst, 1, n, m_file) == n;
}

bool Package::parse(u64 fsz) {
    u8 hdr[kHdrSize];
    if (fsz < (u64)kHdrSize || !read_at(0, hdr, kHdrSize)) {
        m_why = "source smaller than the 0x60-byte header";
        LOG_ERROR("pkg", "parse(%s): %s", m_path.c_str(), m_why.c_str());
        return false;
    }
    m_hdr.magic = le32(hdr + 0x00);
    m_hdr.version = le32(hdr + 0x04);
    m_hdr.count = le32(hdr + 0x24);
    m_hdr.index_size = le32(hdr + 0x2c);
    m_hdr.field_3c = le32(hdr + 0x3c);
    u32 ioff = le32(hdr + 0x40);
    u32 alt = le32(hdr + 0x28);
    m_hdr.index_offset = ioff ? ioff : alt;  // the loader's own fallback at 0x8d901b..0x8d9029

    // Replay the validator's clauses in the same order, with the same reasons.
    if (m_hdr.magic != kMagic) {
        m_why = "magic != DBPF";
    } else if (m_hdr.version > 3) {
        m_why = "version > 3";
    } else if (m_hdr.count >= 0x07FFFFFFu) {
        m_why = "count >= 0x07FFFFFF";
    } else if ((u64)m_hdr.index_offset >= (u64)fsz) {
        m_why = "index_offset >= filesize";
    } else if ((u64)m_hdr.index_offset + (u64)m_hdr.index_size > (u64)fsz) {
        m_why = "index_offset + index_size > filesize";
    } else if (m_hdr.field_3c == 0) {
        m_why = "header +0x3c is zero";
    } else {
        m_why.clear();
    }
    if (!m_why.empty()) {
        m_ok = false;
        LOG_ERROR("pkg", "REJECT %s: %s (ver=%u count=%u idx_size=%u idx_off=%u size=%lld)", m_path.c_str(),
                  m_why.c_str(), m_hdr.version, m_hdr.count, m_hdr.index_size, m_hdr.index_offset, fsz);
        return false;
    }

    m_ok = true;

    // Read the kind dword (index dword[0]) and check the measured formula.
    u8 kb[4] = {};
    if (!read_at(m_hdr.index_offset, kb, 4)) {
        m_why = "cannot read index kind";
        m_ok = false;
        LOG_ONCE_ERROR("pkg", "%s: %s at 0x%x", m_path.c_str(), m_why.c_str(), m_hdr.index_offset);
        return false;
    }
    m_hdr.kind = le32(kb);
    KindInfo ki{};
    m_kind_known = kind_info(m_hdr.kind, ki);
    if (!m_kind_known || m_hdr.index_size != ki.prefix + ki.stride * m_hdr.count) {
        LOG_ONCE_WARN("pkg", "%s: kind=%u count=%u idx_size=%u does not fit prefix+stride*count "
                            "(predicted %u) - index NOT parsed",
                      m_path.c_str(), m_hdr.kind, m_hdr.count, m_hdr.index_size,
                      m_kind_known ? ki.prefix + ki.stride * m_hdr.count : 0u);
        return true;  // header valid, index unknown
    }

    std::vector<u8> idx(m_hdr.index_size);
    if (!read_at(m_hdr.index_offset, idx.data(), m_hdr.index_size)) {
        m_why = "cannot read index";
        m_ok = false;
        LOG_ONCE_ERROR("pkg", "%s: %s at 0x%x", m_path.c_str(), m_why.c_str(), m_hdr.index_offset);
        return false;
    }
    // The prefix is the kind dword followed by one u32 per SHARED field, in field order.
    // A field is shared when its bit is SET in the kind mask (i.e. when has_* is false).
    {
        u32 w = 1;
        auto next = [&](u32& dst, bool shared) {
            if (shared && w * 4 < ki.prefix) dst = le32(idx.data() + 4 * w++);
        };
        next(m_shared.type, !ki.has_type);
        next(m_shared.group, !ki.has_group);
        next(m_shared.unknown, !ki.has_unknown);
    }
    m_records.resize(m_hdr.count);
    for (u32 i = 0; i < m_hdr.count; ++i) {
        const u8* p = idx.data() + ki.prefix + i * ki.stride;
        Record& r = m_records[i];
        r.nwords = ki.nwords;
        r.i_type = ki.i_type;
        r.i_group = ki.i_group;
        r.i_unknown = ki.i_unknown;
        r.i_instance = ki.i_instance;
        r.i_offset = ki.i_offset;
        r.i_size = ki.i_size;
        r.i_usize = ki.i_usize;
        r.i_flags = ki.i_flags;
        r.has_group = ki.has_group;
        r.has_unknown = ki.has_unknown;
        r.has_instance = ki.has_instance;
        r.type_shared = !ki.has_type;
        r.shared_type = m_shared.type;
        r.group_shared = !ki.has_group;
        r.shared_group = m_shared.group;
        r.unknown_shared = !ki.has_unknown;
        r.shared_unknown = m_shared.unknown;
        for (u32 x = 0; x < ki.nwords; ++x) r.raw[x] = le32(p + x * 4);
    }
    // Enforce the validator's invariant per record, so a corrupt index cannot make a later
    // read go out of bounds (this is the clamp the field guide §"defensive clamp" asks for).
    u32 dropped = 0;
    for (auto& r : m_records) {
        if ((u64)r.offset() >= m_hdr.index_offset || (u64)r.offset() + r.packed_size() > m_hdr.index_offset) {
            ++dropped;
            r.raw[r.i_size] = 0;  // mark unreadable rather than trust it
        }
    }
    if (dropped) LOG_WARN("pkg", "%s: %u/%u records violate offset+size<=index_offset", m_path.c_str(), dropped,
                          m_hdr.count);
    LOG_ONCE_INFO("pkg", "opened %s: ver=%u kind=%u count=%u idx_off=0x%x idx_size=%u records=%zu", m_path.c_str(),
                  m_hdr.version, m_hdr.kind, m_hdr.count, m_hdr.index_offset, m_hdr.index_size, m_records.size());
    return true;
}
void Package::close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_mem.clear();
    m_mem.shrink_to_fit();
    m_records.clear();
    m_ok = false;
    m_kind_known = false;
    m_hdr = Header{};
}

bool Package::read_resource(u32 record_index, std::vector<u8>& out_bytes) {
    if (!m_ok || (!m_file && m_mem.empty()) || record_index >= m_records.size()) return false;
    const Record& r = m_records[record_index];
    if (r.packed_size() == 0) return false;
    if ((u64)r.offset() + r.packed_size() > m_hdr.file_size) return false;
    out_bytes.resize(r.packed_size());
    // Go through read_at so a NESTED package (backed by m_mem) reads the same way a file does. This
    // is the one place the refactor first missed, and it failed loudly rather than silently: a nested
    // Package has m_file == nullptr, so the UCRT asserted `stream.valid()` in fseek(). Loud is good.
    if (!read_at(r.offset(), out_bytes.data(), r.packed_size())) {
        LOG_WARN("pkg", "%s: record %u unreadable (%u bytes at 0x%x)", m_path.c_str(), record_index,
                 r.packed_size(), r.offset());
        return false;
    }
    return true;
}

std::vector<u32> Package::find_records(const ResourceKey& key) const {
    std::vector<u32> matches;
    if (!m_ok || !m_kind_known) return matches;
    for (u32 i=0; i<m_records.size(); ++i) {
        const auto& record=m_records[i];
        if (record.instance()==key.instance && record.type()==key.type && record.group()==key.group)
            matches.push_back(i);
    }
    return matches;
}

bool Package::read_resource_decoded(u32 record_index, std::vector<u8>& out_bytes) {
    if (record_index >= m_records.size()) return false;
    const Record& r = m_records[record_index];
    std::vector<u8> packed;
    if (!read_resource(record_index, packed)) return false;
    if (!r.refpacked()) {
        out_bytes = std::move(packed);  // stored raw; memSize == packedSize in this case
        return true;
    }
    std::vector<u8> decoded;
    u32 n = refpack::decode(packed.data(), (u32)packed.size(), r.mem_size(), decoded);
    if (n == (u32)-1) {
        LOG_WARN("pkg", "%s: record %u RefPack decode failed (packed=%u mem=%u)", m_path.c_str(), record_index,
                 r.packed_size(), r.mem_size());
        return false;
    }
    out_bytes = std::move(decoded);
    return true;
}

std::string Package::describe() const {
    char b[320];
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%s: ver=%u kind=%u count=%u idx=0x%x+%u %s records=%zu", m_path.c_str(),
                m_hdr.version, m_hdr.kind, m_hdr.count, m_hdr.index_offset, m_hdr.index_size,
                m_ok ? (m_kind_known ? "parsed" : "KIND-UNKNOWN") : "INVALID", m_records.size());
    return b;
}

std::vector<std::string> find_packages(const std::string& dir, int max_depth) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return out;
    for (auto it = std::filesystem::recursive_directory_iterator(
             dir, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        // depth() returns int and max_depth is int: compare them as-is. Casting max_depth to
        // unsigned here is what produced C4018 (signed/unsigned mismatch).
        if (it.depth() > max_depth) {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && it->path().extension() == ".package")
            out.push_back(it->path().string());
        if (out.size() >= 512u) break;  // bounded: the loader only shows a screenful
    }
    return out;
}

}  // namespace os::pkg
