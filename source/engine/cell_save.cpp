#include "engine/cell_save.h"
#include <cstdio>
#include <cstring>
#include "engine/pkg.h"

namespace os::cell {
namespace {

constexpr u32 kObjectTag = 0x01A80D26u;
constexpr u32 kStreamA = 0x018EB4B7u;  // 1503 declared objects in the real save
constexpr u32 kStreamB = 0x018EB45Eu;  // 1648 declared objects in the real save

u32 be32(const u8* p) {
    return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | u32(p[3]);
}

float be_float(const u8* p) {
    u32 bits = be32(p);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

void apply_field(CreatureStats& stats, u32 fid, const std::vector<u8>& val) {
    if (val.empty()) return;
    switch (fid) {
    case 0x0240E421u: if (val.size() >= 4) stats.stat0 = be32(val.data()); break;
    case 0x0240E422u: stats.stat1 = val[0]; break;
    case 0x0240E423u: if (val.size() >= 4) stats.stat2 = be32(val.data()); break;
    case 0x02E9EFD3u: if (val.size() >= 4) stats.stat3 = be32(val.data()); break;
    case 0x03CDD54Bu: if (val.size() >= 4) stats.stat4 = be32(val.data()); break;
    case 0x024162BFu: if (val.size() >= 4) stats.pos_x = be_float(val.data()); break;
    case 0x024162C0u: if (val.size() >= 4) stats.pos_y = be_float(val.data()); break;
    case 0x024162C1u: if (val.size() >= 4) stats.pos_z = be_float(val.data()); break;
    case 0x024162C2u: if (val.size() >= 4) stats.pos_w = be_float(val.data()); break;
    case 0x02416C5Eu:
        if (val.size() >= 12) {
            stats.detailed_pos = std::array<float, 3>{
                be_float(val.data()), be_float(val.data() + 4), be_float(val.data() + 8)};
        }
        break;
    case 0x02416C60u:
        if (val.size() >= 16) {
            stats.stat5 = std::array<float, 4>{
                be_float(val.data()), be_float(val.data() + 4),
                be_float(val.data() + 8), be_float(val.data() + 12)};
        }
        break;
    case 0x02416C61u: stats.stat6 = val[0]; break;
    case 0x02416C62u: stats.stat7 = val[0]; break;
    case 0x02416C63u: stats.stat8 = val[0]; break;
    case 0x02416C64u: if (val.size() >= 4) stats.stat9 = be_float(val.data()); break;
    case 0x02416C65u: stats.stat10 = val[0]; break;
    case 0x02416C66u: stats.stat11 = val[0]; break;
    case 0x02416C67u: stats.stat12 = val[0]; break;
    case 0x02416C68u: stats.stat13 = val[0]; break;
    case 0x02416C69u: stats.stat14 = val[0]; break;
    case 0x02416C6Au: stats.stat15 = val[0]; break;
    case 0x02416C6Bu: if (val.size() >= 4) stats.stat16 = be32(val.data()); break;
    case 0x02416C6Cu: stats.stat17 = val[0]; break;
    case 0x02416C6Du: if (val.size() >= 4) stats.stat18 = be32(val.data()); break;
    case 0x02416C6Eu: if (val.size() >= 4) stats.stat19 = be32(val.data()); break;
    default: break;
    }
}

// Parse one decoded multi-object stream blob and append its creatures.
// Bounds-guarded; malformed tails just stop the walk (the file stays read-only).
void decode_stream(const std::vector<u8>& blob, std::vector<CreatureStats>& out,
                   StreamInfo& info) {
    if (blob.size() < 24) return;
    info.declared_count = be32(blob.data());
    info.crc = be32(blob.data() + 4);
    info.token = be32(blob.data() + 8);
    info.header_hex.clear();
    for (int i = 0; i < 12; ++i) {
        char b[3];
        snprintf(b, sizeof b, "%02x", blob[12 + i]);
        info.header_hex += b;
    }

    size_t pos = 24;
    u32 obj_idx = 0;
    while (pos + 12 <= blob.size() && obj_idx < info.declared_count) {
        u32 tag = be32(blob.data() + pos);
        if (tag != kObjectTag) { pos++; continue; }
        u32 length = be32(blob.data() + pos + 4);
        u32 cnt = be32(blob.data() + pos + 8);
        size_t frame_end = pos + 8 + length;
        if (cnt > 128 || frame_end > blob.size()) break;
        CreatureStats stats;
        size_t pca = pos + 12 + 8 * cnt;
        for (u32 i = 0; i < cnt; ++i) {
            u32 fid = be32(blob.data() + pos + 12 + 8 * i);
            u32 size = be32(blob.data() + pos + 16 + 8 * i);
            if (pca + size > frame_end) break;
            apply_field(stats, fid, std::vector<u8>(blob.begin() + pca, blob.begin() + pca + size));
            pca += size;
        }
        out.push_back(stats);
        pos = frame_end;
        while (pos + 4 <= blob.size() && be32(blob.data() + pos) != kObjectTag) pos++;
        obj_idx++;
    }
    info.objects_decoded = obj_idx;
    info.bytes_consumed = static_cast<u32>(pos);
}

bool decode_package(pkg::Package& p, std::vector<CreatureStats>& out,
                    StreamInfo& info, std::string& error) {
    const auto& recs = p.records();
    for (size_t i = 0; i < recs.size(); ++i) {
        u32 t = recs[i].type();
        if (t != kStreamA && t != kStreamB) continue;
        std::vector<u8> blob;
        if (!p.read_resource_decoded(static_cast<u32>(i), blob)) {
            error = "stream resource read failed";
            return false;
        }
        decode_stream(blob, out, info);
    }
    error.clear();
    return true;
}

}  // namespace

bool decode_save_from_memory(const std::vector<u8>& data, std::vector<CreatureStats>& out,
                             StreamInfo& info, std::string& error) {
    pkg::Package p;
    if (!p.open_from_memory(data.data(), data.size(), "cell-save")) {
        error = p.why();
        return false;
    }
    return decode_package(p, out, info, error);
}

bool decode_save(const std::string& path, std::vector<CreatureStats>& out,
                 StreamInfo& info, std::string& error) {
    pkg::Package p;
    if (!p.open(path)) {
        error = p.why();
        return false;
    }
    return decode_package(p, out, info, error);
}

}  // namespace os::cell
