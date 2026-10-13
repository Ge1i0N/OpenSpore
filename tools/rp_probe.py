"""Which 2-byte-form bit layout actually decodes the shipped RefPack streams?

The disasm at 0x0092c5b0 is unambiguous on dispatch, but ONE field width in the 2-byte form
is the difference between a working decoder and a fault. This script decodes the same real
payload under several candidate layouts and reports, per candidate, how far it gets and
whether the final length equals the index's memSize. The winner is then re-read against the
instructions line by line - the point is to find WHICH instruction I transcribed wrong, not
to pick a formula by majority.

Usage: python tools/rp_probe.py --pkg <package> --rec <n>
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

# candidate: (lits_expr, len_expr, dist_expr) given control byte c and next byte b1
CANDIDATES = {
    "A mine (lits=c&3, len=((c>>2)&7)+2, d=((c>>5)&3)*256+b1+1)": lambda c, b1: (c & 3, ((c >> 2) & 7) + 2, ((c >> 5) & 3) * 256 + b1 + 1),
    "B canonical (no lits, len=((c>>3)&0x1F)+2, d=(c&7)*256+b1+1)": lambda c, b1: (0, ((c >> 3) & 0x1F) + 2, (c & 7) * 256 + b1 + 1),
    "C lits=c&3, len=((c>>2)&0x1F)+2, d=(c&7)*256+b1+1": lambda c, b1: (c & 3, ((c >> 2) & 0x1F) + 2, (c & 7) * 256 + b1 + 1),
    "D lits=0, len=(c&7)+2, d=((c>>3)&0x1F)*256+b1+1": lambda c, b1: (0, (c & 7) + 2, ((c >> 3) & 0x1F) * 256 + b1 + 1),
    "E lits=c&3, len=(c>>2)+2, d=b1+1+((c>>2)&0)*256": lambda c, b1: (c & 3, (c >> 2) + 2, b1 + 1),
}


def decode(data: bytes, expr, mem: int, max_cmds: int = 100000) -> tuple[int, int, str]:
    """Return (out_len, cmds, status)."""
    nlen = 4 if ((data[0] << 8 | data[1]) & 0x8000) else 3
    skip = 3 if ((data[0] << 8 | data[1]) & 0x100) else 0
    i = 2 + skip + nlen
    out = bytearray()
    cmds = 0
    while cmds < max_cmds:
        if i >= len(data):
            return len(out), cmds, "input exhausted"
        c = data[i]
        i += 1
        cmds += 1
        if c < 0x80:
            if i >= len(data):
                return len(out), cmds, "trunc 2B"
            b1 = data[i]
            i += 1
            lits, ln, dist = expr(c, b1)
            if lits:
                out += data[i : i + lits]
                i += lits
            if dist == 0 or dist > len(out):
                return len(out), cmds, f"BAD dist={dist} out={len(out)} cmd={cmds}"
            s = len(out) - dist
            for k in range(ln):
                out.append(out[s + k])
        elif c < 0xC0:
            if i + 1 >= len(data):
                return len(out), cmds, "trunc 3B"
            b0, b1 = data[i], data[i + 1]
            i += 2
            lits = b0 >> 6
            ln = (c & 0x3F) + 3
            dist = ((b0 & 0x3F) << 8) + b1 + 1
            if lits:
                out += data[i : i + lits]
                i += lits
            if dist == 0 or dist > len(out):
                return len(out), cmds, f"BAD dist3={dist} out={len(out)} cmd={cmds}"
            s = len(out) - dist
            for k in range(ln):
                out.append(out[s + k])
        elif c < 0xE0:
            if i + 2 >= len(data):
                return len(out), cmds, "trunc 4B"
            b0, b1, b2 = data[i], data[i + 1], data[i + 2]
            i += 3
            lits = c & 3
            ln = ((c >> 2) & 3) * 64 + b2 + 4
            dist = ((c & 0x10) << 12) + (b0 << 8) + b1 + 1
            if lits:
                out += data[i : i + lits]
                i += lits
            if dist == 0 or dist > len(out):
                return len(out), cmds, f"BAD dist4={dist} out={len(out)} cmd={cmds}"
            s = len(out) - dist
            for k in range(ln):
                out.append(out[s + k])
        else:
            ln = (c & 0x1F) * 4 + 4
            if ln > 0x70:
                lits = c & 3
                out += data[i : i + lits]
                i += lits
                return len(out), cmds, "tail"
            out += data[i : i + ln]
            i += ln
        if mem and len(out) >= mem:
            return len(out), cmds, ("exact mem" if len(out) == mem else f"OVERSHOOT {len(out)}>{mem}")
    return len(out), cmds, "cmd cap"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pkg", required=True)
    ap.add_argument("--rec", type=int, default=0)
    a = ap.parse_args()
    from tools.pkg_kinds import HDR, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE, KINDS

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
    payload = raw[off : off + packed]
    print(f"# {p.name} rec[{a.rec}] kind={kind} off={off} packed={packed} mem={mem}")
    print(f"# first 24 bytes: {payload[:24].hex(' ')}")
    print()
    for name, expr in CANDIDATES.items():
        n, cmds, status = decode(payload, expr, mem)
        ok = "  <== CLEAN" if status == "exact mem" else ""
        print(f"  {name:<62} out={n:<7} cmds={cmds:<6} {status}{ok}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
