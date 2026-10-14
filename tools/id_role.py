"""What ARE 0x027996d8 and 0x0279bf9e? Identify an unknown id by ROLE, across the whole corpus.

The recovered state script carries exactly one UI verb:

    state DevLauncher -id 1
       cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"

`E3LaunchScreen` is a name the engine cannot use until it knows what the two ids mean - so this is
the seam between "the state machine drives the UI" and "we render the biggest layout we found".

The question is therefore not "find this id" but "what ROLE does this id play". A DBPF index has
three id fields (type, group, instance), and an id's field of residence tells you what it is:
  * in TYPE    -> it is a resource CLASS (then look it up in the type registry);
  * in GROUP   -> it is a collection/section id;
  * in INSTANCE-> it names one resource inside its type.
So: scan every container's INDEX (no payload decoding - that is what made the first version of this
session's tools time out) and report, for each id of interest, every field it appears in.

Also invert the NAME: if `fnv1_lower("E3LaunchScreen")` equals one of them, the id is just the name
hashed, like every other instance we have met, and the layout can be found by name alone.
"""
import sys, struct
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from pathlib import Path

PRIME = 0x01000193
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")

INTEREST = {
    0x027996D8: "uilayout arg 1",
    0x0279BF9E: "uilayout arg 2",
}
NAMES = ["E3LaunchScreen", "LaunchScreen", "DevLauncher", "Main", "GalaxyGameEntry",
         "SporeAppStates", "UILayout", "spui", "E3SplashScreen", "SplashScreen"]
for nm in NAMES:
    h = 0x811C9DC5
    for c in nm.lower():
        h = ((h * PRIME) & 0xFFFFFFFF) ^ ord(c)
    INTEREST.setdefault(h, "fnv1_lower(%r)" % nm)

by_hash = {v: k for k, v in INTEREST.items()}


def index_fields(raw):
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
            shared[nm] = pw[q]
            q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        yield (shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0),
               shared.get("group", w[fld["group"]] if "group" in fld else 0),
               w[fld["instance"]] if "instance" in fld else 0)


hits = defaultdict(Counter)          # id -> Counter(field -> n)
where = defaultdict(set)             # id -> set of (container, field)
total = 0
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, g, i in index_fields(raw):
            total += 1
            for field, val in (("type", t), ("group", g), ("instance", i)):
                if val in INTEREST:
                    hits[val][field] += 1
                    where[val].add((p.name, field))

print("records scanned (index only): %d" % total)
print()
print("=== the ids of interest ===")
for v, label in sorted(INTEREST.items()):
    if v not in hits:
        print("  0x%08X  %-24s NOT in any index field of any container" % (v, label))
        continue
    fields = ", ".join("%s x%d" % (f, n) for f, n in sorted(hits[v].items()))
    print("  0x%08X  %-24s %s" % (v, label, fields))
    for cn, f in sorted(where[v])[:8]:
        print("        %-40s as %s" % (cn[:40], f))
print()
print("=== so: is a uilayout arg the layout's own instance, or something else? ===")
for v in (0x027996D8, 0x0279BF9E):
    if v in hits:
        print("  0x%08X lives in %s" % (v, ", ".join(sorted(hits[v]))))
    else:
        print("  0x%08X is NOT a resource id at all - it is an id inside some other namespace" % v)
