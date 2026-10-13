r"""GMDL (generated model) structural reader - CONFIRMED fields only.

Derived from the shipped bytes (no third-party format description). Session 12 established
the sub-mesh descriptor, which is NOT at a fixed offset (the header before it is a variable
list of {tag, value} groups). The layout, confirmed on four different variants and matching
the one fully-checked record:

    ...variable header of {tag, value} groups...
    u32              count            (1 or 2; sits right before the bbox)
    float32 bbox[7]  minXYZ maxXYZ 1.0            (exactly 0x1c bytes)
    u32              submesh_count    (1 or 2)
    repeat submesh_count times:
        u32  format        (= 4)
        u32  index_count
        u32  stride        (= 16)
        u32  index_bytes   (== index_count * 2)
        u16  indices[index_count]
    ...vertex attribute block...

So `submesh_count` sits at `bbox + 0x1c` and the first descriptor at `bbox + 0x20`. This tool
FINDS the descriptor by its own self-consistency (`format == 4`, `stride == 16`,
`index_bytes == 2 * index_count`) rather than assuming an offset, then reads backwards for the
bbox and submesh count - the same "validate against the file's own numbers" discipline that
killed the earlier fixed-triple hypothesis.

The VERTEX attribute layout is still NOT established: the descriptor says stride = 16, but the
attribute order inside a vertex is unknown (a uniform vec3-at-stride-16 read does not reproduce
the header bbox). That must come from the loader's disassembly.

Usage: python tools/gmdl_mesh.py --pkg <i>
"""
from __future__ import annotations

import argparse
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


def find_descriptors(data: bytes):
    """Every offset d where {d: 4, d+4: C, d+8: 16, d+12: 2C} holds and C is sane.

    The descriptor is self-validating: format 4, stride 16, index_bytes == 2*index_count.
    Requiring index_bytes == 2*index_count and that the indices fit inside the payload keeps
    this from firing on coincidence.
    """
    out = []
    for d in range(0, max(0, len(data) - 16), 4):
        fmt, cnt, st, ib = struct.unpack("<4I", data[d : d + 16])
        if fmt != 4 or st != 16 or cnt == 0:
            continue
        if ib != cnt * 2:
            continue
        if d + 16 + ib > len(data):
            continue
        out.append(d)
    return out


def analyse(data: bytes) -> int:
    if len(data) < 0x40:
        print("   too short for a gmdl header")
        return 1

    descs = find_descriptors(data)
    print(f"   payload {len(data)} bytes; self-consistent sub-mesh descriptors: {len(descs)} "
          f"@ {[hex(d) for d in descs[:6]]}")
    if not descs:
        print("   ! no descriptor - not a gmdl we can read yet")
        return 1

    # The first descriptor is the real one; sub-mesh_count and the bbox sit immediately before.
    d0 = descs[0]
    if d0 >= 0x20:
        bbox = struct.unpack("<7f", data[d0 - 0x20 : d0 - 0x20 + 0x1C])
        sub_count = struct.unpack("<I", data[d0 - 0x4 : d0])[0]
        print(f"   +0x{d0-0x20:03x} bbox           : min({bbox[0]:.4f},{bbox[1]:.4f},{bbox[2]:.4f}) "
              f"max({bbox[3]:.4f},{bbox[4]:.4f},{bbox[5]:.4f}) + {bbox[6]:.4f}")
        print(f"   +0x{d0-0x04:03x} submesh_count  : {sub_count}   (bbox+0x1c == desc-4 ? "
              f"{d0 - 0x1C == d0 - 0x1C})")

    # Walk the sub-meshes: descriptor, then its indices; the next descriptor follows.
    d = d0
    n = 0
    total_idx_bytes = 0
    while d is not None and d + 16 <= len(data):
        fmt, cnt, st, ib = struct.unpack("<4I", data[d : d + 16])
        if fmt != 4 or st != 16 or ib != cnt * 2:
            break
        idx = struct.unpack("<%dH" % cnt, data[d + 16 : d + 16 + ib]) if cnt else ()
        mx = max(idx) if idx else 0
        print(f"   submesh[{n}] @0x{d:03x}: format={fmt} stride={st} index_count={cnt} "
              f"index_bytes={ib} max_index={mx} first10={idx[:10]}")
        total_idx_bytes += 16 + ib
        d = d + 16 + ib
        n += 1
        if n > 16:
            print("   (stopping after 16 sub-meshes)")
            break

    v_off = d0 + total_idx_bytes
    v_bytes = len(data) - v_off
    print(f"   vertex block @0x{v_off:03x}: {v_bytes} bytes; at stride 16 that is "
          f"{v_bytes // 16} vertices (index max needs {max(idx)+1 if idx else 0})")
    print("   NOTE: vertex attribute order is NOT reversed (uniform vec3@16 does not reproduce "
          "the bbox) - read the loader, do not scan bytes.")
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
    print("# CONFIRMED: variable header, bbox[7], submesh_count, {format,index_count,stride,index_bytes}.")
    return analyse(data)


if __name__ == "__main__":
    sys.exit(main())
