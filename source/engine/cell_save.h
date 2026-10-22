#pragma once
// Cell-stage save reader — decodes creature stat/position data from the
// cell-stage .spo save file. Mirrors the field descriptors at binary VA
// 0xF194A0 (file offset 0xF188B8).
//
// Stream format (discovered 2026-10-16):
//   [count BE u32][crc BE u32][token BE u32][12-byte header]
//   then creature groups. One creature = a GROUP of serialized field frames
//   (tag 0x01A80D26) with field-count pattern [5][4][25][11][4][2][35][8|13]
//   separated by 0-8 B, closed by a 24-byte object trailer (150 groups in
//   stream 0x018EB4B7, 100 in 0x018EB45E on the real save). The envelope
//   `count` word is NOT the group count.
//
// Field IDs and their meaning (from binary descriptor table).
// Field payload values are big-endian (verified: 3f800000=1.0, 0000004f=79).
//   0x0240E421: stat0 (u32) — offset 0x00, default 0
//   0x0240E422: stat1 (u8)  — offset 0x04, default 0
//   0x0240E423: stat2 (u32) — offset 0x08, default 0xFFFFFFFF
//   0x02E9EFD3: stat3 (u32) — offset 0x0C, default 0
//   0x03CDD54B: stat4 (u32) — offset 0x10, default 79. In the shipped save this is the object
//               sequence id: it counts 79, 80, 81 … across a stream's groups.
//   0x024162BF..C2: packed int coordinates, NOT floats (the wire values look like int16 pairs)
//   0x02416C5E: 12 B, in data always zeros (vec3-sized)
//   0x02416C60: 16 B, in data 0,0,0,1.0 (vec4/quat-sized)
//   0x02416C64: f32, in data 1.0
//   0x02416C5E..0x02416C6E: detailed stats (15 fields)
//
// Read-only. No writes to the save file. No game-state mutation.
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "common.h"

namespace os::cell {

struct CreatureStats {
    u32 stat0 = 0;         // 0x0240E421
    u8 stat1 = 0;          // 0x0240E422
    u32 stat2 = 0xFFFFFFFF; // 0x0240E423
    u32 stat3 = 0;         // 0x02E9EFD3
    u32 stat4 = 79;        // 0x03CDD54B — object sequence id (79,80,81…)
    // 0x024162BF..C2: NOT floats — packed int coordinates (CB chains are
    // the u32 families; wire values like 04f30503 = int16 pairs 1267/1283).
    // Kept raw until the packing is decoded.
    // The shipped position block: 0x024162BF and 0x024162C2 are 0 in all 150 shipped
    // groups; 0x024162C0 holds the coordinates as a packed int16 pair (x, y) and
    // 0x024162C1 a second packed pair. The wire scale is not recovered.
    u32 pos_x = 0;         // 0x024162BF (unused by the shipped save)
    u32 pos_y = 0;         // 0x024162C0 (the packed position)
    u32 pos_z = 0;         // 0x024162C1 (the second packed pair)
    u32 pos_w = 0;         // 0x024162C2 (unused by the shipped save)
    // Detailed stats (frame type 3+), all big-endian payloads
    std::optional<std::array<float, 3>> detailed_pos;  // 0x02416C5E, 12 B
    std::optional<std::array<float, 4>> stat5;   // 0x02416C60, 16 B (0,0,0,1.0)
    std::optional<u8> stat6;    // 0x02416C61
    std::optional<u8> stat7;    // 0x02416C62
    std::optional<u8> stat8;    // 0x02416C63
    std::optional<float> stat9;   // 0x02416C64
    std::optional<u8> stat10;   // 0x02416C65
    std::optional<u8> stat11;   // 0x02416C66
    std::optional<u8> stat12;   // 0x02416C67
    std::optional<u8> stat13;   // 0x02416C68
    std::optional<u8> stat14;   // 0x02416C69
    std::optional<u8> stat15;   // 0x02416C6A
    std::optional<u32> stat16;  // 0x02416C6B
    std::optional<u8> stat17;   // 0x02416C6C
    std::optional<u32> stat18;  // 0x02416C6D
    std::optional<u32> stat19;  // 0x02416C6E
    // The engine's two progress slots. Their GAME meaning is UNPROVEN: every one of the 250
    // shipped creatures stores 0 in both (analysis/audit/cell-save-full.json), so they cannot be
    // matched against a shipped value. The engine's use of them - DNA earned in `dna`, cells
    // eaten in `eaten` - is a host convention, not a recovered semantic.
    u32 dna = 0;    // 0x043C4AB2
    u32 eaten = 0;  // 0x04BFED68
};

struct StreamInfo {
    u32 declared_count = 0;
    u32 crc = 0;
    u32 token = 0;
    std::string header_hex;
    u32 objects_decoded = 0;
    u32 bytes_consumed = 0;
    u32 total_bytes = 0;
};

// Decode one cell-stage save file. Returns creature objects with stats.
// Read-only: opens file, decodes DBPF + RefPack, extracts field values.
// No writes, no game-state mutation.
bool decode_save(const std::string& path, std::vector<CreatureStats>& out,
                 StreamInfo& info, std::string& error);
// In-memory variant for tests (no filesystem dependency).
bool decode_save_from_memory(const std::vector<u8>& data, std::vector<CreatureStats>& out,
                             StreamInfo& info, std::string& error);

}  // namespace os::cell
