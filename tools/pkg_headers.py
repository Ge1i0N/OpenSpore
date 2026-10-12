"""Differential header analysis across every real .package we have.

Method (authority #2, the shipped data files): a field that is identical across files of
different sizes and contents is a format constant; a field that varies is a count, an
offset, or a size. Guessing one header from one file is how wiki-driven parsers are born;
comparing 12 headers is evidence.

Usage:
    python tools/pkg_headers.py
    python pkg_headers.py --width 96 --diff-only
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SEARCH = [
    ROOT / "build" / "bin" / "Debug" / "Data",
    ROOT / "build" / "bin" / "Debug" / "DataEP1",
]


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--width", type=int, default=96, help="header bytes to compare (default 0x60)")
    ap.add_argument("--diff-only", action="store_true", help="print only the offsets that vary")
    args = ap.parse_args()

    files: list[Path] = []
    for d in SEARCH:
        if d.is_dir():
            files += sorted(d.rglob("*.package"))
    if not files:
        out("! no .package files found under " + ", ".join(str(d) for d in SEARCH))
        return 1

    out(f"# {len(files)} .package files")
    for f in files:
        out(f"  {f.stat().st_size:>12,}  {f.relative_to(ROOT.parent.parent.parent)}")
    out("")

    heads: list[tuple[Path, int, bytes]] = []
    for f in files:
        with f.open("rb") as fh:
            heads.append((f, f.stat().st_size, fh.read(args.width)))

    # per-offset: constant or varying
    out(f"## Byte-by-byte over the first {args.width} bytes (0x{args.width:x})")
    out("")
    out("| off | hex of the field across all files | verdict |")
    out("|---|---|---|")
    const_runs: list[tuple[int, int]] = []
    run_start = None
    varying = []
    for i in range(args.width):
        vals = {h[2][i] for h in heads if i < len(h[2])}
        if len(vals) == 1:
            if run_start is None:
                run_start = i
        else:
            if run_start is not None:
                const_runs.append((run_start, i))
                run_start = None
            varying.append(i)
    if run_start is not None:
        const_runs.append((run_start, args.width))

    if not args.diff_only:
        for a, b in const_runs:
            vals = heads[0][2][a:b]
            out(f"| 0x{a:02x}..0x{b:02x} | `{vals.hex(' ')}` | **constant** ({b-a} bytes) |")
        out("")
    out("### Varying offsets (candidates for counts/offsets/sizes)")
    out("")
    for i in varying:
        col = [h[2][i] for h in heads if i < len(h[2])]
        out(f"| 0x{i:02x} | " + " ".join(f"{c:02x}" for c in col) + " |")
    out("")

    # decode the varying region as u32 LE at every aligned offset
    out("## Same window read as u32 little-endian (aligned offsets only)")
    out("")
    out("| off | " + " | ".join(f.stem.replace("Spore_", "")[:14] for _, _, _ in heads[:6]) + " |")
    out("|---" * (min(6, len(heads)) + 1) + "|")
    for i in range(0, args.width - 3, 4):
        vals = []
        for _, size, h in heads[:6]:
            vals.append(struct.unpack("<I", h[i : i + 4])[0] if i + 4 <= len(h) else 0)
        if len(set(vals)) == 1:
            tag = "const"
        else:
            tag = ""
        out("| 0x%02x | " % i + " | ".join(f"{v}{('*' if tag else '')}" for v in vals) + f" | {tag} |")
    out("")

    # correlate varying u32s with file size
    out("## Correlation with file size")
    out("")
    out("| off | u32 values | file sizes | ratio size/value (if constant-ish) |")
    out("|---|---|---|---|")
    for i in range(0, args.width - 3, 4):
        vals = [struct.unpack("<I", h[i : i + 4])[0] for _, _, h in heads]
        sizes = [s for _, s, _ in heads]
        nz = [(v, s) for v, s in zip(vals, sizes) if v]
        if len(nz) < 2:
            continue
        ratios = [s / v for v, s in nz]
        spread = max(ratios) - min(ratios)
        if spread < 0.5 * max(1e-9, max(ratios)):
            out(f"| 0x{i:02x} | {vals[:6]} | sizes[:3]={sizes[:3]} | ratio ~{sum(ratios)/len(ratios):.3f} (spread {spread:.3f}) |")
    out("")
    return 0


if __name__ == "__main__":
    sys.exit(main())
