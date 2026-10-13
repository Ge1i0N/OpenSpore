r"""Identify the 6 non-position floats in a gmdl vertex by statistics over ALL vertices.

The position is the first float3 (verified: reproduces the bbox exactly). The remaining six
floats are declared by the vertex-declaration table but their meaning is not reversed. This tool
does not guess: it computes, per channel, the range, how often it is in [0,1] (a UV/normalised
candidate), how often it is exactly 0, and whether the (3,4,5) and (6,7,8) triplets look like unit
normals. That is what separates a real attribute from a coincidence.

Usage: python tools/gmdl_attribs.py --pkg 48
"""
from __future__ import annotations

import argparse
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def channels(data: bytes):
    """Return (N, list of 9-tuples) for a decoded gmdl payload, reusing the mesh reader."""
    from tools.gmdl_mesh import find_descriptors

    descs = find_descriptors(data)
    if not descs:
        return 0, []
    d0 = descs[0]
    d = d0
    while True:
        fmt, cnt, res, ib = struct.unpack("<4I", data[d : d + 16])
        if fmt != 4 or res != 16 or ib != cnt * 2:
            break
        d += 16 + ib
    n = struct.unpack("<I", data[d + 4 : d + 8])[0]
    vc_off = 0x1C + (n - 1) * 12
    vc = struct.unpack("<I", data[d + vc_off : d + vc_off + 4])[0]
    vdata = d + vc_off + 8
    rows = []
    for i in range(vc):
        rows.append(struct.unpack("<9f", data[vdata + i * 36 : vdata + i * 36 + 36]))
    return n, rows


def stats(rows):
    out = []
    for c in range(9):
        vals = [r[c] for r in rows]
        finite = [v for v in vals if math.isfinite(v)]
        if not finite:
            out.append(None)
            continue
        in01 = sum(1 for v in finite if 0.0 <= v <= 1.0)
        zeros = sum(1 for v in finite if v == 0.0)
        out.append({
            "min": min(finite), "max": max(finite),
            "mean": sum(finite) / len(finite),
            "in01": 100.0 * in01 / len(finite),
            "zero": 100.0 * zeros / len(finite),
        })
    return out


def unit_triplet(rows, a):
    n = 0
    for r in rows:
        v = r[a:a + 3]
        if all(math.isfinite(x) for x in v):
            l = math.sqrt(v[0] ** 2 + v[1] ** 2 + v[2] ** 2)
            if abs(l - 1.0) < 0.02:
                n += 1
    return 100.0 * n / len(rows) if rows else 0.0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", type=int, required=True)
    a = ap.parse_args()
    from tools.gmdl_mesh import find_gmdl

    p, rec, data = find_gmdl(a.pkg)
    if rec < 0:
        print("no gmdl here")
        return 1
    n, rows = channels(data)
    if not rows:
        print("could not read vertices")
        return 1
    print(f"# {p.name} rec[{rec}]  declaration N={n}  {len(rows)} vertices, stride 36 (9 floats)")
    s = stats(rows)
    print(f"{'ch':>3} {'min':>11} {'max':>11} {'mean':>11} {'in[0,1]%':>9} {'==0%':>7}")
    for c in range(9):
        st = s[c]
        if st is None:
            print(f"{c:>3}   <non-finite>")
            continue
        print(f"{c:>3} {st['min']:>11.4f} {st['max']:>11.4f} {st['mean']:>11.4f} "
              f"{st['in01']:>8.1f}% {st['zero']:>6.1f}%")
    print(f"\n|(3,4,5)| == 1 (unit normal?) : {unit_triplet(rows, 3):.1f}% of vertices")
    print(f"|(6,7,8)| == 1 (unit normal?) : {unit_triplet(rows, 6):.1f}% of vertices")
    return 0


if __name__ == "__main__":
    sys.exit(main())
