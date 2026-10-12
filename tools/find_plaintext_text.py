"""Find which candidate binaries have a *plaintext* .text (i.e. are reversible statically).

Why: refs/bins/ReleaseGA/SporeApp.exe ships with an encrypted .text (entropy 8.000),
which makes the field-guide's "disassembly is authority #1" rule unusable on it. Any
build whose .text has ordinary code entropy (~6.2-6.9 for x86) can be disassembled with
objdump directly, so it becomes the reversal oracle instead.

Method: for every PE under the given roots, compute Shannon entropy per section and
classify. Classification thresholds are stated in the output so they can be argued with.

Usage:
    python tools/find_plaintext_text.py                # scans refs/bins by default
    python tools/find_plaintext_text.py <dir> [<dir>…]
Writes: analysis/plaintext_text.md
"""
from __future__ import annotations

import math
import struct
import sys
from collections import Counter
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

DEFAULT_ROOTS = [_ROOT / "refs" / "bins"]
OUT = _ROOT / "analysis" / "plaintext_text.md"

# Threshold rationale: 256-symbol Shannon entropy of compressed/encrypted data is
# >= 7.99; native x86 code of this size sits near 6.2-6.9. 7.5 is the split, and it is
# deliberately conservative on the "call it encrypted" side (a false 'encrypted' costs
# us a disasm attempt; a false 'plaintext' costs us trusting garbage disasm).
ENCRYPTED_AT = 7.5


def entropy(data: bytes) -> float:
    if not data:
        return 0.0
    c = Counter(data)
    n = len(data)
    return -sum((v / n) * math.log2(v / n) for v in c.values())


def sections_of(path: Path):
    """Minimal PE section walker (no pefile dependency, so it works on any PE)."""
    with path.open("rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        return None, None, None
    pe_off = struct.unpack("<I", data[0x3C:0x40])[0]
    if pe_off + 24 > len(data) or data[pe_off : pe_off + 4] != b"PE\x00\x00":
        return None, None, None
    machine, nsec = struct.unpack("<HH", data[pe_off + 4 : pe_off + 8])
    opt_size, = struct.unpack("<H", data[pe_off + 20 : pe_off + 22])
    sec_off = pe_off + 24 + opt_size
    magic, = struct.unpack("<H", data[pe_off + 24 : pe_off + 26])
    image_base, = struct.unpack("<I", data[pe_off + 24 + 28 : pe_off + 24 + 32]) if magic == 0x10B else (0,)
    out = []
    for i in range(nsec):
        o = sec_off + i * 40
        if o + 40 > len(data):
            break
        name = data[o : o + 8].rstrip(b"\x00").decode("latin-1")
        vsize, va, rsize, rptr = struct.unpack("<IIII", data[o + 8 : o + 24])
        chars, = struct.unpack("<I", data[o + 36 : o + 40])
        blob = data[rptr : rptr + max(rsize, vsize)] if rsize else b""
        out.append((name, image_base + va, vsize, rsize, chars, entropy(blob)))
    return machine, image_base, out


def main(argv: list[str]) -> int:
    roots = [Path(a) for a in argv] or DEFAULT_ROOTS
    files: list[Path] = []
    for r in roots:
        if r.is_file():
            files.append(r)
        elif r.is_dir():
            files += sorted(p for p in r.rglob("*") if p.is_file())
    rows = []
    for p in files:
        try:
            machine, base, secs = sections_of(p)
        except Exception as exc:  # noqa: BLE001 - a scanner must never die on one file
            print(f"! {p}: {type(exc).__name__}: {exc}")
            continue
        if machine is None:
            continue
        text = next((s for s in secs if s[0] == ".text"), None)
        ep = next((s for s in secs if s[0] == ".eptext"), None)
        rows.append((p, machine, base, secs, text, ep))

    lines = ["# Which reference binaries have a plaintext .text?", ""]
    lines.append(f"Threshold: entropy >= {ENCRYPTED_AT} -> classified **encrypted**; below -> plaintext.")
    lines.append("(`.eptext` is the EncryptPE section that holds the encrypted payload when .text is left as a stub.)")
    lines.append("")
    lines.append("| file | size | machine | image_base | .text entropy | .eptext entropy | verdict |")
    lines.append("|---|---|---|---|---|---|---|")
    for p, machine, base, secs, text, ep in rows:
        rel = p.relative_to(_ROOT) if p.is_relative_to(_ROOT) else p
        te = f"{text[5]:.3f}" if text else "-"
        ee = f"{ep[5]:.3f}" if ep else "-"
        if text is None:
            verdict = "no `.text`"
        elif text[5] >= ENCRYPTED_AT:
            verdict = "**encrypted**" + (" (+ .eptext present)" if ep else "")
        else:
            verdict = "**PLAINTEXT - usable for disasm**"
        lines.append(
            f"| `{rel}` | {p.stat().st_size:,} | 0x{machine:04x} | 0x{base:08x} | {te} | {ee} | {verdict} |"
        )
    lines.append("")
    lines.append("## Full section tables")
    lines.append("")
    for p, machine, base, secs, text, ep in rows:
        rel = p.relative_to(_ROOT) if p.is_relative_to(_ROOT) else p
        lines.append(f"### `{rel}`")
        lines.append("")
        lines.append("| section | va | vsize | raw | chars | entropy |")
        lines.append("|---|---|---|---|---|---|")
        for name, va, vsize, rsize, chars, e in secs:
            lines.append(f"| `{name}` | 0x{va:08x} | 0x{vsize:08x} | 0x{rsize:08x} | 0x{chars:08x} | {e:.3f} |")
        lines.append("")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(l for l in lines if l.startswith("|") or l.startswith("#") or l.startswith("Threshold")))
    print(f"\nwrote {OUT.relative_to(_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
