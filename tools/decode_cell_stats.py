r"""Dump the cell archetype records with their RECOVERED field names.

The `cell` archetype record is a fixed struct; the GA type descriptor
(``FUN_01375e30`` @ ``0x1375e30``) names every field and its byte offset, and the
``cAIData`` descriptor (@ ``0x1374725``) names the 46-field AI block that lives at
record ``+0xE0``. See ``analysis/cell-game.md``.

Usage:
    python tools/decode_cell_stats.py                       # summary of the shipped package
    python tools/decode_cell_stats.py --archetype cell_hero_L0
    python tools/decode_cell_stats.py --json out.json

NOTE: this tool previously called ``refpack.decode(data, len, size)``; the real
signature is ``decode(data, expect)``, so it silently produced garbage. It now
reuses ``tools/cell_game.iter_resources`` (the project's own, correct reader).
"""
from __future__ import annotations

import argparse
import json
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from tools.cell_game import CELL, find_packages, iter_resources  # noqa: E402
from tools.spore_hash import load_registry  # noqa: E402

# Top-level cell-archetype fields (record offsets, from the GA `cell` descriptor).
SCALARS = [
    ("structure", 0x000, "u32"),
    ("hp", 0x0A8, "u32"),
    ("fixedOrientation", 0x0AC, "u32"),
    ("flags", 0x0B0, "u32"),
    ("cellType", 0x0B4, "u32"),
    ("unlockType", 0x0B8, "u32"),
    ("density", 0x0BC, "u32"),
    ("sound", 0x0C0, "u32"),
    ("break", 0x0C4, "u32"),
    ("pieces", 0x0C8, "u32"),
    ("leak", 0x0CC, "u32"),
    ("expel", 0x0D0, "u32"),
    ("loot", 0x0D4, "u32"),
    ("explosionTable", 0x0D8, "u32"),
    ("poison", 0x0DC, "u32"),
]

# The `cAIData` block at record +0xE0 (offsets relative to the block base).
AI = [
    ("awarenessRadius", 0x04), ("awarenessRadiusFood", 0x08), ("awarenessRadiusPredator", 0x0C),
    ("movementStyle", 0x10), ("flocking", 0x14), ("speed", 0x18), ("chaseSpeed", 0x1C),
    ("wanderSpeed", 0x20), ("fleeSpeed", 0x24), ("fearsNearbyDamageRadius", 0x28),
    ("fearsNearbyDeathRadius", 0x2C), ("fearsNearbyDamageTime", 0x30), ("fearsNearbyDeathTime", 0x34),
    ("protectRadius", 0x38), ("protectTime", 0x3C), ("turnFactor", 0x40), ("axialMovement", 0x44),
    ("spawnTime", 0x48), ("spawnRestTime", 0x4C), ("spawnOutput", 0x50), ("arcLength", 0x54),
    ("arcLengthSecondary", 0x58), ("numArcs", 0x5C), ("keyTransformation", 0x60),
    ("keyProjectile", 0x64), ("food", 0x68), ("growCount", 0x6C), ("digestionCount", 0x70),
    ("digestionTime", 0x74), ("digestionOutput", 0x78), ("fleeTime", 0x7C),
    ("fleeRestTime", 0x80), ("chaseTime", 0x84), ("chaseRestTime", 0x88),
    ("chasesDamage", 0x8C), ("fearsMouths", 0x8D), ("fearsWeapons", 0x8E), ("fearsDamage", 0x8F),
    ("fearsElectric", 0x90), ("fearsPoison", 0x91), ("ignoresFood", 0x92),
    ("awakeTime", 0x94), ("sleepTime", 0x98), ("growAmount", 0x9C), ("hatchDuration", 0xA0),
    ("poisonRecharge", 0xA4), ("electricRecharge", 0xA8), ("electricRechargeVsSmall", 0xAC),
    ("electricDischarge", 0xB0),
]
AI_BASE = 0xE0

# The cAIData block is MIXED-TYPE. The split was measured over all 189 shipped archetypes:
# a field whose raw word is a small integer in every record is an integer, one whose raw word
# is a sane float bit pattern is a float, and the rest are packed/raw words. Reading an
# integer field as a float yields a denormal (turnFactor 3 -> 4.2e-45).
AI_INT = {
    "awarenessRadiusPredator", "turnFactor", "spawnRestTime", "spawnOutput", "arcLength",
    "digestionTime", "fleeRestTime", "awakeTime", "sleepTime", "poisonRecharge",
    "electricRecharge", "electricRechargeVsSmall",
}
AI_BYTE = {
    "chasesDamage", "fearsMouths", "fearsWeapons", "fearsDamage", "fearsElectric",
    "fearsPoison", "ignoresFood",
}
AI_RAW = {"axialMovement", "spawnTime", "numArcs", "digestionCount"}


def u32(d: bytes, off: int) -> int:
    return struct.unpack_from("<I", d, off)[0] if off + 4 <= len(d) else 0


def f32(d: bytes, off: int) -> float:
    return struct.unpack_from("<f", d, off)[0] if off + 4 <= len(d) else 0.0


def u8(d: bytes, off: int) -> int:
    return d[off] if off < len(d) else 0


def utf16(d: bytes, off: int) -> str:
    out = []
    for i in range(off, min(off + 256, len(d) - 1), 2):
        ch = struct.unpack_from("<H", d, i)[0]
        if ch == 0:
            break
        if 32 <= ch < 127:
            out.append(chr(ch))
        else:
            break
    return "".join(out)


def describe(rec: bytes) -> dict:
    part_count = struct.unpack_from("<H", rec, 0x0A4)[0] if 0x0A6 <= len(rec) else 0
    out = {"template": u32(rec, 0x000), "display": utf16(rec, 0x004), "part_count": part_count}
    for name, off, _kind in SCALARS[1:]:
        out[name] = u32(rec, off)
    ai = {}
    for name, off in AI:
        if name in AI_BYTE:
            ai[name] = u8(rec, AI_BASE + off)
        elif name in AI_INT:
            ai[name] = u32(rec, AI_BASE + off)
        elif name in AI_RAW:
            ai[name] = "0x%08X" % u32(rec, AI_BASE + off)
        else:
            ai[name] = f32(rec, AI_BASE + off)
    out["ai"] = ai
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--archetype", help="only this archetype name/id")
    ap.add_argument("--json", help="write the full dump as JSON")
    a = ap.parse_args()

    names = load_registry()
    cells = []
    seen = set()
    for pkg in find_packages():
        for t, _g, inst, data in iter_resources(pkg):
            if t != CELL or not data:
                continue
            if inst in seen:
                continue
            seen.add(inst)
            name = names.get(inst)
            if a.archetype and name != a.archetype and f"0x{inst:08x}" != a.archetype.lower():
                continue
            rec = describe(data)
            rec["instance"] = f"0x{inst:08x}"
            rec["name"] = name or ""
            cells.append(rec)

    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump(cells, f, indent=2)
        print(f"wrote {a.json} ({len(cells)} archetypes)")
        return 0

    print(f"Found {len(cells)} cell archetypes")
    for c in cells:
        print(f"\n0x{c['instance'][2:].upper()}  name={c['name']!r} display={c['display']!r} "
              f"template=0x{c['template']:08X} parts={c['part_count']}")
        for name, _off, _kind in SCALARS[1:]:
            print(f"    {name:20s} {c[name]}")
        for name, _off in AI:
            v = c["ai"][name]
            if isinstance(v, float):
                print(f"    ai.{name:22s} {v:.4f}")
            elif v:
                print(f"    ai.{name:22s} {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
