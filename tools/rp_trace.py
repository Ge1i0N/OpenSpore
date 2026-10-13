"""Trace the RefPack command loop command-by-command, for debugging the table.

Prints each decoded command with its control byte, the bytes consumed, the output length
before/after, and the distance/length fields, so a wrong formula is visible immediately.

Usage:
    python tools/rp_trace.py <payload-file> [--steps 20]
    python tools/rp_trace.py --pkg <package> --rec <n> [--steps 20]
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))


def trace(data: bytes, steps: int, expect: int | None = None) -> None:
    magic = (data[0] << 8) | data[1]
    print(f"magic BE16 = 0x{magic:04x}   (&0x1FFF)==0x10FB -> {(magic & 0x1FFF) == 0x10FB}")
    print(f"bit15(0x8000)={magic & 0x8000:#x}  bit8(0x100)={magic & 0x100:#x}")
    nlen = 4 if (magic & 0x8000) else 3
    skip = 3 if (magic & 0x100) else 0
    print(f"-> length width {nlen}, extra skip {skip}")
    i = 2 + skip
    size = int.from_bytes(data[i : i + nlen], "big")
    i += nlen
    print(f"size = {size}" + (f"  (expected {expect})" if expect else ""))
    out = bytearray()
    for k in range(steps):
        if i >= len(data):
            print(f"[{k:3d}] INPUT EXHAUSTED at in={i}, out={len(out)}")
            return
        c = data[i]
        i += 1
        if c < 0x80:
            b1 = data[i]
            i += 1
            lits = c & 3
            ln = ((c >> 2) & 7) + 2
            dist = ((c >> 5) & 3) * 256 + b1 + 1
            kind = "2B copy"
        elif c < 0xC0:
            b0, b1 = data[i], data[i + 1]
            i += 2
            lits = b0 >> 6
            ln = (c & 0x3F) + 3
            dist = ((b0 & 0x3F) << 8) + b1 + 1
            kind = "3B copy"
        elif c < 0xE0:
            b0, b1, b2 = data[i], data[i + 1], data[i + 2]
            i += 3
            lits = c & 3
            ln = ((c >> 2) & 3) * 64 + b2 + 4
            dist = ((c & 0x10) << 12) + (b0 << 8) + b1 + 1
            kind = "4B copy"
        else:
            ln = (c & 0x1F) * 4 + 4
            if ln > 0x70:
                lits = c & 3
                dist = 0
                kind = "TAIL"
            else:
                lits = ln
                ln = 0
                dist = 0
                kind = "LITERAL"
        pre = len(out)
        if kind == "LITERAL":
            out += data[i : i + lits]
            i += lits
            print(f"[{k:3d}] {kind:8s} c={c:02x} lits={lits:4d}            out {pre}->{len(out)}")
            continue
        if lits:
            out += data[i : i + lits]
            i += lits
        if kind == "TAIL":
            print(f"[{k:3d}] {kind:8s} c={c:02x} lits={lits:4d}            out {pre}->{len(out)}  END")
            return
        bad = dist == 0 or dist > len(out)
        src = len(out) - dist
        if not bad:
            for j in range(ln):
                out.append(out[src + j])
        print(f"[{k:3d}] {kind:8s} c={c:02x} lits={lits:4d} ln={ln:4d} dist={dist:6d} "
              f"out {pre}->{len(out)}{'   <== BAD DISTANCE' if bad else ''}")
        if bad:
            print(f"       available output = {len(out)} bytes; src would be {src}")
            return
    print(f"... stopped after {steps} commands; out={len(out)} of {size}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file", nargs="?")
    ap.add_argument("--pkg")
    ap.add_argument("--rec", type=int, default=0)
    ap.add_argument("--steps", type=int, default=20)
    ap.add_argument("--expect", type=int)
    a = ap.parse_args()
    if a.pkg:
        from tools.pkg_kinds import DIRS, HDR, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE, KINDS

        p = Path(a.pkg)
        raw = p.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        prefix, stride, fld = KINDS[kind]
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + a.rec * stride :][:stride])
        off, packed, mem = w[fld["offset"]], w[fld["size"]] & 0x7FFFFFFF, w[fld["usize"]]
        comp = w[fld["sentinel"]] & 0xFFFF
        print(f"# {p.name} rec[{a.rec}]: off={off} packed={packed} mem={mem} comp=0x{comp:04x}")
        trace(raw[off : off + packed], a.steps, mem)
        return 0
    if not a.file:
        ap.print_help()
        return 2
    d = Path(a.file).read_bytes()
    trace(d, a.steps, a.expect)
    return 0


if __name__ == "__main__":
    sys.exit(main())
