r"""Verify the gmdl header layout across MANY records, not just one.

Claim under test (from Spore_Content.package rec[12]):
  +0x6c = index count,  +0x74 = index byte size, and  +0x74 == 2 * +0x6c.
  The index array is u16 at +0x78, and every index is < the vertex count implied by the
  following float block (so all indices must be < some sane bound).

A single file agreeing could be luck; if the invariant `+0x74 == 2*+0x6c` holds across
hundreds of independent gmdl resources, the header layout is established. Reports the
histogram so a partial fit is visible too.

Usage: python tools/gmdl_header_check.py [--limit 300]
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

GMDL_TYPE = 0x00E6BCE5


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=300)
    a = ap.parse_args()

    from tools.gmdl import all_packages
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE
    from tools.refpack import decode

    checked = ok_bytes = ok_range = 0
    examples = []
    for p in all_packages():
        if checked >= a.limit:
            break
        raw = p.read_bytes()
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
        pwords = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
        shared, pi = {}, 1
        for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
            if kind & bit:
                shared[nm] = pwords[pi]
                pi += 1
        blob = raw[use + prefix : use + prefix + stride * count]
        for n in range(count):
            if checked >= a.limit:
                break
            w = struct.unpack("<%dI" % (stride // 4), blob[n * stride : (n + 1) * stride])
            tid = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
            if tid != GMDL_TYPE:
                continue
            off = w[fld["offset"]]
            packed = w[fld["size"]] & 0x7FFFFFFF
            mem = w[fld["usize"]]
            comp = w[fld["sentinel"]] & 0xFFFF
            data = raw[off : off + packed]
            if comp == 0xFFFF:
                try:
                    data = decode(data, mem)
                except Exception:
                    continue
            if len(data) < 0x80:
                continue
            checked += 1
            idx_count, vstride, idx_bytes = struct.unpack("<3I", data[0x6C:0x78])
            if idx_bytes == idx_count * 2:
                ok_bytes += 1
            if idx_bytes and 0x78 + idx_bytes <= len(data) and idx_count < 5_000_000:
                navail = min(idx_count, (len(data) - 0x78) // 2, 200000)
                idx = struct.unpack("<%dH" % navail, data[0x78 : 0x78 + 2 * navail])
                if idx and max(idx) < idx_count * 20:  # loose sanity on the value scale
                    ok_range += 1
            if len(examples) < 6 and idx_bytes == idx_count * 2:
                examples.append((p.name, n, idx_count, idx_bytes, vstride, len(data)))
    print(f"# gmdl records checked: {checked}")
    print(f"# +0x74 == 2 * +0x6c  : {ok_bytes}  ({100*ok_bytes/max(1,checked):.1f}%)")
    print(f"# index array sane     : {ok_range}  ({100*ok_range/max(1,checked):.1f}%)")
    print("# examples (pack, rec, index_count, index_bytes, +0x70, decoded_len):")
    for e in examples:
        print(f"   {e[0]:<26} rec={e[1]:<6} icount={e[2]:<8} ibytes={e[3]:<8} u70={e[4]:<4} len={e[5]}")
    verdict = ok_bytes == checked and checked > 0
    print(f"\nVERDICT: {'header layout CONFIRMED across all checked gmdl' if verdict else 'partial fit'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
