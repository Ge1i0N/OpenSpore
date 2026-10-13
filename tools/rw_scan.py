"""Find and inspect `rw4` (0x2F4E681B) resources: RenderWare 4 streams.

RenderWare stores everything as a chunk tree: `u32 chunkType; u32 chunkSize; u32 version`
(plus a 0x80000000 child flag). Once a decoded rw4 payload begins with `10 00 00 00 .. .. .. ..`
we are reading RW's own stream format, and the chunk type/version identify the RW build.

This walks a decoded resource's chunk tree without needing any RenderWare documentation,
so the result is authority #2 (the shipped data) rather than a wiki.

Usage:
    python tools/rw_scan.py --find                 # list packages/records that hold rw4
    python tools/rw_scan.py --pkg <i> --rec <n>    # hexdump + chunk tree of one record
    python tools/rw_scan.py --pkg <i> --rec <n> --depth 4
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pkg_kinds import DIRS, HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE
from tools.refpack import decode, is_refpack

RW4_TYPE = 0x2F4E681B

# The handful of RW chunk ids that matter for orientation. Values are the published RW
# constants AND are printed with their raw number so a mismatch is visible, not hidden.
RW_CHUNKS = {
    0x0001: "STRUCT",
    0x0002: "STRING",
    0x0003: "EXTENSION",
    0x0005: "CAMERA",
    0x0006: "TEXTURE",
    0x0007: "MATERIAL",
    0x0008: "MATERIAL_LIST",
    0x0009: "ATOMIC",
    0x000A: "PLANE_SECTOR",
    0x000B: "WORLD",
    0x000D: "LIGHT",
    0x000E: "UNICODE",
    0x000F: "ATOMIC_SECTOR",
    0x0010: "GEOMETRY",
    0x0014: "CLUMP",
    0x0015: "FRAME_LIST",
    0x0016: "FRAME",
    0x0018: "GEOMETRY_LIST",
    0x001A: "WORLD",
    0x001B: "SKIN",
    0x001C: "MATERIAL_SPLIT",
    0x001D: "H_ANIM",
    0x001E: "H_ANIM_PLG",
    0x0101: "TEXDICTIONARY",
    0x0102: "IMAGE",
    0x0180: "PIPELINE",
    0x0F00: "ATOMIC_PLG",
    0x0F01: "GEOMETRY_PLG",
    0x0F02: "FRAME_PLG",
    0x0F03: "MATERIAL_PLG",
    0x0F04: "FRAMELIST_PLG",
    0x0F05: "LIGHT_PLG",
    0x0F06: "CAMERA_PLG",
    0x0F07: "WORLD_PLG",
    0x0F08: "MATERIALLIST_PLG",
    0x0F09: "TEXDICTIONARY_PLG",
    0x0F0A: "RASTER_PLG",
    0x0F0B: "CLUMP_PLG",
    0x0F0E: "H_ANIM_PLG2",
    0x0F0F: "PIPELINE_PLG",
    0x0F10: "TEXDICTIONARY_PLG2",
    0x0F11: "GEOMETRY_PLG2",
    0x0F12: "ATOMIC_PLG2",
    0x0F13: "FRAMELIST_PLG2",
    0x0F14: "CLUMP_PLG2",
    0x0F1A: "IMAGE_PLG",
    0x0F1B: "SKIN_PLG",
    0x0F1C: "GEOMETRY_PLG3",
}


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def find_rw4(limit: int = 12):
    found = []
    for f in sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package")):
        raw = f.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        if kind not in KINDS:
            continue
        prefix, stride, fld = KINDS[kind]
        if isize != prefix + stride * count:
            continue
        blob = raw[use + prefix : use + prefix + stride * count]
        for n in range(count):
            w = struct.unpack("<%dI" % (stride // 4), blob[n * stride : (n + 1) * stride])
            if w[fld["typeid"]] != RW4_TYPE:
                continue
            off = w[fld["offset"]]
            packed = w[fld["size"]] & 0x7FFFFFFF
            mem = w[fld["usize"]]
            comp = w[fld["sentinel"]] & 0xFFFF
            found.append((f, n, off, packed, mem, comp))
            if len(found) >= limit:
                return found
    return found


def walk(data: bytes, base: int, end: int, depth: int, maxdepth: int, path: str = "") -> None:
    i = base
    while i + 12 <= end:
        ctype, csize, cver = struct.unpack("<III", data[i : i + 12])
        child = bool(ctype & 0x80000000)
        tid = ctype & 0x7FFFFFFF
        name = RW_CHUNKS.get(tid, "?")
        out(f"{'  '*depth}{path}0x{i:06x}: type=0x{tid:08x} ({name}) size={csize} ver=0x{cver:08x} child={int(child)}")
        body = i + 12
        if child and depth < maxdepth:
            # a child chunk's size spans its whole subtree; walk inside [body, i+csize)
            inner_end = min(end, i + csize) if csize else end
            walk(data, body, inner_end, depth + 1, maxdepth, "")
            i += csize if csize else (inner_end - i)
        else:
            i += csize if csize else (end - i)
        if csize == 0:
            return


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--find", action="store_true")
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int, default=0)
    ap.add_argument("--depth", type=int, default=4)
    ap.add_argument("--hexdump", type=int, default=0)
    a = ap.parse_args()

    if a.find:
        rws = find_rw4()
        out(f"# {len(rws)} rw4 (0x{RW4_TYPE:08x}) records found (capped)")
        for f, n, off, packed, mem, comp in rws:
            out(f"   {f.name:<28} rec={n:<6} off=0x{off:<9x} packed={packed:<9} mem={mem:<9} comp=0x{comp:04x}")
        return 0

    if a.pkg is None:
        ap.print_help()
        return 2
    allpk: list[Path] = []
    seen = set()
    for d in DIRS:
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*.package")):
            k = str(p.resolve())
            if k not in seen:
                seen.add(k)
                allpk.append(p)
    p = allpk[a.pkg]
    raw = p.read_bytes()
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
    isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
    ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
    alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
    use = ioff or alt
    kind = struct.unpack("<I", raw[use : use + 4])[0]
    prefix, stride, fld = KINDS[kind]
    w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + a.rec * stride :][:stride])
    off, packed, mem = w[fld["offset"]], w[fld["size"]] & 0x7FFFFFFF, w[fld["usize"]]
    comp = w[fld["sentinel"]] & 0xFFFF
    payload = raw[off : off + packed]
    out(f"# {p.name} rec[{a.rec}] type=0x{w[fld['typeid']]:08x} off=0x{off:x} packed={packed} mem={mem} comp=0x{comp:04x}")
    if comp == 0xFFFF:
        data = decode(payload, mem)
        out(f"# refpack decoded to {len(data)} bytes (== mem {mem})")
    else:
        data = payload
    out(f"# first 32: {data[:32].hex(' ')}")
    if a.hexdump:
        for i in range(0, min(a.hexdump, len(data)), 16):
            row = data[i : i + 16]
            out(f"  {i:06x}  {row.hex(' '):<47} |" + "".join(chr(b) if 32 <= b < 127 else "." for b in row) + "|")
    out("")
    out("# chunk tree:")
    walk(data, 0, len(data), 0, a.depth)
    return 0


if __name__ == "__main__":
    sys.exit(main())
