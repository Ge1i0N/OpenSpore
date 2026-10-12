"""Find immediate constants in the oracle's .text, and who uses them.

Why: the field guide's "who references a global?" trick only works for *addresses*.
Format magics, hashes and small integers appear as inline immediates, so a binary with
no 'DBPF' string still contains the dword 0x46504244. This is how a file-format reader
is located when the string table does not help.

Reports each hit with the surrounding bytes so the instruction form is visible
(`cmp reg,imm` / `mov reg,imm` / `push imm`), and the nearest preceding int3-run end,
which is a reliable function-start estimate (verified: 99.92% of int3 runs end 16-aligned).

Usage:
    python tools/find_imm.py 0x46504244            # 'DBPF' little-endian
    python tools/find_imm.py 0x46504244 0x811c9dc5 # several at once
    python tools/find_imm.py --ascii DBPF          # convert for you
"""
from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.pe import PE

OBJDUMP = Path(r"C:\Users\DartPower\scoop\apps\mingw\current\bin\objdump.exe")


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def func_start(blob: bytes, t0: int, off: int, back: int = 0x400) -> int:
    """Nearest preceding int3-run end = estimated function start."""
    lo = max(0, off - back)
    best = None
    i = off
    while i > lo:
        if blob[i] == 0xCC:
            j = i
            while j > lo and blob[j - 1] == 0xCC:
                j -= 1
            end = j  # first non-CC byte at/after the run
            if end < len(blob):
                best = t0 + end
            break
        i -= 1
    return best if best is not None else t0 + lo


def disasm(pe: PE, lo: int, hi: int) -> list[str]:
    cmd = [str(OBJDUMP), "-d", "-M", "intel", f"--start-address=0x{lo:x}", f"--stop-address=0x{hi:x}", str(pe.path)]
    txt = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    return [ln.rstrip() for ln in txt.splitlines() if re.match(r"^\s+[0-9a-f]+:", ln)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("values", nargs="+", help="hex immediates, or use --ascii")
    ap.add_argument("--ascii", action="store_true", help="treat each value as a 1-4 char ASCII literal")
    ap.add_argument("--ctx", type=int, default=2, help="instructions to show around each hit")
    args = ap.parse_args()

    pe = PE()
    text = pe.sections[".text"]
    t0 = text["va"]
    blob = pe.section_bytes(".text")

    wanted: list[tuple[int, str]] = []
    for v in args.values:
        if args.ascii:
            b = v.encode("latin-1")
            if len(b) > 4:
                out(f"! {v!r} is longer than 4 bytes; use the hex form instead")
                return 2
            wanted.append((struct.unpack("<I", b.ljust(4, b"\x00"))[0], v))
        else:
            wanted.append((int(v, 16), v))

    out(f"# oracle: {pe.path.name}")
    for imm, label in wanted:
        packed = struct.pack("<I", imm)
        hits = [t0 + i for i in range(len(blob) - 3) if blob[i : i + 4] == packed]
        out(f"\n## immediate 0x{imm:08x} ({label}) - {len(hits)} raw byte matches in .text")
        out("   (byte matches include false positives from data/immediates of other widths;"
            " the disasm lines below are the filter)")
        for h in hits[:40]:
            fs = func_start(blob, t0, h - t0)
            out(f"\n  @ 0x{h:08x}   enclosing function starts ~0x{fs:08x}")
            if args.ctx:
                for ln in disasm(pe, max(t0, h - 0x18), min(t0 + len(blob), h + 0x10)):
                    addr = int(ln.strip().split(":", 1)[0], 16)
                    if abs(addr - h) <= 0x14:
                        mark = "  <<<" if addr == h else ""
                        out(f"      {ln.strip()}{mark}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
