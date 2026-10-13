"""Brute-force the RefPack 2-byte form on the SMALLEST real streams.

Every candidate rule must satisfy two exact equalities per stream (consume all input,
emit exactly memSize), so on a 12-byte stream the space of consistent rules is tiny and
can be enumerated. This is the disciplined way to find which instruction of 0x0092c5b0 I
transcribed wrong, instead of arguing about the 240-byte ones.

Usage: python tools/rp_brute.py
"""
from __future__ import annotations

import itertools
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pkg_kinds import DIRS, HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE

# A rule is: for the 2-byte form (c < 0x80, one following byte b1):
#   lits_bits: how many low bits of c are the inline literal count (0..3)
#   len_shift, len_mask, len_base: len = ((c >> len_shift) & len_mask) + len_base
#   dist_hi_mask: which bits of c are the distance high part (contiguous, above len)
# The 3-byte and 4-byte forms are NOT varied: they are unambiguous in the disasm and the
# streams that fail do so in the 2-byte region.
def make_rule(lits_bits, len_shift, len_mask, len_base, hi_shift, hi_bits):
    def two(c, b1):
        lits = (c & ((1 << lits_bits) - 1)) if lits_bits else 0
        ln = ((c >> len_shift) & len_mask) + len_base
        hi = ((c >> hi_shift) & ((1 << hi_bits) - 1)) if hi_bits else 0
        dist = (hi << 8) + b1 + 1
        return lits, ln, dist

    return two


def decode(data: bytes, mem: int, two) -> tuple[int, str]:
    magic = (data[0] << 8) | data[1]
    nlen = 4 if (magic & 0x8000) else 3
    skip = 3 if (magic & 0x100) else 0
    i = 2 + skip + nlen
    out = bytearray()
    guard = 0
    while True:
        guard += 1
        if guard > 100000:
            return len(out), "cap"
        if i >= len(data):
            return len(out), "input exhausted"
        c = data[i]
        i += 1
        if c < 0x80:
            if i >= len(data):
                return len(out), "trunc2"
            b1 = data[i]
            i += 1
            lits, ln, dist = two(c, b1)
        elif c < 0xC0:
            if i + 1 >= len(data):
                return len(out), "trunc3"
            b0, b1 = data[i], data[i + 1]
            i += 2
            lits = b0 >> 6
            ln = (c & 0x3F) + 3
            dist = ((b0 & 0x3F) << 8) + b1 + 1
        elif c < 0xE0:
            if i + 2 >= len(data):
                return len(out), "trunc4"
            b0, b1, b2 = data[i], data[i + 1], data[i + 2]
            i += 3
            lits = c & 3
            ln = ((c >> 2) & 3) * 64 + b2 + 4
            dist = ((c & 0x10) << 12) + (b0 << 8) + b1 + 1
        else:
            ln = (c & 0x1F) * 4 + 4
            if ln > 0x70:
                lits = c & 3
                out += data[i : i + lits]
                i += lits
                return len(out), ("tail_exact" if len(out) == mem and i == len(data) else "tail")
            out += data[i : i + ln]
            i += ln
            continue
        if lits:
            if i + lits > len(data):
                return len(out), "trunc_lits"
            out += data[i : i + lits]
            i += lits
        src = len(out) - dist
        if src < 0 or dist <= 0:
            return len(out), "bad"
        for k in range(ln):
            out.append(out[src + k])


def main() -> int:
    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    small: list[tuple[Path, int, int, int]] = []
    for f in files:
        raw = f.read_bytes()
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
        blob = raw[use + prefix : use + prefix + stride * count]
        for n in range(count):
            w = struct.unpack("<%dI" % (stride // 4), blob[n * stride : (n + 1) * stride])
            if (w[fld["sentinel"]] & 0xFFFF) != 0xFFFF:
                continue
            packed = w[fld["size"]] & 0x7FFFFFFF
            if packed <= 40:
                small.append((f, w[fld["offset"]], packed, w[fld["usize"]]))
    small.sort(key=lambda r: r[2])
    print(f"# {len(small):,} RefPack records with packedSize <= 40; smallest 12 used as the probe set")
    probe = small[:12]
    for f, off, packed, mem in probe:
        print(f"   {f.name:<26} packed={packed:<3} mem={mem:<5} bytes={off}")

    # enumerate candidate 2-byte rules
    cands = []
    for lits_bits in (0, 1, 2, 3):
        for len_shift in range(0, 5):
            for len_mask in (1, 3, 7, 15, 31):
                for len_base in (0, 1, 2, 3, 4):
                    for hi_shift in range(0, 6):
                        for hi_bits in (0, 1, 2, 3):
                            if lits_bits + (len_mask.bit_length()) + hi_bits > 8:
                                pass  # allow overlap; the exactness test filters anyway
                            cands.append((lits_bits, len_shift, len_mask, len_base, hi_shift, hi_bits))
    print(f"\n# enumerating {len(cands):,} candidate 2-byte rules against {len(probe)} probe streams")
    winners = Counter()
    for spec in cands:
        two = make_rule(*spec)
        good = 0
        for f, off, packed, mem in probe:
            raw = f.read_bytes()
            d = raw[off : off + packed]
            n, status = decode(d, mem, two)
            if status == "tail_exact":
                good += 1
        if good:
            winners[spec] = good
    print(f"# rules that decode at least one probe stream exactly: {len(winners)}")
    for spec, g in winners.most_common(12):
        print(f"   lits_bits={spec[0]} len=((c>>{spec[1]})&{spec[2]})+{spec[3]} hi=(c>>{spec[4]})&{(1<<spec[5])-1 if spec[5] else 0} ({spec[5]} bits)  -> {g}/{len(probe)} streams")
    return 0


if __name__ == "__main__":
    sys.exit(main())
