"""Dump a run of u32s at a virtual address, annotating anything that looks like a string or code pointer.

Used to read registration/dispatch tables: a table of (type id, handler) or (name, id) is the
fastest way from a resource TYPE to the code that parses it.

Usage: python tools/dump_u32.py 0x15777e0 --len 32
       python tools/dump_u32.py 0x140e028 --len 24 --annotate
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pe import PE


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("va")
    ap.add_argument("--len", type=int, default=16)
    ap.add_argument("--annotate", action="store_true")
    a = ap.parse_args()
    va = int(a.va, 16)
    pe = PE()
    t0 = pe.sections[".text"]["va"]
    t1 = t0 + pe.sections[".text"]["vsize"]
    rows = []
    for i in range(a.len):
        p = va + i * 4
        try:
            v = pe.u32(p)
        except Exception as e:  # noqa: BLE001
            rows.append((p, None, f"<{e}>"))
            break
        ann = ""
        if t0 <= v < t1:
            ann = "-> code"
        else:
            sec = pe.section_of(v)
            if sec and sec["name"] in (".rdata", ".data", "CONST"):
                try:
                    s = pe.cstr(v, 48).decode("latin-1", "replace")
                except Exception:  # noqa: BLE001
                    s = ""
                printable = s and all(32 <= ord(c) < 127 for c in s)
                if printable:
                    ann = f'"{s}"'
                elif 0x13cc000 <= v < 0x1730000:
                    ann = f"[{sec['name']}]"
        rows.append((p, v, ann))
    for p, v, ann in rows:
        if v is None:
            print(f"  0x{p:08x}  {ann}")
        else:
            print(f"  0x{p:08x}  0x{v:08x}  {ann}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
