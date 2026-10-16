r"""Cell-stage game data decoder (CellGame.package + CellGame_Layers.package).

The cell mini-game ("Game_Cell", see analysis/game-stages.md) is defined entirely by data
packages, not code:

* ``Data/CellGame.package``        - 442 resources: archetypes, globals, tables
* ``Data/CellGame_Layers.package`` - backgrounds/layers

All numbers inside a resource are little-endian; DBPF headers/indexes are little-endian
(see tools/pkg_kinds.py). Inside a ``cell`` archetype a UTF-16LE display string follows a
4-byte header (e.g. ``cell_squid`` carries ``"Snorf"``).

Resource types and instance names are FNV-1 hashes (tools/spore_hash.py). This tool names
them from ``upstream/SporeMaster3/data/reg_*.txt`` plus a small embedded cell dictionary,
then decodes the structured resources:

| type | name | decoder |
|------|------|---------|
| 0x00B1B104 | prop      | property table (tools/prop.py), float2 arrays |
| 0x8C042499 | look_table| steering weights (docs/LOOKTABLE_FORMAT.md) |
| 0xD92AF091 | loottable | cell loot table (docs/LOOTTABLE_FORMAT.md) |
| 0xDFAD9F51 | cell      | archetype: display name |
| 0x2A3CE5B7 | globals   | cellgame raw float tuning |
| 0x9B8E862F | world     | tide-pool biome definition |
| 0xF9C3D770 | random_creature | random spawn table |

Usage:
    python tools/cell_game.py                # summary of CellGame.package
    python tools/cell_game.py --json out.json
    python tools/cell_game.py --globals       # named cell-globals floats
    python tools/cell_game.py --anims         # animation table (needs xlrd)
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE  # noqa: E402
from tools.refpack import decode  # noqa: E402
from tools.spore_hash import fnv1_lower, load_registry  # noqa: E402

PROP = 0x00B1B104
LOOK_TABLE = 0x8C042499
LOOTTABLE = 0xD92AF091
CELL = 0xDFAD9F51
GLOBALS = 0x2A3CE5B7
WORLD = 0x9B8E862F
RANDOM_CREATURE = 0xF9C3D770
STRUCTURE = 0x4B9EF6DC
POPULATE = 0xDA141C1B
XLS = 0x1F639D98
EFFECTMAP = 0x433FB70C
BACKGROUNDMAP = 0x612B3191
ADVECT = 0x04805684

# Extra cell names not present in reg_file.txt but confirmed on real resources.
EMBEDDED = [
    "cell", "structure", "world", "globals", "populate", "look_table", "look_algorithm",
    "random_creature", "loottable", "effectmap", "backgroundmap", "density", "advect",
    "cell_dragon", "cell_squid", "cell_chomper", "cell_megamouth", "cell_poolball",
    "cell_sleeper", "cell_parasite", "cell_jawhead", "cell_ducky", "cell_waterballoon",
    "cell_puffish", "cell_poker", "cell_spiky", "cell_spikey", "cell_buzzy", "cell_charger",
    "cell_booster", "cell_grubby", "cell_minno1", "cell_stabberalpha", "cell_weapon_poison_npc",
    "cell_eggkid", "cell_eggmom", "cell_eggdad", "cell_nosey", "cell_goldy", "cell_pinky",
    "cell_shyster", "cell_creeper", "cell_jetster", "cell_ocatmouth", "poolball", "creeper",
    "beaker", "bolt", "cute1",
]

LOOK_ENTRY = {0: "up", 1: "forward", 2: "left", 3: "right", 4: "pursue_target",
              5: "flee_target", 6: "object", 7: "food", 8: "predator", 9: "destination",
              10: "cursor", 11: "player"}
LOOT_TYPE = {0: "nothing", 1: "cell", 2: "table"}


def find_packages() -> list[Path]:
    """Locate CellGame*.package across common Spore install roots (machine-local)."""
    roots = [
        Path(os.environ.get("SPORE_DATA", "")),
        ROOT / "build" / "bin" / "Debug" / "Data",
        Path(r"S:\Games\Other\Spore\Data"),
        Path(r"S:\Games\Steam\steamapps\common\spore\Data"),
        Path(r"C:\Program Files (x86)\Origin Games\Spore\Data"),
        Path(r"C:\Program Files (x86)\EA Games\Spore\Data"),
    ]
    found: list[Path] = []
    for r in roots:
        if r and r.is_dir():
            found += [Path(p) for p in glob.glob(str(r / "CellGame*.package"))]
    return found


def iter_resources(path: Path):
    """Yield (type_id, group, instance, decoded_or_None) for every resource in a package."""
    raw = path.read_bytes()
    if raw[:4] != b"DBPF":
        return
    count = struct.unpack("<I", raw[OFF_COUNT:OFF_COUNT + 4])[0]
    isize = struct.unpack("<I", raw[OFF_ISIZE:OFF_ISIZE + 4])[0]
    use = struct.unpack("<I", raw[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", raw[OFF_ALT:OFF_ALT + 4])[0]
    if not use or use + 4 > len(raw):
        return
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        return
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]
            q += 1
    for r in range(count):
        base = use + prefix + r * stride
        if base + stride > len(raw):
            break
        w = struct.unpack("<%dI" % (stride // 4), raw[base:base + stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        off = w[fld["offset"]]
        sz = w[fld["size"]] & 0x7FFFFFFF
        data = raw[off:off + sz]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                data = decode(data, w[fld["usize"]])
            except Exception:  # noqa: BLE001
                data = None
        yield t, g, i, data


def decode_prop(data: bytes):
    """Property table: count(BE) then {id(BE u32), type+flags(BE u32), itemCount, itemSize}."""
    from tools.prop import parse
    entries, consumed, why = parse(data)
    out = []
    for key, typ, icnt, isz, off in entries:
        vec = []
        if typ == 0x0030009C and isz == 8:  # float2 array
            for k in range(icnt):
                x, y = struct.unpack_from("<2f", data, off + k * 8)
                vec.append((round(x, 5), round(y, 5)))
        out.append({"key": key, "type": typ, "count": icnt, "size": isz, "floats": vec})
    return out, why


def decode_look_table(data: bytes):
    off, cnt = struct.unpack_from("<2i", data, 0)
    rows = []
    for k in range(cnt):
        t, v = struct.unpack_from("<if", data, off + k * 8)
        rows.append((LOOK_ENTRY.get(t, t), round(v, 4)))
    return rows


def decode_loottable(data: bytes, names: dict[int, str] | None = None):
    names = names or {}
    off, cnt = struct.unpack_from("<2i", data, 0)
    minr, maxr, alpha, force = struct.unpack_from("<4f", data, 8)
    effect, = struct.unpack_from("<I", data, 24)
    delay, = struct.unpack_from("<f", data, 32)
    rows = []
    for k in range(cnt):
        t, cid, tid, w, c, cd, lo = struct.unpack_from("<3If3i", data, off + k * 28)
        rows.append({"type": LOOT_TYPE.get(t, t),
                     "cell": names.get(cid, "0x%08x" % cid),
                     "table": names.get(tid, "0x%08x" % tid) if tid else None,
                     "weight": round(w, 4), "count": c, "countDelta": cd, "levelOffset": lo})
    return {"minRadius": minr, "maxRadius": maxr, "initialAlpha": alpha,
            "expelForce": force, "effect": names.get(effect, "0x%08x" % effect),
            "delay": delay, "entries": rows}


def display_name(data: bytes) -> str:
    """First UTF-16LE printable run in a cell archetype (the creature's display name)."""
    i = 4
    s = ""
    while i + 1 < len(data):
        ch = struct.unpack_from("<H", data, i)[0]
        if 32 <= ch < 127:
            s += chr(ch)
            i += 2
        elif ch == 0 and s:
            break
        else:
            if s:
                break
            i += 2
    return s


def build_name_map() -> dict[int, str]:
    m = {fnv1_lower(n): n for n in EMBEDDED}
    m.update(load_registry())
    return m


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", type=Path)
    ap.add_argument("--globals", action="store_true", help="print named cell-globals floats")
    ap.add_argument("--anims", action="store_true", help="print the animation table")
    a = ap.parse_args()

    pkgs = find_packages()
    if not pkgs:
        print("# CellGame.package not found (set SPORE_DATA to the Data dir)", file=sys.stderr)
        return 1
    names = build_name_map()
    report: dict = {"packages": [str(p) for p in pkgs], "types": {}, "prop": {},
                    "look_tables": {}, "loottables": {}, "cells": {}, "world": {},
                    "random_creature": {}}

    for pkg in pkgs:
        for t, g, i, data in iter_resources(pkg):
            tname = names.get(t, "0x%08x" % t)
            report["types"][tname] = report["types"].get(tname, 0) + 1
            iname = names.get(i, None)
            if data is None:
                continue
            if t == PROP:
                key = "0x%08x" % i
                entries, why = decode_prop(data)
                report["prop"][key] = entries
                if a.globals and why is None:
                    print("== prop %s (%s) ==" % (key, names.get(i, "?")))
                    for e in entries:
                        nm = names.get(e["key"], "0x%08x" % e["key"])
                        print("   %-28s %s" % (nm, e["floats"]))
            elif t == LOOK_TABLE:
                report["look_tables"][iname or "0x%08x" % i] = decode_look_table(data)
            elif t == LOOTTABLE:
                report["loottables"][iname or "0x%08x" % i] = decode_loottable(data, names)
            elif t == CELL:
                report["cells"][iname or "0x%08x" % i] = {
                    "instance": "0x%08x" % i, "display": display_name(data), "size": len(data)}
            elif t == WORLD:
                report["world"][iname or "0x%08x" % i] = {"size": len(data)}
            elif t == RANDOM_CREATURE:
                report["random_creature"][iname or "0x%08x" % i] = {"size": len(data)}

    # summary
    print("# CellGame packages: %d" % len(pkgs))
    print("# resource types:")
    for k, v in sorted(report["types"].items(), key=lambda x: -x[1]):
        print("   %-22s %d" % (k, v))
    print("# cell archetypes: %d (%d named)" % (
        len(report["cells"]),
        sum(1 for k in report["cells"] if not k.startswith("0x"))))
    print("# look_tables: %d   loottables: %d   worlds: %d   random_creature: %d" % (
        len(report["look_tables"]), len(report["loottables"]),
        len(report["world"]), len(report["random_creature"])))

    if a.anims:
        for pkg in pkgs:
            for t, g, i, data in iter_resources(pkg):
                if t == XLS and data and data[:8] == b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
                    try:
                        import io
                        import xlrd
                        book = xlrd.open_workbook(file_contents=data)
                        for sh in book.sheets():
                            print("== sheet %r (%dx%d) ==" % (sh.name, sh.nrows, sh.ncols))
                            for r in range(sh.nrows):
                                cells = [sh.cell_value(r, c) for c in range(sh.ncols)]
                                if any(str(c) for c in cells):
                                    print("  %s" % cells)
                    except ImportError:
                        print("# (install xlrd to dump the animation table)", file=sys.stderr)
                    break

    if a.json:
        a.json.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print("# wrote %s" % a.json)
    return 0


if __name__ == "__main__":
    sys.exit(main())
