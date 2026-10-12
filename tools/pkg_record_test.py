"""Test the 24-byte record hypothesis against every fixed-stride package.

Hypothesis (from UI/NameGen hexdumps, to be CONFIRMED by the record-parser disasm):
  index = 12-byte prefix + count * 24-byte record
  record = u32[6]: [0]=? [1]=? [2]=file_offset [3]=size|0x80000000 [4]=? [5]=?
Checks, across all records of all files where idx_size == 12 + 24*count:
  * [2] + ([3] & 0x7fffffff) <= index_offset  (a resource never overlaps the index)
  * [2] < index_offset
  * distribution of [5]  (looked like a constant 0x1ffff in the two dumps)
  * whether [4] >= masked [3] always (i.e. [4] = uncompressed size)
This is authority #2 used to AIM the disassembler, not to replace it: whatever fails here
tells us which field guess is wrong.

Usage: python tools/pkg_record_test.py
"""
from __future__ import annotations

import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = [ROOT / "build" / "bin" / "Debug" / "Data", ROOT / "build" / "bin" / "Debug" / "DataEP1"]
HDR = 0x60
OFF_COUNT, OFF_ISIZE, OFF_IOFF, OFF_ALT = 0x24, 0x2C, 0x40, 0x28


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def main() -> int:
    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    fixed = []
    for f in files:
        h = f.read_bytes()[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        if isize == 12 + 24 * count and count:
            fixed.append((f, count, use))
    out(f"# {len(fixed)} files match idx_size == 12 + 24*count")
    total = bad_off = bad_tail = 0
    f5 = Counter()
    f0 = Counter()
    cmp43 = Counter()
    sizes = Counter()
    for f, count, ioff in fixed:
        with f.open("rb") as fh:
            fh.seek(ioff + 12)
            blob = fh.read(24 * count)
        for i in range(count):
            rec = blob[i * 24 : (i + 1) * 24]
            w = struct.unpack("<6I", rec)
            total += 1
            off, szraw = w[2], w[3]
            sz = szraw & 0x7FFFFFFF
            f5[w[5]] += 1
            f0[w[0]] += 1
            sizes[szraw >> 31] += 1
            cmp43["[4]>=[3]" if w[4] >= sz else "[4]<[3]"] += 1
            if off >= ioff:
                bad_off += 1
            if off + sz > ioff:
                bad_tail += 1
    out(f"records examined: {total:,}")
    out(f"[2] offset >= index_offset : {bad_off}  ({'OK' if bad_off == 0 else 'VIOLATION'})")
    out(f"[2]+[3] beyond index_offset: {bad_tail}  ({'OK' if bad_tail == 0 else 'VIOLATION'})")
    out(f"[3] high bit set: {dict(sizes)}")
    out(f"[4] vs masked [3]: {dict(cmp43)}")
    out(f"[5] distinct values: {len(f5)}; top: {f5.most_common(6)}")
    out(f"[0] distinct values: {len(f0)}; top: {f0.most_common(6)}")
    out("")
    out("## Interpretation")
    out("- if bad_off/bad_tail are 0, [2]=offset and [3]=size are confirmed by every record.")
    out("- a single dominant [5] value means it is a constant/sentinel, not data.")
    out("- [4] >= [3] everywhere supports [4]=uncompressed size, [3]=stored size.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
