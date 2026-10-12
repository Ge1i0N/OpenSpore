"""Enumerate every argument name looked up through a given helper function.

Why this is the right move: the target's arg lookup (`0x0092b570`) takes the *name string
pointer* as its first pushed argument, so the complete command-line/config vocabulary is
recoverable mechanically from the call sites - no guessing, no wiki.

Method: find every E8-relative `call` whose target is `helper`, then walk back up to
`--scan` bytes looking for `68 <imm32>` (push imm32) operands that point at a readable
string in .rdata/.data. Both ASCII and UTF-16 names are decoded.

Usage:
    python tools/arg_vocab.py                      # the devdirs/shipDirs lookup helper
    python tools/arg_vocab.py 0x92b570 --ctx 24
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.pe import PE


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def decode(pe: PE, va: int) -> tuple[str, str] | None:
    """Return (kind, text) if `va` looks like a string, else None."""
    sec = pe.section_of(va)
    if sec is None or sec["name"] not in (".rdata", ".data", "CONST"):
        return None
    try:
        a = pe.cstr(va, 96).decode("latin-1", "replace")
    except Exception:  # noqa: BLE001
        a = ""
    u = ""
    try:
        u = pe.wstr(va)
    except Exception:  # noqa: BLE001
        pass
    a_ok = len(a) >= 2 and all(32 <= ord(c) < 127 for c in a)
    u_ok = len(u) >= 2 and "\\" not in u and all(32 <= ord(c) < 127 for c in u)
    # Prefer whichever is longer/cleaner; a UTF-16 name reads as alternating NULs in ASCII.
    if u_ok and (not a_ok or len(u) > len(a)):
        return ("utf16", u)
    if a_ok:
        return ("ascii", a)
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("helper", nargs="?", default="0x92b570")
    ap.add_argument("--scan", type=int, default=28, help="bytes to walk back for the push imm32")
    ap.add_argument("--out", default=None, help="write the vocabulary here (markdown)")
    args = ap.parse_args()
    helper = int(args.helper, 16)

    pe = PE()
    text = pe.sections[".text"]
    t0 = text["va"]
    blob = pe.section_bytes(".text")

    sites: list[tuple[int, int]] = []  # (call_site_va, name_va)
    total_calls = 0
    for i in range(len(blob) - 5):
        if blob[i] != 0xE8:
            continue
        rel = struct.unpack("<i", blob[i + 1 : i + 5])[0]
        site = t0 + i
        if site + 5 + rel != helper:
            continue
        total_calls += 1
        # walk back for `68 <imm32>`
        lo = max(0, i - args.scan)
        found = None
        for j in range(i - 5, lo - 1, -1):
            if j < 0 or blob[j] != 0x68:
                continue
            imm = struct.unpack("<I", blob[j + 1 : j + 5])[0]
            d = decode(pe, imm)
            if d:
                found = (j, imm, d)
                break
        if found:
            sites.append((site, found[1]))

    names: Counter[str] = Counter()
    detail: list[tuple[int, int, str, str]] = []
    for site, imm in sites:
        d = decode(pe, imm)
        if not d:
            continue
        kind, txt = d
        names[txt] += 1
        detail.append((site, imm, kind, txt))

    lines = []
    lines.append(f"# Argument vocabulary via helper 0x{helper:08x}")
    lines.append(f"- oracle: {pe.path.name}")
    lines.append(f"- call sites to the helper: {total_calls}")
    lines.append(f"- sites with a decodable name pushed: {len(detail)}")
    lines.append(f"- distinct names: {len(names)}")
    lines.append("")
    lines.append("## Vocabulary (alphabetical)")
    lines.append("")
    for n in sorted(names):
        lines.append(f"- `{n}`  x{names[n]}")
    lines.append("")
    lines.append("## Call sites")
    lines.append("")
    lines.append("| call site | name VA | kind | name |")
    lines.append("|---|---|---|---|")
    for site, imm, kind, txt in detail:
        lines.append(f"| 0x{site:08x} | 0x{imm:08x} | {kind} | `{txt}` |")
    lines.append("")

    out("\n".join(lines))
    if args.out:
        p = Path(args.out)
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text("\n".join(lines) + "\n", encoding="utf-8")
        out(f"wrote {p.relative_to(_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
