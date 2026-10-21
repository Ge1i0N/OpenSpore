#include "engine/raster.h"

#include <cstring>

namespace os::raster {
namespace {

inline void rgb565(u16 v, u8& r, u8& g, u8& b) {
    u8 r5 = (u8)((v >> 11) & 0x1F);
    u8 g6 = (u8)((v >> 5) & 0x3F);
    u8 b5 = (u8)(v & 0x1F);
    r = (u8)((r5 << 3) | (r5 >> 2));
    g = (u8)((g6 << 2) | (g6 >> 4));
    b = (u8)((b5 << 3) | (b5 >> 2));
}

// DXT1 (8-byte blocks) / DXT5 (16-byte blocks, interpolated alpha) -> BGRA.
void decode_block(const u8* p, bool dxt1, u8 out[16][4]) {
    u8 alpha[8];
    const u8* col = p;
    if (!dxt1) {
        u8 a0 = p[0], a1 = p[1];
        u64 abits = (u64)p[2] | ((u64)p[3] << 8) | ((u64)p[4] << 16) | ((u64)p[5] << 24)
                    | ((u64)p[6] << 32) | ((u64)p[7] << 40);
        alpha[0] = a0;
        alpha[1] = a1;
        if (a0 > a1) {
            for (int i = 1; i <= 6; ++i) alpha[i + 1] = (u8)(((7 - i) * a0 + i * a1) / 7);
        } else {
            for (int i = 1; i <= 4; ++i) alpha[i + 1] = (u8)(((5 - i) * a0 + i * a1) / 5);
            alpha[6] = 0;
            alpha[7] = 255;
        }
        col = p + 8;
        // stash abits in the caller via the low bits of a1? No - recompute below.
        u16 c0 = (u16)(col[0] | (col[1] << 8));
        u16 c1 = (u16)(col[2] | (col[3] << 8));
        u32 bits = (u32)col[4] | ((u32)col[5] << 8) | ((u32)col[6] << 16) | ((u32)col[7] << 24);
        u8 r0, g0, b0, r1, g1, b1;
        rgb565(c0, r0, g0, b0);
        rgb565(c1, r1, g1, b1);
        u8 cr[4] = {r0, r1, 0, 0}, cg[4] = {g0, g1, 0, 0}, cb[4] = {b0, b1, 0, 0};
        if (c0 > c1) {
            cr[2] = (u8)((2 * r0 + r1) / 3); cg[2] = (u8)((2 * g0 + g1) / 3); cb[2] = (u8)((2 * b0 + b1) / 3);
            cr[3] = (u8)((r0 + 2 * r1) / 3); cg[3] = (u8)((g0 + 2 * g1) / 3); cb[3] = (u8)((b0 + 2 * b1) / 3);
        } else {
            cr[2] = (u8)((r0 + r1) / 2); cg[2] = (u8)((g0 + g1) / 2); cb[2] = (u8)((b0 + b1) / 2);
            cr[3] = cg[3] = cb[3] = 0;
        }
        for (int i = 0; i < 16; ++i) {
            int ci = (bits >> (2 * i)) & 3;
            int ai = (int)((abits >> (3 * i)) & 7);
            u8 av = alpha[ai];
            out[i][0] = (u8)(cb[ci] * av / 255);
            out[i][1] = (u8)(cg[ci] * av / 255);
            out[i][2] = (u8)(cr[ci] * av / 255);
            out[i][3] = av;
        }
        return;
    }
    // DXT1
    u16 c0 = (u16)(p[0] | (p[1] << 8));
    u16 c1 = (u16)(p[2] | (p[3] << 8));
    u32 bits = (u32)p[4] | ((u32)p[5] << 8) | ((u32)p[6] << 16) | ((u32)p[7] << 24);
    u8 r0, g0, b0, r1, g1, b1;
    rgb565(c0, r0, g0, b0);
    rgb565(c1, r1, g1, b1);
    u8 cr[4] = {r0, r1, 0, 0}, cg[4] = {g0, g1, 0, 0}, cb[4] = {b0, b1, 0, 0};
    u8 alphas[4] = {255, 255, 255, 255};
    if (c0 > c1) {
        cr[2] = (u8)((2 * r0 + r1) / 3); cg[2] = (u8)((2 * g0 + g1) / 3); cb[2] = (u8)((2 * b0 + b1) / 3);
        cr[3] = (u8)((r0 + 2 * r1) / 3); cg[3] = (u8)((g0 + 2 * g1) / 3); cb[3] = (u8)((b0 + 2 * b1) / 3);
    } else {
        cr[2] = (u8)((r0 + r1) / 2); cg[2] = (u8)((g0 + g1) / 2); cb[2] = (u8)((b0 + b1) / 2);
        cr[3] = cg[3] = cb[3] = 0;
        alphas[3] = 0;  // DXT1: when c0 <= c1 the fourth colour is the transparent one
    }
    for (int i = 0; i < 16; ++i) {
        int ci = (bits >> (2 * i)) & 3;
        out[i][0] = cb[ci];
        out[i][1] = cg[ci];
        out[i][2] = cr[ci];
        out[i][3] = alphas[ci];
    }
}

}  // namespace

Texture decode(const u8* data, size_t size) {
    Texture t;
    if (!data || size < 28) {
        t.why = "payload too short";
        return t;
    }
    u32 ver, d1, d2, code, k8, base;
    std::memcpy(&ver, data + 0, 4);
    std::memcpy(&d1, data + 4, 4);
    std::memcpy(&d2, data + 8, 4);
    std::memcpy(&code, data + 12, 4);
    std::memcpy(&k8, data + 16, 4);
    std::memcpy(&base, data + 24, 4);
    (void)ver; (void)code; (void)k8;
    if (d1 == 0 || d2 == 0 || d1 > 8192 || d2 > 8192) {
        t.why = "implausible dimensions";
        return t;
    }
    const u8* tag = data + 20;
    const u8* pix = data + 28;
    const size_t avail = size - 28;
    t.width = (int)d1;
    t.height = (int)d2;
    t.bgra.assign((size_t)t.width * t.height * 4, 0);

    const bool dxt1 = std::memcmp(tag, "DXT1", 4) == 0;
    const bool dxt5 = std::memcmp(tag, "DXT5", 4) == 0;
    if (dxt1 || dxt5) {
        const size_t block = dxt1 ? 8 : 16;
        const int bw = t.width / 4, bh = t.height / 4;
        if ((size_t)bw * bh * block > avail) {
            t.why = "compressed data does not fit";
            return t;
        }
        t.format = dxt1 ? "DXT1" : "DXT5";
        u8 px[16][4];
        for (int by = 0; by < bh; ++by) {
            for (int bx = 0; bx < bw; ++bx) {
                decode_block(pix + ((size_t)by * bw + bx) * block, dxt1, px);
                for (int py = 0; py < 4; ++py) {
                    for (int px2 = 0; px2 < 4; ++px2) {
                        const u8* s = px[py * 4 + px2];
                        const int X = bx * 4 + px2, Y = by * 4 + py;
                        u8* o = &t.bgra[((size_t)Y * t.width + X) * 4];
                        o[0] = s[0]; o[1] = s[1]; o[2] = s[2]; o[3] = s[3];
                    }
                }
            }
        }
        t.ok = true;
        return t;
    }
    if (tag[0] == 0x15) {
        if ((size_t)t.width * t.height * 4 > avail) {
            t.why = "raw data does not fit";
            return t;
        }
        t.format = "raw";
        for (size_t i = 0; i < (size_t)t.width * t.height; ++i) {
            // source is B,G,R,A already
            std::memcpy(&t.bgra[i * 4], pix + i * 4, 4);
        }
        t.ok = true;
        return t;
    }
    t.why = "unknown format tag";
    return t;
}

}  // namespace os::raster
