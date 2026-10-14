"""What do Spore_EP1_Locale_01/02.package actually contain?

My hypothesis was that the missing EP1 caption tables ship in those two packages and that our locale
loader simply never opened them (a third instance of this session's "the data does not exist" class).
tools/find_ep1_locale.py falsified it: across all 57 packages, `locale` resources appear in exactly the
15 Data/Locale/<lang>/Text.package files and nowhere else. So before concluding either way, look at
what those two packages actually hold - and at whether the missing table ids appear as ANY resource
instance, in any type, in the whole tree.
"""
import sys, struct
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from pathlib import Path

TYPE_NAMES = {}
import re
pat = re.compile(r"\{\s*(0x[0-9A-Fa-f]+)u?\s*,\s*\"([^\"]+)\"\s*\}")
for m in pat.finditer(open(r"E:\_DPT_Projects_E\_Harness\OpenSpore\source\engine\typeids.inc",
                           encoding="utf-8").read()):
    TYPE_NAMES[int(m.group(1), 16)] = m.group(2)

MISSING = {0xC4F86AE0: "EP1_Headers", 0x3DADE1BB: "GGETextEP1", 0x8B1A2341: "EP1_Buttons",
           0xEA78CFB9: "?", 0x277DEE6A: "mvj", 0xF3108302: "EP1_Tooltips", 0xEA25B547: "?",
           0xEFDB68EC: "ScenarioBuildMode", 0xADB810C4: "legal", 0x96071085: "?"}

root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
all_inst = defaultdict(Counter)          # instance -> {type: count}
group_hits = defaultdict(Counter)        # group   -> {type: count}

for p in sorted(root.rglob("*.package")):
    raw = p.read_bytes()
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT:OFF_COUNT + 4])[0]
    use = struct.unpack("<I", h[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", h[OFF_ALT:OFF_ALT + 4])[0]
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        continue
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]; q += 1
    rel = p.relative_to(root)
    tc = Counter()
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        tc[t] += 1
        all_inst[i][t] += 1
        group_hits[g][t] += 1
    if "Locale" in str(rel) or "locale" in rel.name.lower():
        print("=== %s : %d records ===" % (rel, count))
        for t, c in tc.most_common(12):
            print("     %-24s 0x%08X  %5d" % (TYPE_NAMES.get(t, "?"), t, c))
        print()

print("=== do the missing table ids appear as ANY resource INSTANCE anywhere? ===")
for i, nm in sorted(MISSING.items()):
    hits = all_inst.get(i)
    if hits:
        print("   0x%08X %-20s IS an instance of: %s" % (
            i, nm, ", ".join("%s(0x%08X) x%d" % (TYPE_NAMES.get(t, "?"), t, c) for t, c in hits.most_common(3))))
    else:
        print("   0x%08X %-20s absent as an instance" % (i, nm))

print()
print("=== ...or as a GROUP? (a locale table's text could be filed by group, not instance) ===")
for i, nm in sorted(MISSING.items()):
    hits = group_hits.get(i)
    if hits:
        print("   0x%08X %-20s IS a group holding: %s" % (
            i, nm, ", ".join("%s x%d" % (TYPE_NAMES.get(t, "?"), c) for t, c in hits.most_common(3))))
    else:
        print("   0x%08X %-20s absent as a group too" % (i, nm))
