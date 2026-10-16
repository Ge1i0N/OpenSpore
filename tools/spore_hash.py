r"""Spore name hash (FNV-1, case-folded) + name registry loader.

Spore names resources and properties by a 32-bit **FNV-1** hash of the *lower-cased*
name: ``h = (h * 0x01000193) ^ c`` over ``ord(c) | 0x20`` for ASCII letters, seeded with
``0x811C9DC5``. Verified on real data:

* resource types: ``fnv1_lower("cell") == 0xDFAD9F51`` (189 cells in `CellGame.package`),
  ``"structure" -> 0x4B9EF6DC``, ``"world" -> 0x9B8E862F``, ``"globals" -> 0x2A3CE5B7``;
* cell archetypes: ``"cell_dragon" -> 0x5905CC05``, ``"cell_squid" -> 0x4BC16582``;
* cell-globals property keys: ``"time_dilation" -> 0x18B75C47``, ``"cilia_count" ->
  0x2EF68471`` (see `tools/cell_game.py`).

Registries live in ``upstream/SporeMaster3/data/`` (``reg_file.txt``, ``reg_property.txt``,
``reg_type.txt``, ``reg_simulator.txt``). A registry line is either ``name`` (id = FNV) or
``name<TAB>0xID`` (an explicit alias whose id is *not* FNV of the name).

Usage: python tools/spore_hash.py cell cell_dragon time_dilation
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
REG_DIR = ROOT / "upstream" / "SporeMaster3" / "data"

FNV_OFFSET = 0x811C9DC5
FNV_PRIME = 0x01000193
FNV_UNSTEP = 0x359C449B  # 0x01000193 * FNV_UNSTEP == 1 (mod 2**32)


def fnv1_lower(name: str) -> int:
    """The Spore hash: FNV-1 over the ASCII-lower-cased name."""
    h = FNV_OFFSET
    for ch in name:
        c = ord(ch)
        if 65 <= c <= 90:
            c += 32
        h = ((h * FNV_PRIME) & 0xFFFFFFFF) ^ c
    return h


def fnv1(name: str) -> int:
    """FNV-1 over the name byte-for-byte (case preserved)."""
    h = FNV_OFFSET
    for ch in name:
        h = ((h * FNV_PRIME) & 0xFFFFFFFF) ^ (ord(ch) & 0xFF)
    return h


def fnv_reverse(h: int, tail: str) -> int:
    """Remove a known ``tail`` from a Spore hash: returns the hash of the prefix."""
    for ch in reversed(tail):
        c = ord(ch)
        if 65 <= c <= 90:
            c += 32
        h ^= c
        h = (h * FNV_UNSTEP) & 0xFFFFFFFF
    return h


def load_registry(directory: Path | None = None) -> dict[int, str]:
    """Merge every ``reg_*.txt`` in ``directory`` into one ``id -> name`` map."""
    reg: dict[int, str] = {}
    for path in sorted((directory or REG_DIR).glob("reg_*.txt")):
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            if "\t" in line:
                name, _, h = line.partition("\t")
                h = h.strip()
                if h.lower().startswith("0x"):
                    try:
                        reg[int(h, 16)] = name.strip()
                        continue
                    except ValueError:
                        pass
                line = name.strip()
            if not line:
                continue
            reg.setdefault(fnv1_lower(line), line)
    return reg


def main() -> int:
    names = sys.argv[1:]
    reg = load_registry()
    out = sys.stdout.buffer
    for n in names:
        h = fnv1_lower(n)
        alias = reg.get(h)
        tag = " (alias)" if alias and alias != n else ""
        out.write(("0x%08X  %-30s registered=%r%s\n" % (h, n, alias, tag)).encode("ascii", "replace"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
