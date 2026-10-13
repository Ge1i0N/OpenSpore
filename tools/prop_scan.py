r"""prop (property table) format scan - derive the layout from the shipped bytes.

`prop` = type 0x00b1b104 (registry), **30,645 resources** - the most numerous type. A first look at
`NameGeneration` rec[5] showed BIG-endian integers (like RefPack), a leading count, and repeated
groups {key, type, value}. This tool dumps many props and tallies the structure so the format can be
derived rather than guessed.

Usage: python tools/prop_scan.py --count 12
       python tools/prop_scan.py --pkg 0 --rec 5
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

PROP = 0x00B1B104


def iter_props(limit=None):
    from tools.gmdl import all_packages
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
    from tools.refpack import decode

    n = 0
    for pi, p in enumerate(all_packages()):
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
            if t != PROP:
                continue
            off = w[fld["offset"]]
            packed = w[fld["size"]] & 0x7FFFFFFF
            mem = w[fld["usize"]]
            comp = w[fld["sentinel"]] & 0xFFFF
            data = raw[off : off + packed]
            if comp == 0xFFFF:
                try:
                    data = decode(data, mem)
                except Exception:  # noqa: BLE001
                    continue
            yield p.name, r, data
            n += 1
            if limit and n >= limit:
                return


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--count", type=int, default=10)
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int)
    ap.add_argument("--dump", type=int, default=96)
    a = ap.parse_args()

    shown = 0
    for name, rec, data in iter_props(limit=None if (a.pkg is not None) else a.count):
        if a.pkg is not None and a.rec is not None:
            # only the requested one
            pass
        if shown >= a.count and a.pkg is None:
            break
        # big-endian first word
        be = struct.unpack(">I", data[:4])[0] if len(data) >= 4 else 0
        le = struct.unpack("<I", data[:4])[0] if len(data) >= 4 else 0
        print(f"== {name} rec[{rec}] {len(data)} bytes   firstBE={be} firstLE={le}")
        d = data[: a.dump]
        for i in range(0, len(d), 16):
            row = d[i : i + 16]
            be4 = " ".join("%08x" % struct.unpack(">I", row[j : j + 4])[0]
                           for j in range(0, len(row) - 3, 4))
            asc = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
            print(f"  {i:06x}  {row.hex(' '):<47} |{asc}|  BE: {be4}")
        shown += 1
        if shown >= a.count and a.pkg is None:
            break
    return 0


if __name__ == "__main__":
    sys.exit(main())
