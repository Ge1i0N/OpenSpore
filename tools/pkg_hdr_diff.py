"""Diff full 0x60 headers of a fixed-stride vs a "var-width" package.

Both families fit an exact formula (12 + 24*count vs 16 + 20*count), so "var-width" is
probably a SECOND FORMAT VARIANT, not variable records. If so, the header must carry a
discriminator. This dumps every non-zero dword of both, side by side, to find it.

Usage: python tools/pkg_hdr_diff.py <fileA> <fileB>
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def hdr(p: str) -> bytes:
    with open(p, "rb") as f:
        return f.read(0x60)


def main() -> int:
    if len(sys.argv) < 3:
        out("usage: pkg_hdr_diff.py <fileA> <fileB>")
        return 2
    a, b = hdr(sys.argv[1]), hdr(sys.argv[2])
    out(f"A = {sys.argv[1]}")
    out(f"B = {sys.argv[2]}")
    out("")
    out("| off | A u32 | B u32 | A bytes | B bytes | same? |")
    out("|---|---|---|---|---|---|")
    for i in range(0, 0x60, 4):
        va = struct.unpack("<I", a[i : i + 4])[0]
        vb = struct.unpack("<I", b[i : i + 4])[0]
        if va == 0 and vb == 0:
            continue
        out(
            f"| 0x{i:02x} | {va} (0x{va:08x}) | {vb} (0x{vb:08x}) | {a[i:i+4].hex(' ')} | {b[i:i+4].hex(' ')} | "
            f"{'=' if va == vb else '**DIFF**'} |"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
