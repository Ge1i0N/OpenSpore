#include "engine/pkg.h"

#include <cstring>
#include <filesystem>

#include "engine/log.h"

namespace os::pkg {
namespace {

constexpr u32 kMagic = 0x46504244u;  // "DBPF", from the validator's immediate at 0x8d90d6
constexpr u32 kHdrSize = 0x60;
constexpr u32 kPrefix = 12;   // confirmed: 31/57 files have index_size == 12 + 24*count
constexpr u32 kRecord = 24;

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

    u8 hdr[kHdrSize];
    if (fsz < (long long)kHdrSize || fread(hdr, 1, kHdrSize, f) != kHdrSize) {
        m_why = "file smaller than the 0x60-byte header";
        LOG_ERROR("pkg", "open(%s): %s", path.c_str(), m_why.c_str());
        fclose(f);
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
        LOG_ERROR("pkg", "REJECT %s: %s (ver=%u count=%u idx_size=%u idx_off=%u size=%lld)", path.c_str(),
                  m_why.c_str(), m_hdr.version, m_hdr.count, m_hdr.index_size, m_hdr.index_offset, fsz);
        fclose(f);
        return false;
    }

    m_file = f;
    m_ok = true;
    m_fixed = (m_hdr.index_size == kPrefix + kRecord * m_hdr.count) && m_hdr.count != 0;
    if (!m_fixed) {
        LOG_ONCE_WARN("pkg", "%s: index_size %u != 12 + 24*count (%u) - variable-width records, "
                            "parser not disassembled yet; enumerating 0 records",
                      path.c_str(), m_hdr.index_size, kPrefix + kRecord * m_hdr.count);
        return true;
    }

    std::vector<u8> idx(m_hdr.index_size);
    if (fseek(f, (long)m_hdr.index_offset, SEEK_SET) != 0 ||
        fread(idx.data(), 1, m_hdr.index_size, f) != m_hdr.index_size) {
        m_why = "cannot read index";
        m_ok = false;
        LOG_ERROR("pkg", "%s: %s at 0x%x", path.c_str(), m_why.c_str(), m_hdr.index_offset);
        return false;
    }
    m_records.resize(m_hdr.count);
    for (u32 i = 0; i < m_hdr.count; ++i) {
        const u8* p = idx.data() + kPrefix + i * kRecord;
        for (u32 w = 0; w < 6; ++w) m_records[i].raw[w] = le32(p + w * 4);
    }
    // Enforce the validator's invariant per record, so a corrupt index cannot make a later
    // read go out of bounds (this is the clamp the field guide §"defensive clamp" asks for).
    u32 dropped = 0;
    for (auto& r : m_records) {
        if ((u64)r.offset() >= m_hdr.index_offset || (u64)r.offset() + r.size() > m_hdr.index_offset) {
            ++dropped;
            r.raw[3] = 0;  // mark unreadable rather than trust it
        }
    }
    if (dropped) LOG_WARN("pkg", "%s: %u/%u records violate offset+size<=index_offset", path.c_str(), dropped,
                          m_hdr.count);
    // LOG_ONCE: one line per package, all from this call site. The per-site ledger would
    // consolidate everything past the 8th package ("x9, further repeats suppressed"), and
    // a package listing is exactly the one-per-run line LOG_ONCE exists for.
    LOG_ONCE_INFO("pkg", "opened %s: ver=%u count=%u idx_off=0x%x idx_size=%u fixed_stride=yes records=%zu",
                  path.c_str(), m_hdr.version, m_hdr.count, m_hdr.index_offset, m_hdr.index_size, m_records.size());
    return true;
}

void Package::close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_records.clear();
    m_ok = false;
    m_fixed = false;
    m_hdr = Header{};
}

bool Package::read_resource(u32 record_index, std::vector<u8>& out_bytes) {
    if (!m_ok || !m_file || record_index >= m_records.size()) return false;
    const Record& r = m_records[record_index];
    if (r.size() == 0) return false;
    if ((u64)r.offset() + r.size() > m_hdr.file_size) return false;
    out_bytes.resize(r.size());
    if (fseek(m_file, (long)r.offset(), SEEK_SET) != 0) return false;
    size_t got = fread(out_bytes.data(), 1, r.size(), m_file);
    if (got != r.size()) {
        LOG_WARN("pkg", "%s: record %u short read %zu/%u", m_path.c_str(), record_index, got, r.size());
        return false;
    }
    return true;
}

std::string Package::describe() const {
    char b[320];
    _snprintf_s(b, sizeof(b), _TRUNCATE, "%s: ver=%u count=%u idx=0x%x+%u %s records=%zu",
                m_path.c_str(), m_hdr.version, m_hdr.count, m_hdr.index_offset, m_hdr.index_size,
                m_ok ? (m_fixed ? "fixed" : "VAR-WIDTH") : "INVALID", m_records.size());
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
