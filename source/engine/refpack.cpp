#include "engine/refpack.h"

namespace os::refpack {

bool is_refpack(const u8* data, size_t len) {
    if (!data || len < 2) return false;
    // The detector at 0x0092cd40 masks: (& 0x1FFF) == 0x10FB. Top bits are flags, so both
    // 0x10FB and 0x50FB pass - and the shipped packages contain both.
    u32 word = ((u32)data[0] << 8) | (u32)data[1];
    return (word & 0x1FFFu) == 0x10FBu;
}

u32 decode(const u8* src, u32 src_len, u32 expected, std::vector<u8>& out) {
    if (!is_refpack(src, src_len)) return (u32)-1;
    const u32 word = ((u32)src[0] << 8) | (u32)src[1];
    const u32 nlen = (word & 0x8000u) ? 4u : 3u;  // bit15 selects a 3- or 4-byte length
    if (src_len < 2 + nlen) return (u32)-1;
    u32 size = 0;
    for (u32 i = 0; i < nlen; ++i) size = (size << 8) | (u32)src[2 + i];  // BIG-endian
    // `expected` comes from the DBPF index memSize. Enforcing it turns a silently wrong
    // decode into a detectable failure (this is how the +1 loop-count bug was caught).
    if (expected != 0 && size != expected) return (u32)-1;

    out.clear();
    out.reserve(size);
    u32 i = 2 + nlen;
    for (;;) {
        if (i >= src_len) return (u32)-1;
        const u8 c = src[i++];
        u32 lits, len, dist;
        if (c < 0x80) {
            if (i >= src_len) return (u32)-1;
            const u8 b1 = src[i++];
            lits = c & 3u;
            len = ((c >> 2) & 7u) + 3u;               // +3: asm loop runs N+1 times
            dist = ((u32)(c & 0x60u) << 3) + b1 + 1u;
        } else if (c < 0xC0) {
            if (i + 1 >= src_len) return (u32)-1;
            const u8 b0 = src[i], b1 = src[i + 1];
            i += 2;
            lits = b0 >> 6;
            len = (c & 0x3Fu) + 4u;                    // +4, same reason
            dist = ((u32)(b0 & 0x3Fu) << 8) + b1 + 1u;
        } else if (c < 0xE0) {
            if (i + 2 >= src_len) return (u32)-1;
            const u8 b0 = src[i], b1 = src[i + 1], b2 = src[i + 2];
            i += 3;
            lits = c & 3u;
            len = ((u32)(c & 0x0Cu) << 6) + b2 + 5u;    // (c&0xC)<<6 per 0x92c71f, +5 per loop
            dist = ((u32)(c & 0x10u) << 12) + ((u32)b0 << 8) + b1 + 1u;
        } else {
            len = ((u32)(c & 0x1Fu) << 2) + 4u;
            if (len > 0x70u) {
                // 0xFC..0xFF: tail - copy (c & 3) literals and stop
                lits = c & 3u;
                if (i + lits > src_len) return (u32)-1;
                for (u32 k = 0; k < lits; ++k) out.push_back(src[i++]);
                break;
            }
            if (i + len > src_len) return (u32)-1;
            for (u32 k = 0; k < len; ++k) out.push_back(src[i++]);
            continue;
        }
        if (lits) {
            if (i + lits > src_len) return (u32)-1;
            for (u32 k = 0; k < lits; ++k) out.push_back(src[i++]);
        }
        if (dist == 0 || dist > out.size()) return (u32)-1;
        const size_t s = out.size() - dist;
        // Byte-by-byte: a match may overlap its own output (RLE semantics in the original).
        for (u32 k = 0; k < len; ++k) out.push_back(out[s + k]);
    }
    if (out.size() != size) return (u32)-1;
    return (u32)out.size();
}

}  // namespace os::refpack
