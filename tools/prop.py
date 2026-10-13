r"""prop (property table) reader + validator.

Format derived from the shipped bytes (BIG-endian integers, like RefPack):

    u32 BE  entry_count
    repeat entry_count times:
        u32 BE  key            (a hashed property name)
        u32 BE  type           (a type id; 0x0020009c is common)
        u32 BE  item_count
        u32 BE  item_size      (bytes per item)
        u8[item_count * item_size]  data

The test that this is right is EXACTNESS: the walk must consume the whole payload with no leftover
and no overrun, across many resources. That is the same discipline that solved gmdl (its positions
had to reproduce the header bbox exactly).

Usage: python tools/prop.py --verify --limit 3000
       python tools/prop.py --pkg 0 --rec 5
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


def parse(data: bytes):
    """Return (entries, consumed, why). entries = [(key, type, count, size, off_data)]."""
    if len(data) < 4:
        return [], 0, "short"
    count = struct.unpack(">I", data[:4])[0]
    if count == 0 or count > 100000:
        return [], 0, f"implausible count {count}"
    entries = []
    off = 4
    for _ in range(count):
        if off + 16 > len(data):
            return entries, off, "entry header past end"
        key, typ, icnt, isize = struct.unpack(">4I", data[off : off + 16])
        off += 16
        total = icnt * isize
        if off + total > len(data):
            return entries, off, "item data past end"
        entries.append((key, typ, icnt, isize, off))
        off += total
    return entries, off, None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--verify", action="store_true")
    ap.add_argument("--limit", type=int, default=3000)
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int)
    ap.add_argument("--dump", type=int, default=8)
    a = ap.parse_args()

    from tools.prop_scan import iter_props

    if a.pkg is not None and a.rec is not None and not a.verify:
        for name, rec, data in iter_props():
            if rec == a.rec:
                entries, consumed, why = parse(data)
                print(f"# {name} rec[{rec}] {len(data)} bytes -> {len(entries)} entries, consumed {consumed}, why={why}")
                for k, t, ic, isz, o in entries[: a.dump]:
                    print(f"  key=0x{k:08x} type=0x{t:08x} items={ic} size={isz}")
                return 0
        print("not found")
        return 1

    ok = 0
    fail = 0
    types = Counter()
    sizes = Counter()
    leftovers = []
    n = 0
    for name, rec, data in iter_props(limit=a.limit):
        n += 1
        entries, consumed, why = parse(data)
        if why is None and consumed == len(data):
            ok += 1
            for _k, t, ic, isz, _o in entries:
                types[t] += 1
                sizes[isz] += 1
        else:
            fail += 1
            if len(leftovers) < 8:
                leftovers.append((name, rec, len(data), consumed, why))
    print(f"# prop resources scanned : {n}")
    print(f"# parse consumed EXACTLY  : {ok}")
    print(f"# mismatches              : {fail}")
    for lo in leftovers:
        print(f"   {lo[0]} rec[{lo[1]}] {lo[2]}B consumed {lo[3]} ({lo[4]})")
    print("\n# top type ids")
    for t, c in types.most_common(12):
        print(f"   0x{t:08x}  x{c}")
    print("\n# top item sizes")
    for s, c in sizes.most_common(12):
        print(f"   {s:>4}  x{c}")
    return 0 if fail == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
