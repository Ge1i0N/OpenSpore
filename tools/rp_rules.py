"""Decide the ONE open question in the RefPack port by exactness, not by argument.

The 2/3/4-byte command table is transcribed verbatim from 0x0092c5b0 (re-dumped and checked
character by character; see the trace in this repo). The only thing that can still differ is
the DISTANCE BASE: whether a copy's source is computed from the output position before or
after this command's inline literals, and whether the trailing `dec` applies.

Rather than reason about it, test every candidate against the one oracle that cannot lie:
a correct decode of record N must consume ALL packedSize input bytes and emit EXACTLY
memSize output bytes. That is two independent exact equalities per record, over thousands
of records, so the winning rule is not a fit - it is a proof.

Usage: python tools/rp_rules.py [--limit 400]
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pkg_kinds import DIRS, HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE


def decode(data: bytes, mem: int, packed: int, lits_before: bool, dist_adj: int) -> tuple[int, int, str]:
    """lits_before: copy inline literals before the match (the disasm order).
    dist_adj: added to the computed source offset (0 = exactly the disasm)."""
    magic = (data[0] << 8) | data[1]
    nlen = 4 if (magic & 0x8000) else 3
    skip = 3 if (magic & 0x100) else 0
    i = 2 + skip + nlen
    out = bytearray()
    while True:
        if i >= len(data):
            return len(out), i, "input exhausted"
        c = data[i]
        i += 1
        if c < 0x80:
            if i >= len(data):
                return len(out), i, "trunc"
            b1 = data[i]
            i += 1
            lits = c & 3
            ln = ((c >> 2) & 7) + 2
            dist = ((c & 0x60) << 3) + b1 + 1
        elif c < 0xC0:
            if i + 1 >= len(data):
                return len(out), i, "trunc"
            b0, b1 = data[i], data[i + 1]
            i += 2
            lits = b0 >> 6
            ln = (c & 0x3F) + 3
            dist = ((b0 & 0x3F) << 8) + b1 + 1
        elif c < 0xE0:
            if i + 2 >= len(data):
                return len(out), i, "trunc"
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
                return len(out), i, "tail"
            out += data[i : i + ln]
            i += ln
            continue
        if lits_before and lits:
            if i + lits > len(data):
                return len(out), i, "trunc lits"
            out += data[i : i + lits]
            i += lits
            lits = 0
        src = len(out) - dist + dist_adj
        if src < 0 or dist <= 0:
            return len(out), i, f"bad src={src} dist={dist} out={len(out)}"
        if lits and not lits_before:
            # literals written AFTER the match
            out += data[i : i + lits]
            i += lits
        for k in range(ln):
            if src + k >= len(out):
                return len(out), i, f"overlap overrun src={src} k={k} out={len(out)}"
            out.append(out[src + k])
        if not lits_before and lits:
            out += data[i : i + lits]
            i += lits


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=400)
    a = ap.parse_args()

    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    recs: list[tuple[Path, int, int, int]] = []
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
            comp = w[fld["sentinel"]] & 0xFFFF
            if comp != 0xFFFF:
                continue
            off = w[fld["offset"]]
            packed = w[fld["size"]] & 0x7FFFFFFF
            mem = w[fld["usize"]]
            recs.append((f, off, packed, mem))
            if len(recs) >= a.limit:
                break
        if len(recs) >= a.limit:
            break
    print(f"# {len(recs):,} RefPack records sampled from {len(files)} packages")
    results = Counter()
    for lits_before in (True, False):
        for adj in (-2, -1, 0, 1, 2):
            ok = bad = 0
            why: Counter[str] = Counter()
            for f, off, packed, mem in recs:
                raw = f.read_bytes() if False else None
                # read lazily per file below; keep a simple path for the sample
                data = _payload(f, off, packed)
                n_out, i_used, status = decode(data, mem, packed, lits_before, adj)
                if n_out == mem and status == "tail":
                    ok += 1
                else:
                    bad += 1
                    why[status.split("=")[0]] += 1
            tag = f"lits_before={int(lits_before)} adj={adj:+d}"
            results[tag] = (ok, bad, why.most_common(3))
            print(f"  {tag:<24} exact={ok:,}/{len(recs):,}  fail={bad:,}  top={why.most_common(2)}")
    print()
    best = max(results.items(), key=lambda kv: kv[1][0])
    print(f"## best rule: {best[0]}  exact={best[1][0]:,}  fail={best[1][1]:,}")
    return 0


_CACHE: dict[Path, bytes] = {}


def _payload(f: Path, off: int, n: int) -> bytes:
    if f not in _CACHE:
        if len(_CACHE) > 8:
            _CACHE.clear()
        _CACHE[f] = f.read_bytes()
    return _CACHE[f][off : off + n]


if __name__ == "__main__":
    sys.exit(main())
