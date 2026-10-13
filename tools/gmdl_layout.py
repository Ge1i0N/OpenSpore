r"""Derive the `gmdl` mesh layout by self-consistency, the way RefPack was cracked.

Method: a mesh interpretation is (vertex_offset, vertex_stride, vertex_count, index_offset,
index_format, index_count). Real geometry satisfies an exactness property that a wrong guess
almost never does: **every index is < vertex_count**, and the index run ends exactly where the
declared index_count says. Enumerate candidates and keep only those that hold.

No third-party format description is used; the layout comes from the shipped bytes only.

Usage: python tools/gmdl_layout.py --pkg <i> [--rec <n>]
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

GMDL_TYPE = 0x00E6BCE5


def first_gmdl(pkg_index: int):
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
            _, data = record_payload(p, n)
            return p, n, data
    return p, -1, b""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", type=int, required=True)
    ap.add_argument("--rec", type=int, default=-1)
    ap.add_argument("--min-verts", type=int, default=16)
    a = ap.parse_args()
    p, rec, data = first_gmdl(a.pkg)
    if rec < 0:
        print("no gmdl here")
        return 1
    print(f"# {p.name} rec[{rec}] gmdl {len(data)} bytes")

    # Candidate vertex arrays: any 4-byte-aligned offset where a run of finite floats begins.
    # A vertex is 12 bytes (vec3). We test many (off, stride, count) and score by whether a
    # following u16 index array is fully in range.
    best = []
    n4 = len(data) // 4
    for voff in range(0, min(0x400, len(data) - 12), 4):
        # require a plausible first vertex (three finite, bounded floats)
        try:
            v0 = struct.unpack("<3f", data[voff : voff + 12])
        except struct.error:
            continue
        if not all(v == v and abs(v) < 1e4 for v in v0):
            continue
        for stride in (12, 16, 20, 24, 28, 32):
            maxn = (len(data) - voff) // stride
            if maxn < a.min_verts:
                continue
            for vcount in range(a.min_verts, maxn + 1):
                vend = voff + vcount * stride
                if vend + 8 > len(data):
                    break
                # try an index array right after the vertices, u16
                for ioff in (vend, vend + 4, vend + 8, vend + 12):
                    if ioff + 6 > len(data):
                        continue
                    icount = (len(data) - ioff) // 2
                    if icount < 3:
                        continue
                    idx = struct.unpack("<%dH" % min(icount, 4000), data[ioff : ioff + 2 * min(icount, 4000)])
                    if max(idx) < vcount and max(idx) > 0 and min(idx) == 0:
                        best.append((voff, stride, vcount, ioff, len(data) - ioff))
    # de-duplicate and rank by how much of the file the interpretation covers
    best = sorted(set(best), key=lambda t: -(t[2] * 12))
    seen = set()
    print(f"\n# self-consistent (vertex_offset, stride, vertex_count, index_offset, index_bytes):")
    shown = 0
    for voff, stride, vcount, ioff, ibytes in best:
        key = (voff, stride, vcount)
        if key in seen:
            continue
        seen.add(key)
        print(f"   voff=0x{voff:06x} stride={stride} verts={vcount} ioff=0x{ioff:06x} index_bytes={ibytes}")
        shown += 1
        if shown >= 12:
            break
    if not best:
        print("   (none - layout needs a header field, not a plain scan)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
