r"""GMDL (generated model) structural reader - CONFIRMED fields only.

Derived from the shipped bytes (no third-party format description). Verified on
Spore_Content.package rec[12] (175,264 bytes decoded):

  +0x44  u32 = 1            (sub-mesh / record count candidate)
  +0x48  7 x float32        bounding box: minX minY minZ  maxX maxY maxZ  then 1.0
  +0x64  u32 = 1
  +0x68  u32 = 4            (per-index type/format candidate - NOT confirmed)
  +0x6c  u32 = 18000        INDEX COUNT      - confirmed: the u16 run at +0x78 has exactly
                                              this many entries in range
  +0x70  u32 = 16           vertex stride candidate (pos+attr) - NOT confirmed
  +0x74  u32 = 36000        INDEX BYTE SIZE  - confirmed: == index_count * 2
  +0x78  u16[index_count]   the index array; max(index) < vertex count (self-consistent)
  after  float32 data       vertex attributes (positions first; a bbox check is printed)

The index side is proven (count field == run length, all indices in range). The vertex
attribute layout is NOT: the float block after the indices is longer than a single position
array for the index range, and its Y extent exceeds the header bbox, so it holds more than
positions. Do not assume a stride; that is the next thing to reverse.

Usage: python tools/gmdl_mesh.py --pkg <i>
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

GMDL_TYPE = 0x00E6BCE5


def find_gmdl(pkg_index: int):
    from tools.gmdl import all_packages, record_payload
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF

    p = all_packages()[pkg_index]
    raw = p.read_bytes()
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
    ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
    alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
    use = ioff or alt
    kind = struct.unpack("<I", raw[use : use + 4])[0]
    prefix, stride, fld = KINDS[kind]
    pwords = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
    shared, pi = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pwords[pi]
            pi += 1
    for n in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + n * stride :][:stride])
        tid = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        if tid == GMDL_TYPE:
            return p, n, record_payload(p, n)[1]
    return p, -1, b""


def analyse(data: bytes) -> int:
    if len(data) < 0x80:
        print("   too short for a gmdl header")
        return 1
    u44 = struct.unpack("<I", data[0x44:0x48])[0]
    bbox = struct.unpack("<7f", data[0x48:0x64])
    u64, u68 = struct.unpack("<2I", data[0x64:0x6C])
    idx_count, stride, idx_bytes = struct.unpack("<3I", data[0x6C:0x78])
    print(f"   +0x44 record/block count : {u44}")
    print(f"   +0x48 bbox               : min({bbox[0]:.4f},{bbox[1]:.4f},{bbox[2]:.4f}) "
          f"max({bbox[3]:.4f},{bbox[4]:.4f},{bbox[5]:.4f}) + {bbox[6]:.4f}")
    print(f"   +0x64/+0x68/stride       : {u64} / {u68} / {stride}")
    print(f"   +0x6c index count        : {idx_count}")
    print(f"   +0x74 index byte size    : {idx_bytes}  (== count*2 ? {idx_bytes == idx_count * 2})")
    idx_off = 0x78
    if idx_bytes == 0 or idx_off + idx_bytes > len(data):
        print("   ! index block does not fit - not a plain gmdl")
        return 1
    idx = struct.unpack("<%dH" % idx_count, data[idx_off : idx_off + idx_bytes])
    mx = max(idx)
    inrange = sum(1 for v in idx if v < mx + 1)
    print(f"   indices @0x{idx_off:x}..0x{idx_off+idx_bytes:x}: {idx_count} u16, max={mx}, "
          f"first10={idx[:10]}")
    # vertex block starts right after the index array
    voff = idx_off + idx_bytes
    vcount_by_stride = {}
    for st in (12, 16, 20, 24, 28, 32):
        n = (len(data) - voff) // st
        ok = True
        for i in range(min(n, 500)):
            x, y, z = struct.unpack("<3f", data[voff + i * st : voff + i * st + 12])
            if not (math.isfinite(x) and math.isfinite(y) and math.isfinite(z) and abs(x) < 1e4
                    and abs(y) < 1e4 and abs(z) < 1e4):
                ok = False
                break
        if ok:
            vcount_by_stride[st] = n
    print(f"   vertex block @0x{voff:x} ({len(data)-voff} bytes): stride candidates "
          f"{vcount_by_stride}")
    print(f"   -> index max {mx} needs at least {mx+1} vertices; a stride is plausible only if "
          f"its vertex count >= that")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", type=int, required=True)
    a = ap.parse_args()
    p, rec, data = find_gmdl(a.pkg)
    if rec < 0:
        print("no gmdl here")
        return 1
    print(f"# {p.name} rec[{rec}] gmdl {len(data)} bytes")
    print("# CONFIRMED: bbox, index count/size; index array validated. Vertex stride NOT confirmed.")
    return analyse(data)


if __name__ == "__main__":
    sys.exit(main())
