"""Is the 'missing' EP1 locale text actually in our tree, just outside the paths we search?

The caption references that failed to resolve are 96% tables whose INSTANCE id is absent from the
locale resources our engine loads (analysis: tools/spui_caption_gaps.py + classify_287.py), and the
registry names them: 'EP1_Headers', 'GGETextEP1', 'EP1_Buttons', 'EP1_Tooltips', 'legal',
'ScenarioBuildMode', 'mvj'. Every one of those ids is fnv1_lower(its name), so a locale table instance
IS the hash of its name - which also means the naming we already have can label any table.

Meanwhile the disk shows DataEP1/Spore_EP1_Locale_01.package and _02.package, i.e. EP1 locale content
ships as LOOSE PACKAGES in DataEP1, not under a DataEP1/Locale/<lang>/ directory. Our text.cpp filters
search paths by tag "Locale", so it never opens them. That is the third instance of this session's
recurring error class ("the data does not exist" == "I looked in the wrong place"): after
exe_dir/Locale vs Data/Locale, and after SporeAppStates as a group vs an instance.

This script tests the hypothesis directly: open those two packages and look for the missing tables.
"""
import sys, struct, re
from collections import Counter
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

LOCALE = 0x02FAC0B6
WANT = {
    0xC4F86AE0: "EP1_Headers", 0x3DADE1BB: "GGETextEP1", 0x8B1A2341: "EP1_Buttons",
    0xEA78CFB9: "?", 0x277DEE6A: "mvj", 0xF3108302: "EP1_Tooltips", 0xEA25B547: "?",
    0xEFDB68EC: "ScenarioBuildMode", 0xADB810C4: "legal", 0x96071085: "?",
}

root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
cands = sorted(root.rglob("*.package"))
print("packages under the exe dir: %d" % len(cands))

found_tables = {}
per_pkg = Counter()
for p in cands:
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
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        if t != LOCALE:
            continue
        inst = w[fld["instance"]] if "instance" in fld else 0
        off = w[fld["offset"]]
        d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                continue
        per_pkg[str(p.relative_to(root))] += 1
        if inst in WANT:
            txt = d.decode("utf-8", "replace")
            if txt.startswith("\ufeff"):
                txt = txt[1:]
            keys = {}
            for line in txt.replace("\r\n", "\n").split("\n"):
                m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
                if not m:
                    continue
                body = m.group(2)
                mk = re.match(r"^~([^~]*)~[ \t]*", body)
                k = int(m.group(1), 16)
                keys[k] = (body[mk.end():] if mk else body).strip()
            found_tables[inst] = (p.relative_to(root), r, len(d), keys)

print()
print("=== which packages hold `locale` resources at all? ===")
for name, c in per_pkg.most_common(20):
    print("   %-46s %3d" % (name, c))

print()
print("=== are the MISSING caption tables among them? ===")
for t, nm in sorted(WANT.items(), key=lambda kv: -len(found_tables.get(kv[0], [(0,)])[2]) if kv[0] in found_tables else 999):
    if t in found_tables:
        rel, r, sz, keys = found_tables[t]
        print("   0x%08X %-20s FOUND in %-34s rec[%d] %6dB %d keys" % (t, nm, str(rel), r, sz, len(keys)))
        for k, v in list(keys.items())[:4]:
            print("        0x%08X -> %r" % (k, v[:52]))
    else:
        print("   0x%08X %-20s still absent" % (t, nm))

print()
print("recovered: %d of %d missing tables" % (len(found_tables), len(WANT)))
print("keys recovered: %d" % sum(len(v[3]) for v in found_tables.values()))
