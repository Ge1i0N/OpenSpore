"""Find a string in the oracle's .rdata/.data and report who references it in .text.

This is the workhorse for reversal by cross-reference: the game's own format strings and
table keys are the fastest way to locate the function that uses them, because the code
embeds their addresses as immediates.

Usage:
    python tools/xref.py "Switch to given state"
    python tools/xref.py --regex "state" --sec .rdata --limit 20
    python tools/xref.py --addr 0x01413730 --addr 0x013f8dc8
Options:
    --sec NAME      restrict the string search to one section (default: .rdata then .data)
    --no-xref       just list matching strings, do not resolve references
    --limit N       max strings to report (default 20)
    --ctx N         disassemble N instructions of context around each reference (default 0)
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

from tools.pe import PE, DEFAULT_EXE
from tools.strings_scan import all_strings

OBJDUMP = Path(r"C:\Users\DartPower\scoop\apps\mingw\current\bin\objdump.exe")


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def load_strings(pe: PE, secs: list[str]) -> list[tuple[int, str, str]]:
    rows: list[tuple[int, str, str]] = []
    for name in secs:
        if name not in pe.sections:
            continue
        s = pe.sections[name]
        rows += all_strings(pe.section_bytes(name), s["va"], 3)
    return rows


def disasm_ctx(pe: PE, addr: int, before: int = 0x30, after: int = 0x20) -> list[str]:
    lo = max(pe.sections[".text"]["va"], addr - before)
    hi = min(pe.sections[".text"]["end_va"], addr + after)
    cmd = [str(OBJDUMP), "-d", "-M", "intel", f"--start-address=0x{lo:x}", f"--stop-address=0x{hi:x}", str(pe.path)]
    txt = subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout
    lines = [ln for ln in txt.splitlines() if re.match(r"^\s+[0-9a-f]+:", ln)]
    keep = []
    for ln in lines:
        a = int(ln.strip().split(":", 1)[0], 16)
        if a >= addr:
            keep.append(ln.rstrip())
    return keep[:12]


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("patterns", nargs="*", help="literal substrings to find (case-insensitive)")
    ap.add_argument("--regex", action="append", default=[], help="regex to match against decoded strings")
    ap.add_argument("--addr", action="append", default=[], help="exact string VA (hex), bypasses the search")
    ap.add_argument("--sec", default=None, help="restrict search to this section")
    ap.add_argument("--no-xref", action="store_true")
    ap.add_argument("--limit", type=int, default=20)
    ap.add_argument("--ctx", type=int, default=0, help="also disassemble around each reference")
    args = ap.parse_args()

    pe = PE()
    out(f"# oracle: {pe.path.name}  (image base 0x{pe.base:08x})")
    text = pe.sections[".text"]
    tblob = pe.section_bytes(".text")

    targets: list[tuple[int, str, str]] = []
    if args.addr:
        for a in args.addr:
            va = int(a, 16) if isinstance(a, str) else a
            sec = pe.section_of(va)
            kind = "u"
            val = pe.wstr(va)
            if val == "" or "\\u" in val[:2]:
                alt = pe.cstr(va, 128).decode("latin-1", "replace")
                if alt:
                    val, kind = alt, "a"
            targets.append((va, kind, f"{val}  [{sec['name'] if sec else '?'}]"))
    else:
        secs = [args.sec] if args.sec else [".rdata", ".data", "CONST"]
        rows = load_strings(pe, secs)
        pats = [p.lower() for p in args.patterns]
        regs = [re.compile(r, re.I) for r in args.regex]
        for va, kind, txt in rows:
            low = txt.lower()
            if any(p in low for p in pats) or any(rx.search(txt) for rx in regs):
                targets.append((va, kind, txt))
                if len(targets) >= args.limit:
                    break

    if not targets:
        out("! no matching strings")
        return 1

    for va, kind, txt in targets:
        out(f"\n## 0x{va:08x} ({'utf16' if kind=='u' else 'ascii'}) {ascii(txt)}")
        if args.no_xref:
            continue
        refs = [a for a in pe.find_bytes(struct.pack("<I", va), ".text")]
        if not refs:
            out("   no immediate references in .text")
            continue
        # group references by the function they live in (nearest preceding 16-aligned
        # function start is approximated by the nearest preceding int3-run end)
        for r in refs:
            out(f"   ref @ 0x{r:08x}")
            if args.ctx:
                for ln in disasm_ctx(pe, r):
                    out("      " + ln.strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
