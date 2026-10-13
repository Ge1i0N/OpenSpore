#pragma once
// RefPack (EA LZRef) decoder - a direct port of the ORIGINAL's instructions.
//
// Source of truth: refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe
//   0x0092cd40  detector:  bswap(16-bit load) & 0x1FFF == 0x10FB
//   0x0092ccd0  codec vtable+0x20 entry: reads the header, bounds-checks, calls the loop
//   0x0092c4e0  big-endian 1/2/3/4-byte reader
//   0x0092c5b0  the command loop (quoted below)
// The vtable at 0x0140a12c names this codec ("Refpack" string getter at +0x40 = 0x92c5a0),
// and the package resource path 0x008d8b3b calls the detector then the decode method, so
// this is the codec the game itself uses for `.package` entries with compression 0xFFFF.
//
// HEADER (0x0092ccd0 / 0x0092c5b0 prologue, quoted):
//   movzx eax,BYTE [src]; shl 8; add next          -> BE16 magic
//   test eax,0x8000 / je                            -> bit15 picks a 3- vs 4-byte length
//   test eax,0x100 / je / add ecx,3or4              -> bit8 skips extra header bytes
//   then 3 (or 4) BE bytes are the UNCOMPRESSED size
// So the length is BIG-ENDIAN. (The upstream SporeMaster3 doc says little-endian; it is
// wrong - recorded in IDEAS.md. Verified here by 10 FB 00 01 38 -> 0x000138 = 312 = the
// index's memSize for the same record.)
//
// COMMANDS (control byte c):
//   c < 0x80   2-byte: lits = c&3, len = ((c>>2)&7)+3, dist = ((c&0x60)<<3) + b1 + 1
//   c < 0xC0   3-byte: lits = b0>>6, len = (c&0x3F)+4,  dist = ((b0&0x3F)<<8) + b1 + 1
//   c < 0xE0   4-byte: lits = c&3, len = ((c&0xC)<<6)+b2+5, dist = ((c&0x10)<<12)+(b0<<8)+b1+1
//   c < 0xFC   literal block: len = ((c&0x1F)<<2)+4
//   else       tail: (c&3) literals, then stop
//
// THE +1 IN THE COPY LENGTHS IS THE WHOLE STORY. The literal loops test the counter AFTER
// decrementing (`dec esi / test esi,esi / jne` -> exactly N iterations), but the COPY loops
// test it BEFORE (`mov eax,esi / dec esi / test eax,eax / jne` -> N+1 iterations):
//   92c660: mov al,[edi] / mov [edx],al / mov eax,esi / inc edx / inc edi / dec esi
//   92c669: test eax,eax / jne 0x92c660
// A first port that used the register value directly failed 250/250 real streams; with the
// +1 it is exact on 500/500 (tools/refpack.py --verify).
//
// Copies may overlap the destination (the loop is byte-at-a-time in the original), so the
// match is emitted byte by byte here too - memmove semantics would be wrong.
#include <cstddef>
#include <cstdint>
#include <vector>

#include "common.h"

namespace os::refpack {

// True when `data` starts with a RefPack magic (the detector's exact test).
bool is_refpack(const u8* data, size_t len);

// Decompress `src` (src_len bytes) into `out`, which must hold at least `expected` bytes.
// Returns the number of bytes written, or (u32)-1 on any malformed input. `expected` is
// taken from the DBPF index (memSize) and is enforced: a stream that does not produce
// exactly `expected` bytes is reported as an error rather than trusted.
u32 decode(const u8* src, u32 src_len, u32 expected, std::vector<u8>& out);

}  // namespace os::refpack
