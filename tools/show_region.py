"""Show the string/constant neighbourhood of a virtual address.

Used to read argument tables, format-string clusters and .eBT behaviour-tree records
out of the plaintext sections (.rdata/.data). Prints a hexdump plus every string that
starts inside the window, so a suspected table can be confirmed or disproven in one go.

Usage:
    python tools/show_region.py 0x013f8592                       # +/-0x300, .rdata
    python tools/show_region.py 0x0150c000 0x100 --sec .data
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.pe import PE
from tools.strings_scan import all_strings


def hexdump(pe: PE, lo: int, hi: int) -> None:
    data = pe.read(lo, hi - lo)
    for i in range(0, len(data), 16):
        row = data[i : i + 16]
        ascii_col = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
        print(f"0x{lo+i:08x}  {' '.join('%02x' % b for b in row):<47} |{ascii_col}|")


def floats(pe: PE, lo: int, hi: int) -> list[tuple[int, float]]:
    out = []
    for a in range(lo, hi - 3, 4):
        v = struct.unpack("<f", pe.read(a, 4))[0]
        if v == v and 1e-6 < abs(v) < 1e7:  # finite, non-zero, plausible
            out.append((a, v))
    return out


def main(argv: list[str]) -> int:
    if not argv:
        print(__doc__)
        return 2
    va = int(argv[0], 16)
    radius = 0x300
    sec = ".rdata"
    if len(argv) > 1 and not argv[1].startswith("--"):
        radius = int(argv[1], 0)
    if "--sec" in argv:
        sec = argv[argv.index("--sec") + 1]
    hexmode = "--hex" in argv

    pe = PE()
    s = pe.sections[sec]
    sec_data = pe.section_bytes(sec)
    sec_end = s["va"] + len(sec_data)
    lo = max(s["va"], va - radius)
    hi = min(sec_end, va + radius)

    if hexmode:
        hexdump(pe, lo, hi)
        return 0

    rows = all_strings(sec_data[lo - s["va"] : hi - s["va"]], lo)
    print(f"# region {sec} 0x{lo:08x}..0x{hi:08x}   centre 0x{va:08x}")
    for a, kind, txt in rows:
        mark = "  <<<" if abs(a - va) < 16 else ""
        print(f"0x{a:08x} {kind} {txt!r}{mark}")
    if not rows:
        print("# no strings in region -> likely a data/constant table; try --hex")
        hexdump(pe, lo, hi)
        fv = floats(pe, lo, hi)
        if fv:
            print("# plausible float32 values (4-byte aligned):")
            for a, v in fv[:40]:
                print(f"0x{a:08x} {v!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
