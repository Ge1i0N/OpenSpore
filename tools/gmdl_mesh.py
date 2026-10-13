r"""GMDL (generated model) reader - CONFIRMED layout, positions verified exactly.

Derived from the shipped bytes (no third-party format description). Session 12 solved the
whole vertex side; the decisive test is that the extracted positions reproduce the file's
own bounding box EXACTLY (err 0.0000) on every variant tried.

Layout (all fields little-endian):

    ...variable header of {tag, value} groups...
    u32              count             (1 or 2)
    float32 bbox[7]  minXYZ maxXYZ 1.0
    u32              submesh_count     (1 or 2)
    repeat submesh_count times:
        u32  format        (= 4)
        u32  index_count
        u32  reserved      (= 16)
        u32  index_bytes   (== index_count * 2)
        u16  indices[index_count]
    u32  1
    u32  N                (vertex-declaration entry count: 5 or 6)
    u32  0
    u32  2
    u32  0
    u32  0x000C0000
    (N-1) x 12 bytes      vertex declaration
    u32  0
    u32  vertex_count     (== max_index + 1)
    u32  vertex_bytes     (== vertex_count * 36)
    float32 vertices[vertex_count][9]   ; stride 36 bytes; position = the FIRST float3

So the vertex stride is **36**, NOT the descriptor's 16 (that field is a reserved/format code).
Position is the first three floats; the remaining six are other attributes whose meaning
depends on N (for N=6 the last two of the nine read as UVs in [0,1] on every sampled vertex,
for N=5 they do not) - that part is declared by the table and is the remaining open question.

Usage:
    python tools/gmdl_mesh.py --pkg <i>                 # report structure + validate bbox
    python tools/gmdl_mesh.py --pkg <i> --obj out.obj   # export the geometry
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
VERTEX_STRIDE = 36


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


def find_descriptors(data: bytes):
    """Offsets d where the sub-mesh descriptor is self-consistent: {4, C, 16, 2C}, indices fit."""
    out = []
    for d in range(0, max(0, len(data) - 16), 4):
        fmt, cnt, res, ib = struct.unpack("<4I", data[d : d + 16])
        if fmt != 4 or res != 16 or cnt == 0 or ib != cnt * 2:
            continue
        if d + 16 + ib > len(data):
            continue
        out.append(d)
    return out


class Mesh:
    """A parsed gmdl: bbox, sub-meshes (index lists) and the shared vertex position array."""

    def __init__(self):
        self.bbox = (0.0,) * 7
        self.sub_count = 0
        self.indices: list[tuple[int, ...]] = []
        self.decl_n = 0
        self.vertex_count = 0
        self.positions: list[tuple[float, float, float]] = []
        self.vertex_stride = VERTEX_STRIDE


def parse(data: bytes) -> Mesh | None:
    descs = find_descriptors(data)
    if not descs:
        return None
    d0 = descs[0]
    if d0 < 0x20:
        return None
    m = Mesh()
    m.bbox = struct.unpack("<7f", data[d0 - 0x20 : d0 - 0x04])
    m.sub_count = struct.unpack("<I", data[d0 - 0x04 : d0])[0]

    d = d0
    while d + 16 <= len(data):
        fmt, cnt, res, ib = struct.unpack("<4I", data[d : d + 16])
        if fmt != 4 or res != 16 or ib != cnt * 2:
            break
        m.indices.append(struct.unpack("<%dH" % cnt, data[d + 16 : d + 16 + ib]))
        d += 16 + ib

    # vertex block header
    if d + 0x1C > len(data):
        return m
    m.decl_n = struct.unpack("<I", data[d + 4 : d + 8])[0]
    vc_off = 0x1C + (m.decl_n - 1) * 12  # N=6 -> 0x58, N=5 -> 0x4c
    if d + vc_off + 8 > len(data):
        return m
    m.vertex_count = struct.unpack("<I", data[d + vc_off : d + vc_off + 4])[0]
    vdata = d + vc_off + 8
    if vdata + m.vertex_count * VERTEX_STRIDE > len(data):
        return m
    m.positions = [
        struct.unpack("<3f", data[vdata + i * VERTEX_STRIDE : vdata + i * VERTEX_STRIDE + 12])
        for i in range(m.vertex_count)
    ]
    return m


def bbox_error(m: Mesh):
    xs = [p[0] for p in m.positions]
    ys = [p[1] for p in m.positions]
    zs = [p[2] for p in m.positions]
    b = m.bbox
    return (abs(min(xs) - b[0]) + abs(max(xs) - b[3]) + abs(min(ys) - b[1])
            + abs(max(ys) - b[4]) + abs(min(zs) - b[2]) + abs(max(zs) - b[5]))


def write_obj(m: Mesh, path: Path) -> None:
    with path.open("w", encoding="ascii") as f:
        f.write(f"# gmdl: {m.vertex_count} vertices, {len(m.indices)} sub-mesh(es)\n")
        for x, y, z in m.positions:
            f.write(f"v {x:.6f} {y:.6f} {z:.6f}\n")
        base = 1  # OBJ indices are 1-based and shared across sub-meshes (they index one buffer)
        for idx in m.indices:
            for i in range(0, len(idx) - 2, 3):
                f.write(f"f {base + idx[i]} {base + idx[i+1]} {base + idx[i+2]}\n")


def analyse(data: bytes, obj: Path | None) -> int:
    m = parse(data)
    if m is None:
        print("   ! not a readable gmdl (no self-consistent sub-mesh descriptor)")
        return 1
    b = m.bbox
    print(f"   bbox         : min({b[0]:.4f},{b[1]:.4f},{b[2]:.4f}) max({b[3]:.4f},{b[4]:.4f},{b[5]:.4f})")
    print(f"   submesh_count: {m.sub_count}")
    tris = 0
    for i, idx in enumerate(m.indices):
        tris += len(idx) // 3
        print(f"   submesh[{i}]   : {len(idx)} indices ({len(idx)//3} tris), max={max(idx) if idx else 0}")
    print(f"   declaration N: {m.decl_n}   vertex_count: {m.vertex_count} (stride {m.vertex_stride})")
    print(f"   total tris   : {tris}")
    err = bbox_error(m)
    ok = "OK (positions reproduce the header bbox exactly)" if err < 1e-4 else f"FAIL err={err}"
    print(f"   bbox check   : {ok}")
    if obj:
        write_obj(m, obj)
        print(f"   wrote {obj} ({m.vertex_count} v, {tris} f)")
    return 0 if err < 1e-4 else 1


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", type=int, required=True)
    ap.add_argument("--obj", default=None)
    a = ap.parse_args()
    p, rec, data = find_gmdl(a.pkg)
    if rec < 0:
        print("no gmdl here")
        return 1
    print(f"# {p.name} rec[{rec}] gmdl {len(data)} bytes")
    return analyse(data, Path(a.obj) if a.obj else None)


if __name__ == "__main__":
    sys.exit(main())
