r"""Dump the first N records of a given resource TYPE across the packages.

The types are numeric ids; the names come from upstream/SporeMaster3/data/reg_type.txt (a level-4
reference for NAMES only - the ids themselves are what the DBPF records carry). This is the
"look at the real bytes of a format" tool used before writing any parser.

Usage: python tools/first_of_type.py raster            # decode + hexdump the first few
       python tools/first_of_type.py prop --count 3
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def type_ids():
    p = ROOT / "upstream" / "SporeMaster3" / "data" / "reg_type.txt"
    out = {}
    if not p.exists():
        return out
    for line in p.read_text(encoding="utf-8", errors="replace").splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1].lower().startswith("0x"):
            try:
                out[parts[0]] = int(parts[1], 16)
            except ValueError:
                pass
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("type")
    ap.add_argument("--count", type=int, default=2)
    ap.add_argument("--dump", type=int, default=128)
    a = ap.parse_args()
    ids = type_ids()
    if a.type not in ids:
        print(f"unknown type name {a.type!r}; known: {sorted(ids)[:20]} ...")
        return 1
    tid = ids[a.type]
    print(f"# type {a.type} = 0x{tid:08x}")

    from tools.gmdl import all_packages
    from tools.gmdl import record_payload
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF

    shown = 0
    for pi, p in enumerate(all_packages()):
        raw = p.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        prefix, stride, fld = KINDS[kind]
        pw = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
        shared, q = {}, 1
        for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
            if kind & bit:
                shared[nm] = pw[q]
                q += 1
        for n in range(count):
            w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + n * stride :][:stride])
            t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
            if t != tid:
                continue
            try:
                _, data = record_payload(p, n)
            except Exception as e:  # noqa: BLE001
                continue
            print(f"\n== {p.name} rec[{n}]  decoded {len(data)} bytes ==")
            d = data[: a.dump]
            for i in range(0, len(d), 16):
                row = d[i : i + 16]
                asc = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
                print(f"  {i:06x}  {row.hex(' '):<47} |{asc}|")
            shown += 1
            if shown >= a.count:
                return 0
    print(f"\n(no records of type {a.type} found)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
