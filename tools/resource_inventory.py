"""Global resource inventory: what is actually inside the game, by name.

Chain that makes this possible (all reversed in this project):
  DBPF index (kind bit mask) -> record { type, group, instance, offset, packedSize, memSize,
  compression } -> RefPack decode -> type name (reg_type.txt) -> group/instance name.

Group and instance are 32-bit hashes. They resolve because the game's hash is FNV-1 with
tolower folding (confirmed by disassembly at 0x00932e50: `imul` before `xor`, mode arg 1 =
tolower), and the community registry lists names whose ids can be recomputed. Verified on the
cs-cz NameGeneration package: 14/14 instances resolved (LexiconPeople_CS, LexiconCities_CS, ...).

Every claim here is checkable:
  * counts come from the 57 shipped packages' own indexes;
  * names come from reg_file.txt/reg_type.txt via a hash we verified against the binary;
  * unresolved ids are reported as unresolved, never guessed.

Usage: python tools/resource_inventory.py [--top 40] [--packages]
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

from tools.pkg_kinds import DIRS, HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE

REG = ROOT / "upstream" / "SporeMaster3" / "data"
FNV_P, FNV_M = 0x1000193, 0xFFFFFFFF


def fnv1_lower(s: str) -> int:
    h = 0x811C9DC5
    for ch in s:
        c = ord(ch)
        if 65 <= c <= 90:
            c += 32
        h = ((h * FNV_P) & FNV_M) ^ c
    return h


def load_reg(path: Path) -> dict[int, str]:
    reg: dict[int, str] = {}
    if not path.is_file():
        return reg
    for ln in path.read_text(encoding="utf-8", errors="replace").splitlines():
        ln = ln.rstrip()
        if not ln:
            continue
        if "\t" in ln:
            name, h = ln.split("\t", 1)
            h = h.strip()
            if h.lower().startswith("0x"):
                try:
                    reg[int(h, 16)] = name
                except ValueError:
                    pass
            continue
        reg[fnv1_lower(ln)] = ln
    return reg


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--packages", action="store_true")
    a = ap.parse_args()

    types = load_reg(REG / "reg_type.txt")
    names = load_reg(REG / "reg_file.txt")
    print(f"# registry: {len(types):,} type names, {len(names):,} group/instance names")

    files = []
    seen = set()
    for d in DIRS:
        if not d.is_dir():
            continue
        for p in sorted(d.rglob("*.package")):
            k = str(p.resolve())
            if k not in seen:
                seen.add(k)
                files.append(p)

    by_type: Counter[str] = Counter()
    by_type_id: Counter[int] = Counter()
    inst_hit = inst_miss = 0
    total = 0
    total_mem = 0
    per_pkg: list[tuple[str, int]] = []
    unknown_types: Counter[int] = Counter()
    for f in files:
        raw = f.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        if kind not in KINDS:
            continue
        prefix, stride, fld = KINDS[kind]
        if isize != prefix + stride * count:
            continue
        # The prefix is [kind][shared fields in order type,group,unknown]. A field is shared
        # when its bit is set in the kind mask, so read them in that same order.
        pwords = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
        shared = {}
        pi = 1
        for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
            if kind & bit:
                shared[nm] = pwords[pi] if pi < len(pwords) else 0
                pi += 1
        blob = raw[use + prefix : use + prefix + stride * count]
        for n in range(count):
            w = struct.unpack("<%dI" % (stride // 4), blob[n * stride : (n + 1) * stride])
            total += 1
            total_mem += w[fld["usize"]]
            if "type" in shared:
                tid = shared["type"]
            elif "typeid" in fld:
                tid = w[fld["typeid"]]
            else:
                tid = 0
            by_type_id[tid] += 1
            tn = types.get(tid)
            if tn:
                by_type[tn] += 1
            else:
                unknown_types[tid] += 1
            inst = w[fld["instance"]]
            if inst in names:
                inst_hit += 1
            else:
                inst_miss += 1
        per_pkg.append((f.name, count))

    print(f"\n# {total:,} resources across {len(files)} packages, {total_mem/1e9:.2f} GB decoded")
    print(f"# instance names resolved: {inst_hit:,}/{total:,} ({100*inst_hit/total:.1f}%)")
    print(f"# distinct types: {len(by_type_id)}; named: {len(by_type)}; unnamed ids: {len(unknown_types)}")
    print(f"\n## Top {a.top} types by resource count")
    for t, c in by_type.most_common(a.top):
        print(f"   {c:>8,}  {t}")
    if unknown_types:
        print(f"\n## Unnamed type ids (top 15)")
        for tid, c in unknown_types.most_common(15):
            print(f"   {c:>8,}  0x{tid:08x}")
    if a.packages:
        print(f"\n## Resources per package")
        for nm, c in sorted(per_pkg, key=lambda kv: -kv[1]):
            print(f"   {c:>8,}  {nm}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
