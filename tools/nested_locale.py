"""The EP1 locale text is inside NESTED packages (type `package` 0x06EFC6AA) that we never open.

tools/what_in_ep1_locale.py established:
  * `locale` resources exist in exactly the 15 Data/Locale/<lang>/Text.package files and nowhere else;
  * the 10 missing caption tables appear as NO resource instance AND as no group anywhere in the tree;
  * but DataEP1/Spore_EP1_Locale_01.package holds 17 records of type **`package` (0x06EFC6AA)**, and
    _02 holds 4. That is a DBPF containing embedded DBPFs - a container our loaders do not recurse into.

So this is the THIRD variant of the session's recurring error, and a new one: not a wrong directory
(exe_dir/Locale vs Data/Locale), not a wrong id field (group vs instance), but a wrong CONTAINER
DEPTH. The data is in the tree; the tree is deeper than we look.

This script opens the nested packages and looks for the missing tables. If they are there, the engine
gains a real capability (recursive package mount) and ~285 caption references start resolving.
"""
import sys, struct, re, io
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

LOCALE, PKG_TYPE = 0x02FAC0B6, 0x06EFC6AA
MISSING = {0xC4F86AE0: "EP1_Headers", 0x3DADE1BB: "GGETextEP1", 0x8B1A2341: "EP1_Buttons",
           0xEA78CFB9: "?", 0x277DEE6A: "mvj", 0xF3108302: "EP1_Tooltips", 0xEA25B547: "?",
           0xEFDB68EC: "ScenarioBuildMode", 0xADB810C4: "legal", 0x96071085: "?"}


def records(raw):
    """Yield (type, group, instance, payload_bytes) from a DBPF image."""
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT:OFF_COUNT + 4])[0]
    use = struct.unpack("<I", h[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", h[OFF_ALT:OFF_ALT + 4])[0]
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        return
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]; q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        off = w[fld["offset"]]
        packed = w[fld["size"]] & 0x7FFFFFFF
        d = raw[off:off + packed]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                pass
        yield t, g, i, d


root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
targets = [root / "DataEP1" / "Spore_EP1_Locale_01.package",
           root / "DataEP1" / "Spore_EP1_Locale_02.package"]

found = {}
nested_types = Counter()
depth2 = 0
for tp in targets:
    raw = tp.read_bytes()
    print("=== %s (%.1f MB) ===" % (tp.name, len(raw) / 1e6))
    for t, g, i, d in records(raw):
        if t != PKG_TYPE:
            continue
        # Is the payload itself a DBPF?
        if len(d) < 0x60 or d[:4] != b"DBPF":
            print("   nested inst=0x%08X %6dB  NOT a DBPF (starts %s)" % (i, len(d), d[:8].hex(" ")))
            continue
        print("   nested inst=0x%08X %6dB  IS a DBPF -> opening" % (i, len(d)))
        for t2, g2, i2, d2 in records(d):
            nested_types[t2] += 1
            if t2 == LOCALE:
                txt = d2.decode("utf-8", "replace")
                if txt.startswith("\ufeff"):
                    txt = txt[1:]
                keys = {}
                for line in txt.replace("\r\n", "\n").split("\n"):
                    m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
                    if not m:
                        continue
                    body = m.group(2)
                    mk = re.match(r"^~([^~]*)~[ \t]*", body)
                    keys[int(m.group(1), 16)] = (body[mk.end():] if mk else body).strip()
                found[i2] = (tp.name, i, "depth2", len(keys), keys)
            # one more level, in case a nested package nests again
            if t2 == PKG_TYPE and len(d2) >= 0x60 and d2[:4] == b"DBPF":
                depth2 += 1
                for t3, g3, i3, d3 in records(d2):
                    nested_types[("depth3", t3)] += 1
                    if t3 == LOCALE:
                        found[i3] = (tp.name, i, "depth3", 0, {})
    print()

print("=== resource types inside the nested EP1 locale packages ===")
TYPE_NAMES = {}
pat = re.compile(r"\{\s*(0x[0-9A-Fa-f]+)u?\s*,\s*\"([^\"]+)\"\s*\}")
for m in pat.finditer(open(r"E:\_DPT_Projects_E\_Harness\OpenSpore\source\engine\typeids.inc",
                           encoding="utf-8").read()):
    TYPE_NAMES[int(m.group(1), 16)] = m.group(2)
for t, c in nested_types.most_common(14):
    nm = TYPE_NAMES.get(t, "?") if not isinstance(t, tuple) else "?"
    print("   %-26s 0x%08X  %5d" % (nm, t if not isinstance(t, tuple) else 0, c))
print("   depth-3 packages seen: %d" % depth2)

print()
print("=== are the missing caption tables in there? ===")
for t, nm in sorted(MISSING.items()):
    if t in found:
        pkg, ni, where, nk, keys = found[t]
        print("   0x%08X %-20s FOUND (%s, nested 0x%08X %s, %d keys)" % (t, nm, pkg, ni, where, nk))
        for k, v in list(keys.items())[:5]:
            print("        0x%08X -> %r" % (k, v[:50]))
    else:
        print("   0x%08X %-20s still absent" % (t, nm))
print()
print("recovered: %d of %d" % (len([t for t in MISSING if t in found]), len(MISSING)))
