r"""raster (RenderWare texture) decoder: header + DXT1/DXT5/raw -> PNG.

Format derived from the shipped bytes (tools/raster_scan.py over 1200 records):
    +0x00 u32 = 1            (version)
    +0x04 u32 = dim          (h or w; every sample is square so the two are equal)
    +0x08 u32 = dim          (the other dimension)
    +0x0c u32 = code         (10/9/6/7 - mip/quality code, meaning open)
    +0x10 u32 = 8            (constant in every sample)
    +0x14 char[4]            format tag: "DXT5", "DXT1", or u32 0x15 for raw
    +0x18 u32 = base bytes   (== w*h for DXT5, w*h/2 for DXT1, w*h*4 for raw)
    +0x1c ...                pixel data (base level, then mip levels)

This decodes the BASE level only (enough to see the texture). It is a viewer/analysis tool, not
the engine's texture path.

Usage: python tools/raster_decode.py --pkg 48 --rec 0 --png out.png
       python tools/raster_decode.py --first --png out.png
"""
from __future__ import annotations

import argparse
import struct
import sys
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

RASTER = 0x2F4E681C


def rgb565(v: int):
    r = (v >> 11) & 0x1F
    g = (v >> 5) & 0x3F
    b = v & 0x1F
    return (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)


def decode_dxt(data: bytes, w: int, h: int, dxt1: bool):
    out = bytearray(w * h * 3)
    bw, bh = w // 4, h // 4
    pos = 0
    for by in range(bh):
        for bx in range(bw):
            if dxt1:
                c0, c1, bits = struct.unpack_from("<HHI", data, pos)
                pos += 8
                a = [255] * 4
            else:
                a0, a1 = data[pos], data[pos + 1]
                abits = int.from_bytes(data[pos + 2 : pos + 8], "little")
                c0, c1, bits = struct.unpack_from("<HHI", data, pos + 8)
                pos += 16
                a = [a0, a1]
                if a0 > a1:
                    a += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
                else:
                    a += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
            r0, g0, b0 = rgb565(c0)
            r1, g1, b1 = rgb565(c1)
            cols = [(r0, g0, b0), (r1, g1, b1)]
            if c0 > c1 or dxt1:
                cols += [tuple((2 * cols[0][k] + cols[1][k]) // 3 for k in range(3)),
                         tuple((cols[0][k] + 2 * cols[1][k]) // 3 for k in range(3))]
            else:
                cols += [tuple((cols[0][k] + cols[1][k]) // 2 for k in range(3)), (0, 0, 0)]
            for py in range(4):
                for px in range(4):
                    i = py * 4 + px
                    ci = (bits >> (2 * i)) & 3
                    r, g, b = cols[ci]
                    if not dxt1:
                        ai = (abits >> (3 * i)) & 7
                        av = a[ai] if ai < len(a) else 255
                        r = r * av // 255
                        g = g * av // 255
                        b = b * av // 255
                    X, Y = bx * 4 + px, by * 4 + py
                    o = (Y * w + X) * 3
                    out[o], out[o + 1], out[o + 2] = r, g, b
    return out


def write_png(path: Path, w: int, h: int, rgb: bytes):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        raw += rgb[y * w * 3 : (y + 1) * w * 3]

    def chunk(tag, d):
        return struct.pack(">I", len(d)) + tag + d + struct.pack(">I", zlib.crc32(tag + d) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def parse_and_decode(data: bytes):
    ver, d1, d2, code, k8, = struct.unpack_from("<5I", data, 0)
    tag = data[20:24]
    base = struct.unpack_from("<I", data, 24)[0]
    pix = data[28:]
    if tag == b"DXT5":
        return d1, d2, decode_dxt(pix, d1, d2, dxt1=False)
    if tag == b"DXT1":
        return d1, d2, decode_dxt(pix, d1, d2, dxt1=True)
    if tag[:1] == b"\x15":
        w, h = d1, d2
        rgb = bytearray(w * h * 3)
        for i in range(w * h):
            b, g, r, _a = pix[i * 4 : i * 4 + 4]
            rgb[i * 3 : i * 3 + 3] = bytes((r, g, b))
        return w, h, rgb
    return None, None, None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int, default=0)
    ap.add_argument("--first", action="store_true")
    ap.add_argument("--tag", default=None, help="only accept this format tag (DXT1/DXT5/raw)")
    ap.add_argument("--png", required=True)
    a = ap.parse_args()

    from tools.gmdl import all_packages
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
    from tools.refpack import decode as refdecode

    def payload(raw, rec, fld, prefix, stride, shared):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + rec * stride :][:stride])
        off = w[fld["offset"]]
        packed = w[fld["size"]] & 0x7FFFFFFF
        mem = w[fld["usize"]]
        comp = w[fld["sentinel"]] & 0xFFFF
        d = raw[off : off + packed]
        return refdecode(d, mem) if comp == 0xFFFF else d

    for pi, p in enumerate(all_packages()):
        if a.pkg is not None and pi != a.pkg:
            continue
        raw = p.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        if kind not in KINDS:
            continue
        prefix, stride, fld = KINDS[kind]
        pw = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
        shared, q = {}, 1
        for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
            if kind & bit:
                shared[nm] = pw[q]
                q += 1
        for r in range(count):
            w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride :][:stride])
            t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
            if t != RASTER:
                continue
            if not a.first and r != a.rec:
                continue
            data = payload(raw, r, fld, prefix, stride, shared)
            tag = data[20:24]
            if a.tag:
                want = a.tag.encode("ascii")
                is_raw = tag[:1] == b"\x15"
                if (a.tag == "raw" and not is_raw) or (a.tag != "raw" and tag != want):
                    continue
            ww, hh, rgb = parse_and_decode(data)
            if not rgb:
                if a.first:
                    continue
                print("unknown raster format")
                return 1
            write_png(Path(a.png), ww, hh, rgb)
            print(f"{p.name} rec[{r}] {ww}x{hh} {len(data)} bytes -> {a.png}")
            return 0
    print("no raster found")
    return 1


if __name__ == "__main__":
    sys.exit(main())
