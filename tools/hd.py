"""Hexdump the head of a real game file - authority #2, before writing any parser.

Field guide rule: "a parser's expected shape is visible in 10 lines of its input."
This tool exists so the first move on `.package` is reading bytes, not recalling a wiki.

Usage:
    python tools/hd.py build/bin/Debug/Data/Spore_Game.package
    python tools/hd.py <file> --len 512 --at 0
    python tools/hd.py <file> --strings          # also decode printable runs
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


def hexdump(data: bytes, base: int = 0, width: int = 16) -> str:
    out = []
    for i in range(0, len(data), width):
        row = data[i : i + width]
        hexpart = " ".join(f"{b:02x}" for b in row)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        out.append(f"{base+i:012x}  {hexpart:<{width*3}}  |{text}|")
    return "\n".join(out)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--at", type=lambda x: int(x, 0), default=0)
    ap.add_argument("--len", type=lambda x: int(x, 0), default=256)
    ap.add_argument("--strings", action="store_true")
    ap.add_argument("--tail", action="store_true", help="dump the last N bytes instead")
    args = ap.parse_args()

    p = Path(args.file)
    size = p.stat().st_size
    with p.open("rb") as f:
        if args.tail:
            off = max(0, size - args.len)
            f.seek(off)
        else:
            off = args.at
            f.seek(off)
        data = f.read(args.len)

    print(f"# {p}  size={size:,} bytes ({size:#x})")
    print(f"# window 0x{off:x}..0x{off+len(data):x}")
    print(hexdump(data, off))

    if args.strings:
        print("\n# printable runs (>=4):")
        for m in re.finditer(rb"[\x20-\x7e]{4,}", data):
            print(f"  +0x{off+m.start():06x}  {m.group().decode('latin-1')!r}")
        # UTF-16LE too: Spore is a wide-string codebase
        for align in (0, 1):
            i = align
            end = len(data) - 1
            while i < end:
                u = data[i] | (data[i + 1] << 8)
                if not 32 <= u <= 126:
                    i += 2
                    continue
                start, chars = i, []
                while i < end:
                    u = data[i] | (data[i + 1] << 8)
                    if not 32 <= u <= 126:
                        break
                    chars.append(chr(u))
                    i += 2
                if len(chars) >= 4:
                    print(f"  +0x{off+start:06x}  (utf16) {''.join(chars)!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
