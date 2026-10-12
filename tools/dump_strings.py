"""Dump ASCII/UTF-16 strings from the plaintext sections of SporeApp.exe (GA).

Why this exists: the shipped GA exe has an **encrypted .text** (entropy 8.000, garbage
disasm - see MEMORY.md), but .rdata/.data are plaintext, so strings + data tables are
still recoverable statically. This script is the reader for that evidence.

Output: analysis/strings/<section>.txt  with lines "0xVA\tkind\ttext"
where kind is 'a' (ASCII) or 'u' (UTF-16LE).

Usage:
    python tools/dump_strings.py                # all plaintext-ish sections
    python tools/dump_strings.py .rdata .data   # selected
"""
from __future__ import annotations

import sys
from pathlib import Path

# Allow both `python tools/dump_strings.py` and `python -m tools.dump_strings`.
_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.pe import PE, ROOT
from tools.strings_scan import all_strings

OUT = ROOT / "analysis" / "strings"


def main(argv: list[str]) -> int:
    pe = PE()
    wanted = argv[1:] or [".rdata", ".data", "CONST", ".tls"]
    OUT.mkdir(parents=True, exist_ok=True)
    for name in wanted:
        if name not in pe.sections:
            print(f"skip {name}: no such section")
            continue
        s = pe.sections[name]
        data = pe.section_bytes(name)
        rows = all_strings(data, s["va"])
        path = OUT / f"{name.strip('.').lower()}.txt"
        with path.open("w", encoding="ascii", errors="replace") as f:
            for va, kind, txt in rows:
                f.write(f"0x{va:08x}\t{kind}\t{txt}\n")
        print(f"{name:<8} {len(rows):>7} strings -> {path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
