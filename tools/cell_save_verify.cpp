// Constructed-input tests for cell_save.cpp — no game data required.
#include "engine/cell_save.h"
#include <cstring>
#include <iostream>
#include <vector>

using namespace os;

static int failures = 0;
static void check(bool ok, const char* what) {
    if (!ok) { ++failures; std::cerr << "FAIL: " << what << "\n"; }
}

static std::vector<u8> be_bytes(u32 v) {
    return {static_cast<u8>(v >> 24), static_cast<u8>(v >> 16),
            static_cast<u8>(v >> 8), static_cast<u8>(v)};
}

static std::vector<u8> make_dbpf(const std::vector<u8>& payload) {
    // Layout mirrors the shipped files: header(0x60) + payload + index, all
    // little-endian. Index kind 7 = shared type/group/unknown, stride 20.
    u32 payload_off = 0x60;
    u32 index_offset = payload_off + static_cast<u32>(payload.size());
    u32 index_size = 16 + 20;  // kind 7: prefix 16 + stride 20 * 1 record
    std::vector<u8> out(index_offset + index_size, 0);
    memcpy(out.data(), "DBPF", 4);
    auto put32 = [&](size_t at, u32 v) { memcpy(out.data() + at, &v, 4); };
    put32(4, 3); put32(8, 0); put32(0x24, 1); put32(0x2C, index_size);
    put32(0x3C, 3); put32(0x40, index_offset);
    memcpy(out.data() + payload_off, payload.data(), payload.size());
    size_t ix = index_offset;
    put32(ix, 7);                 // kind: type+group+unknown shared
    put32(ix + 4, 0x018EB4B7u);   // shared type
    put32(ix + 8, 0);             // shared group
    put32(ix + 12, 0);            // shared unknown
    put32(ix + 16, 0);            // instance
    put32(ix + 20, payload_off);  // offset
    put32(ix + 24, 0x80000000u | static_cast<u32>(payload.size()));  // size|used-bit
    put32(ix + 28, static_cast<u32>(payload.size()));                // mem size
    put32(ix + 32, 0x00010000u);  // sentinel: raw + flags 1
    return out;
}

static std::vector<u8> make_stream(u32 count,
    const std::vector<std::vector<std::pair<u32, std::vector<u8>>>>& objs) {
    std::vector<u8> stream;
    auto put32 = [&](u32 v) {
        stream.push_back(static_cast<u8>(v >> 24));
        stream.push_back(static_cast<u8>(v >> 16));
        stream.push_back(static_cast<u8>(v >> 8));
        stream.push_back(static_cast<u8>(v));
    };
    put32(count); put32(0xDEADBEEF); put32(0x12345678);
    for (int i = 0; i < 12; ++i) stream.push_back(0);
    for (auto& fields : objs) {
        u32 payload_bytes = 0;
        for (auto& [fid, val] : fields) payload_bytes += static_cast<u32>(val.size());
        u32 length = 4 + 8 * static_cast<u32>(fields.size()) + payload_bytes;
        put32(0x01A80D26u); put32(length); put32(static_cast<u32>(fields.size()));
        for (auto& [fid, val] : fields) { put32(fid); put32(static_cast<u32>(val.size())); }
        for (auto& [fid, val] : fields) stream.insert(stream.end(), val.begin(), val.end());
        // 24-byte object trailer closes the group (mirrors the real save)
        for (int i = 0; i < 6; ++i) put32(0);
    }
    return stream;
}

int main() {
    // Test 1: single object with stat fields (big-endian payloads)
    {
        auto sval = be_bytes(42);
        std::vector<u8> fval = {7};
        auto ival = be_bytes(0xFFFFFFFF);
        auto payload = make_stream(1, {{{0x0240E421u, sval}, {0x0240E422u, fval}, {0x0240E423u, ival}}});
        auto dbpf = make_dbpf(payload);
        std::vector<cell::CreatureStats> out;
        cell::StreamInfo info;
        std::string err;
        bool ok = cell::decode_save_from_memory(dbpf, out, info, err);
        check(ok, "decode single object");
        check(out.size() == 1, "one creature");
        check(out[0].stat0 == 42, "stat0 == 42");
        check(out[0].stat1 == 7, "stat1 == 7");
        check(out[0].stat2 == 0xFFFFFFFF, "stat2 == 0xFFFFFFFF");
        check(info.objects_decoded == 1, "info count");
    }

    // Test 2: packed int coordinates (u32 wire values, not floats)
    {
        auto xval = be_bytes(0x04F30503);
        auto yval = be_bytes(0xEE0F01F4);
        auto payload = make_stream(1, {{{0x024162BFu, xval}, {0x024162C0u, yval}}});
        auto dbpf = make_dbpf(payload);
        std::vector<cell::CreatureStats> out;
        cell::StreamInfo info;
        std::string err;
        bool ok = cell::decode_save_from_memory(dbpf, out, info, err);
        check(ok, "decode position");
        check(out.size() == 1, "one creature");
        check(out[0].pos_x == 0x04F30503u, "pos_x raw");
        check(out[0].pos_y == 0xEE0F01F4u, "pos_y raw");
    }

    // Test 3: empty stream
    {
        auto payload = make_stream(0, {});
        auto dbpf = make_dbpf(payload);
        std::vector<cell::CreatureStats> out;
        cell::StreamInfo info;
        std::string err;
        bool ok = cell::decode_save_from_memory(dbpf, out, info, err);
        check(ok, "decode empty stream");
        check(out.empty(), "no creatures");
    }

    // Test 4: invalid DBPF rejected
    {
        std::vector<u8> bad = {0x4E, 0x4F, 0x54, 0x44}; // "NOTD"
        std::vector<cell::CreatureStats> out;
        cell::StreamInfo info;
        std::string err;
        bool ok = cell::decode_save_from_memory(bad, out, info, err);
        check(!ok, "invalid DBPF rejected");
        check(!err.empty(), "error message present");
    }

    // Test 5: multiple objects
    {
        auto sval = be_bytes(100);
        auto payload = make_stream(3, {
            {{0x0240E421u, sval}},
            {{0x0240E421u, sval}},
            {{0x0240E421u, sval}}
        });
        auto dbpf = make_dbpf(payload);
        std::vector<cell::CreatureStats> out;
        cell::StreamInfo info;
        std::string err;
        bool ok = cell::decode_save_from_memory(dbpf, out, info, err);
        check(ok, "decode multiple");
        check(out.size() == 3, "three creatures");
    }

    if (failures) {
        std::cerr << failures << " cell_save_verify check(s) failed\n";
        return 1;
    }
    std::cout << "cell_save_verify: 5 constructed-input checks PASS\n";
    return 0;
}
