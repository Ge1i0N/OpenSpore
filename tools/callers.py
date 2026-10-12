"""Find every relative/near CALL and JMP that targets a function address.

Why: `tools/xref.py` answers "who references this *string*". Reversal needs the other
question constantly - "who CALLS this function?" - and the answer is a rel32 immediate
inside an E8 (call) / E9 (jmp) / FF 25 (jmp [iat], handled separately) instruction, so the
byte pattern is `E8 <rel32>` where target = site + 5 + rel.

Reports each caller with its enclosing function start (nearest preceding int3-run end),
which is how you climb from a leaf helper up to the subsystem that owns it.

Usage:
    python tools/callers.py 0x8d90ae            # the DBPF header validator
    python tools/callers.py 0x8d90ae 0x8d8bf0   # several
    python tools/callers.py 0x8d90ae --disasm   # show the call site in context
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


def func_start(blob: bytes, t0: int, off: int, back: int = 0x800) -> int:
    lo = max(0, off - back)
    i = off
    while i > lo:
        if blob[i] == 0xCC:
            j = i
            while j > lo and blob[j - 1] == 0xCC:
                j -= 1
            if j < len(blob):
                return t0 + j
            break
        i -= 1
    return t0 + lo


def disasm(pe: PE, lo: int, hi: int) -> list[str]:
    cmd = [str(OBJDUMP), "-d", "-M", "intel", f"--start-address=0x{lo:x}", f"--stop-address=0x{hi:x}", str(pe.path)]
    txt = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    return [ln.rstrip() for ln in txt.splitlines() if re.match(r"^\s+[0-9a-f]+:", ln)]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("targets", nargs="+")
    ap.add_argument("--disasm", action="store_true")
    ap.add_argument("--limit", type=int, default=40)
    args = ap.parse_args()

    pe = PE()
    text = pe.sections[".text"]
    t0 = text["va"]
    blob = pe.section_bytes(".text")
    out(f"# oracle: {pe.path.name}   .text 0x{t0:08x}..0x{t0+len(blob):08x}")

    # Precompute every E8/E9 target once (16 MB scan, ~10 s) so multiple targets are free.
    calls: dict[int, list[tuple[int, int]]] = {}  # target -> [(site, kind)]  kind 0=call 1=jmp
    for i in range(len(blob) - 5):
        op = blob[i]
        if op not in (0xE8, 0xE9):
            continue
        rel = struct.unpack("<i", blob[i + 1 : i + 5])[0]
        tgt = t0 + i + 5 + rel
        if not (t0 <= tgt < t0 + len(blob)):
            continue
        calls.setdefault(tgt, []).append((t0 + i, 0 if op == 0xE8 else 1))
    out(f"# indexed {sum(len(v) for v in calls.values()):,} branch sites")

    for ts in args.targets:
        tgt = int(ts, 16)
        hits = calls.get(tgt, [])
        out(f"\n## callers of 0x{tgt:08x}: {len(hits)}")
        for site, kind in hits[: args.limit]:
            fs = func_start(blob, t0, site - t0)
            out(f"  {'call' if kind == 0 else 'jmp '} @ 0x{site:08x}   in function ~0x{fs:08x}")
            if args.disasm:
                for ln in disasm(pe, max(t0, site - 0x20), min(t0 + len(blob), site + 0x10)):
                    a = int(ln.strip().split(":", 1)[0], 16)
                    if site - 0x10 <= a <= site + 0x10:
                        out(("      " + ln.strip() + ("  <<<" if a == site else "")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
