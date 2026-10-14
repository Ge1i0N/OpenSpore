"""Is the layout a state script NAMES also a `spui` resource we can already read?

tools/id_role.py inverted fnv1_lower("E3LaunchScreen") to 0xE932BD44 and found it as a real resource
INSTANCE. If that resource's TYPE is `spui` (0x250FE9A2) then the chain is closed end to end:

    state script name -> fnv1_lower -> instance -> the very bytes spui::Reader already parses

which is the difference between "we render the biggest layout we found" (our choice) and "the state
machine chose this screen" (the game's choice). If the type is something else, the name hashes into a
DIFFERENT namespace and `uilayout -load` is loading a wrapper, not the layout.

So report, for every state-script UI name we can think of: its hash, which container(s) carry that
instance, and what TYPE those records have. Cross-check against the set of instances our spui reader
actually parses, and against the two unexplained `uilayout` arguments (0x027996d8, 0x0279bf9e), which
are NOT resource ids at all and so live in some inner namespace - most likely the record ids inside a
spui stream, which is exactly what this tool can also test.
"""
import sys, struct
from collections import defaultdict, Counter
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

PRIME = 0x01000193
SPUI, XML, LOCALE, TRIGGER, PACKAGE = (0x250FE9A2, 0xCF6C21B8, 0x02FAC0B6, 0x024A0E52, 0x06EFC6AA)
TYPES = {SPUI: "spui", XML: "xml", LOCALE: "locale", TRIGGER: "trigger", PACKAGE: "package"}
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")


def fnv1_lower(s):
    h = 0x811C9DC5
    for c in s.lower():
        h = ((h * PRIME) & 0xFFFFFFFF) ^ ord(c)
    return h


NAMES = ["E3LaunchScreen", "LaunchScreen", "DevLauncher", "Main", "GalaxyGameEntry",
         "SpaceGame", "CreatureGame", "TribeGame", "VehicleEditor", "E3SplashScreen",
         "UILaunchScreen", "SporeLaunchScreen", "MainMenu", "E3MainMenu", "GalaxyMap",
         "UIWorld_Loading", "LoadingScreen", "StarSelection", "PickAStar"]
want = {fnv1_lower(n): n for n in NAMES}
ARGS = {0x027996D8: "uilayout arg1", 0x0279BF9E: "uilayout arg2"}


def index(raw):
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
               w[fld["instance"]] if "instance" in fld else 0, (raw, w, fld))


def payload(meta):
    raw, w, fld = meta
    off = w[fld["offset"]]
    d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
    if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
        try:
            d = decode(d, w[fld["usize"]])
        except Exception:
            pass
    return d


spui_instances = defaultdict(list)   # instance -> [(container, size)]
byname = defaultdict(list)
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, g, i, meta in index(raw):
            if t == SPUI:
                spui_instances[i].append((p.name, meta[1][meta[2]["size"]] & 0x7FFFFFFF))
            if i in want:
                byname[want[i]].append((t, TYPES.get(t, "0x%08X" % t), p.name))

print("=== the state-script names, hashed and looked up as a resource INSTANCE ===")
print("  %-22s %-11s %s" % ("name", "instance", "carried as"))
for n in NAMES:
    h = fnv1_lower(n)
    rows = byname.get(n, [])
    if not rows:
        print("  %-22s 0x%08X  -" % (n, h))
        continue
    tally = Counter(r[1] for r in rows)
    print("  %-22s 0x%08X  %s" % (n, h, ", ".join("%s x%d" % (k, v) for k, v in tally.most_common())))
    for t, tn, cn in rows[:6]:
        inset = "  <-- IS a spui layout we parse" if t == SPUI else ""
        print("      %-9s in %-30s size=%s%s" % (tn, cn[:30],
              next((str(s) for c, s in spui_instances.get(h, []) if c == cn), "?"), inset))

print()
print("=== E3LaunchScreen specifically ===")
h = fnv1_lower("E3LaunchScreen")
print("  hash 0x%08X ; spui instances matching: %s" % (h, spui_instances.get(h, "NONE")))
print("  total distinct spui instances in the corpus: %d" % len(spui_instances))

print()
print("=== are the two unexplained uilayout args BYTES inside a spui stream? ===")
# The records are BYTE-packed (id(4) | code:u16 | marker:u16 | payload), so a 4-byte-aligned scan
# would miss them; search every offset. bytes.find is C-speed, which matters because the spui corpus
# is tens of megabytes.
streams = 0
blob = bytearray()
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, g, i, meta in index(raw):
            if t != SPUI:
                continue
            d = payload(meta)
            streams += 1
            blob += d
            blob += b"\x00" * 8  # a seam so a match cannot straddle two layouts
print("  spui streams concatenated: %d, total %s bytes" % (streams, format(len(blob), ",")))
for a, label in ARGS.items():
    needle = struct.pack("<I", a)
    n, at, first = 0, 0, -1
    while True:
        at = blob.find(needle, at)
        if at < 0:
            break
        if first < 0:
            first = at
        n += 1
        at += 1
    print("  0x%08X  %-14s : %d occurrence(s) in the spui corpus%s"
          % (a, label, n, (" (first at byte %d)" % first) if n else ""))
