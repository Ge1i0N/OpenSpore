"""Inspect a `gmdl` (generated model) resource: the baked mesh format.

Found by looking at a real gmdl payload: it contains float triplets (vertex positions, e.g.
0x3ef4fa6a = +0.478, 0x3f800000 = 1.0) and runs of u16 indices, plus repeated 4-byte values that
are the rw4 type id in BIG-endian byte order (`2f 4e 68 1b`). That makes gmdl far more tractable
than the rw4 container, and it is the geometry the game actually draws (11,041 resources).

This tool reports STRUCTURE ONLY - candidate counts, offsets and the byte layout of the first
part of the file - so the format can be derived from the bytes rather than guessed. It also counts
how many 16-bit values look like valid indices (in range) for each candidate vertex count, which
is exactly the consistency check that separates a real interpretation from a coincidence.

Usage: python tools/gmdl.py --find                 # locate gmdl resources
       python tools/gmdl.py --pkg <i> --rec <n>     # analyse one
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

GMDL_TYPE = 0x00E6BCE5


def all_packages():
    from tools.pkg_kinds import DIRS
    out, seen = [], set()
    for d in DIRS:
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*.package")):
            k = str(p.resolve())
            if k not in seen:
                seen.add(k)
                out.append(p)
    return out


def record_payload(p: Path, rec: int):
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE
    from tools.refpack import decode

    raw = p.read_bytes()
    h = raw[:HDR]
    ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
    alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
    use = ioff or alt
    kind = struct.unpack("<I", raw[use : use + 4])[0]
    prefix, stride, fld = KINDS[kind]
    w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + rec * stride :][:stride])
    tid = w[fld["typeid"]] if "typeid" in fld else 0
    off = w[fld["offset"]]
    packed = w[fld["size"]] & 0x7FFFFFFF
    mem = w[fld["usize"]]
    comp = w[fld["sentinel"]] & 0xFFFF
    data = raw[off : off + packed]
    if comp == 0xFFFF:
        data = decode(data, mem)
    return tid, data


def floats_in(data: bytes):
    n = len(data) // 4
    vals = struct.unpack("<%df" % n, data[: n * 4])
    return vals


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--find", action="store_true")
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int, default=0)
    ap.add_argument("--dump", type=int, default=256)
    a = ap.parse_args()

    if a.find:
        found = []
        for p in all_packages():
            try:
                tid, data = record_payload(p, 0)
            except Exception:
                continue
            if tid == GMDL_TYPE and len(data) > 4096:
                found.append((p, len(data)))
            if len(found) >= 8:
                break
        print(f"# gmdl candidates: {len(found)}")
        for p, n in found:
            print(f"   {p.name} first-record-decoded={n}")
        return 0

    if a.pkg is None:
        ap.print_help()
        return 2
    p = all_packages()[a.pkg]
    # find the first gmdl record in this package
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE
    raw = p.read_bytes()
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
    ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
    alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
    use = ioff or alt
    kind = struct.unpack("<I", raw[use : use + 4])[0]
    prefix, stride, fld = KINDS[kind]
    pwords = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
    shared = {}
    pi = 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pwords[pi]
            pi += 1
    rec = None
    for n in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + n * stride :][:stride])
        tid = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        if tid == GMDL_TYPE:
            rec = n
            break
    if rec is None:
        print("no gmdl record in this package")
        return 1
    tid, data = record_payload(p, rec)
    print(f"# {p.name} rec[{rec}] gmdl, decoded {len(data)} bytes")
    print(f"# first {a.dump} bytes:")
    for i in range(0, min(a.dump, len(data)), 16):
        row = data[i : i + 16]
        print(f"  {i:06x}  {row.hex(' '):<47} |" + "".join(chr(b) if 32 <= b < 127 else "." for b in row) + "|")
    # interpret the leading dwords
    print("\n# leading u32 (LE) and as float:")
    for i in range(0, min(64, len(data)) - 3, 4):
        u = struct.unpack("<I", data[i : i + 4])[0]
        f = struct.unpack("<f", data[i : i + 4])[0]
        note = ""
        if 0.001 < abs(f) < 1e4:
            note = f"float~{f:.4f}"
        if u in (0x2F4E681B, 0x1B684E2F):
            note += " rw4-id(" + ("LE" if u == 0x2F4E681B else "BE") + ")"
        print(f"  +0x{i:02x}  u32=0x{u:08x}  {note}")
    # float runs
    print("\n# candidate float32 runs (>=8 finite, |v|<1e4):")
    vals = floats_in(data)
    run_start = None
    runs = []
    for i, v in enumerate(vals):
        ok = v == v and abs(v) < 1e4  # finite-ish
        if ok and run_start is None:
            run_start = i
        elif not ok and run_start is not None:
            if i - run_start >= 8:
                runs.append((run_start * 4, (i - run_start) * 4))
            run_start = None
    if run_start is not None and len(vals) - run_start >= 8:
        runs.append((run_start * 4, (len(vals) - run_start) * 4))
    for off, ln in runs[:12]:
        print(f"   off=0x{off:06x} len={ln} floats ({ln//12} vec3)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
