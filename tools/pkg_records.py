"""Derive the DBPF index record size from the files, then confirm it in the disasm.

Method: the header validator at 0x008d90ae (disassembled, see MEMORY/VERIFY) proves
+0x24 = index entry count and +0x2c = index byte size, and +0x40 = index offset with
the invariant offset+size <= filesize. So size/count is the record stride, and if that
ratio is the same integer for every package the format is fixed-width. That is a
prediction the disassembly must agree with; when it does, we know where to look.

Usage: python tools/pkg_records.py
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = [ROOT / "build" / "bin" / "Debug" / "Data", ROOT / "build" / "bin" / "Debug" / "DataEP1"]

HDR = 0x60
OFF_MAGIC, OFF_VER, OFF_COUNT, OFF_ALTIDX, OFF_ISIZE, OFF_3C, OFF_IOFF = 0x00, 0x04, 0x24, 0x28, 0x2C, 0x3C, 0x40


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def main() -> int:
    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    out(f"# {len(files)} .package files")
    out("")
    out("| file | size | ver | count | idx_size | idx_off | stride | off+size | tail |")
    out("|---|---|---|---|---|---|---|---|---|")
    strides = {}
    for f in files:
        with f.open("rb") as fh:
            h = fh.read(HDR)
        size = f.stat().st_size
        magic, ver = struct.unpack("<4sI", h[0:8])
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALTIDX : OFF_ALTIDX + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        f3c = struct.unpack("<I", h[OFF_3C : OFF_3C + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        use_off = ioff if ioff != 0 else alt
        stride = (isize / count) if count else 0.0
        strides.setdefault(round(stride, 3), []).append(f.name)
        tail = size - (use_off + isize)
        out(
            f"| `{f.name}` | {size:,} | {ver} | {count} | {isize:,} | {use_off:,} | {stride:.3f} | "
            f"{use_off+isize:,} | {tail:+,} |"
        )
        if magic != b"DBPF":
            out(f"| **BAD MAGIC {magic!r} in {f.name}** |")
    out("")
    out("## Stride histogram")
    for k, v in sorted(strides.items()):
        out(f"- stride {k}: {len(v)} files" + (f"  e.g. {v[0]}" if v else ""))
    out("")
    # For the most common integer stride, dump the first few records of one file
    ints = [k for k in strides if float(k).is_integer()]
    if ints:
        best = max(ints, key=lambda k: len(strides[k]))
        name = strides[best][0]
        path = next(p for p in files if p.name == name)
        with path.open("rb") as fh:
            fh.seek(0)
            h = fh.read(HDR)
            count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
            ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
            alt = struct.unpack("<I", h[OFF_ALTIDX : OFF_ALTIDX + 4])[0]
            use_off = ioff if ioff else alt
            fh.seek(use_off)
            n = min(int(best) * 8, 4096)
            blob = fh.read(n)
        out(f"## First {len(blob)//int(best)} records of `{name}` (stride {int(best)}, index at 0x{use_off:x})")
        out("")
        out("```")
        rs = int(best)
        for i in range(0, len(blob) - rs + 1, rs):
            rec = blob[i : i + rs]
            words = struct.unpack("<%dI" % (rs // 4), rec)
            out(f"  #{i//rs:<3} " + " ".join(f"{w:10d}" for w in words) + "   | " + " ".join(f"0x{w:08x}" for w in words))
        out("```")
    return 0


if __name__ == "__main__":
    sys.exit(main())
