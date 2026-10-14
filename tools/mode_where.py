"""What IS a `mode`? Test both places it can live: the shipped DATA and the binary's own strings.

The recovered state script fires 17 distinct modes across 60 states, and our engine logs every one as
"not executed (the engine's mode table is not implemented)". Before reversing the mode SYSTEM, settle
where the mode TABLE's data is, because the answer decides the whole approach:

  * if a mode name hashes (fnv1_lower) to a resource instance/group in the corpus, the modes are
    DATA in a package - probably an ArgScript or a trigger - and we can read them the way we read
    SporeAppStates;
  * if the mode names appear as STRINGS in the binary, the modes are REGISTERED CLASSES (an AppMode
    table in .rdata), and the mode verb is a lookup by name into a C++ registry - which means the
    disassembly of the registrar is the route, and there is no data file to parse.

Both tests are cheap and index-only. The mode names come from the script itself, plus the 60 state
names as a control group: if state names hit and mode names miss, that difference is itself the
finding.
"""
import sys, struct
from collections import defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from pathlib import Path

PRIME = 0x01000193
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
ORACLE = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe")

MODES = ["AnimViewer", "Editors", "EffectsViewer", "GalaxyGameEntry", "GameCinematics", "Game_Cell",
         "Game_Civ", "Game_Creature", "Game_EditMode", "Game_Space", "Game_Tribe", "Launcher",
         "LoadGame", "ModelViewer", "MultiverseJournal", "ScenarioMode", "TerrainEditor"]

# The 60 state names, read back out of the recovered script so this file cannot drift from the data.
STATES = []
script = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\SporeAppStates.txt")
for line in script.read_text(encoding="utf-8", errors="replace").splitlines():
    t = line.strip()
    if t.startswith("state "):
        STATES.append(t.split()[1])

# Names that look like a mode but are spelled as a STATE's `mode` argument in the script's own words.
NAMES = {}
for m in MODES:
    NAMES.setdefault(m, []).append(("mode", m))
for s in STATES:
    NAMES.setdefault(s, []).append(("state", s))


def fnv1_lower(s):
    h = 0x811C9DC5
    for c in s.lower():
        h = ((h * PRIME) & 0xFFFFFFFF) ^ ord(c)
    return h


want = {}
for nm, kinds in NAMES.items():
    want.setdefault(fnv1_lower(nm), nm)


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
               w[fld["instance"]] if "instance" in fld else 0)


hits = defaultdict(lambda: defaultdict(int))
seen_files = set()
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        k = str(p.resolve())
        if k in seen_files:
            continue          # the double-visit trap: Data=<exe>\Data AND cwd=<exe>
        seen_files.add(k)
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, g, i in index(raw):
            for field, val in (("type", t), ("group", g), ("instance", i)):
                if val in want:
                    hits[want[val]][field] += 1

print("modes: %d, states: %d, distinct names tested: %d" % (len(MODES), len(STATES), len(want)))
print("containers scanned (canonical-deduped): %d" % len(seen_files))
print()
print("=== TEST 1: does a mode/state NAME hash to a resource id in the corpus? ===")
mode_hits, state_hits = [], []
for nm in MODES:
    if nm in hits:
        mode_hits.append(nm)
        print("  MODE  %-20s %s" % (nm, ", ".join("%s x%d" % kv for kv in sorted(hits[nm].items()))))
for nm in STATES:
    if nm in hits:
        state_hits.append(nm)
print("  modes whose name hashes to a resource id : %d / %d" % (len(mode_hits), len(MODES)))
print("  states whose name hashes to a resource id: %d / %d  (%s)"
      % (len(state_hits), len(STATES), ", ".join(state_hits) or "none"))
print("  (for reference: a state name hashing to a LOCALE TABLE is the known case - GalaxyGameEntry,")
print("   SpaceGame, CreatureGame, TribeGame. Anything else would have shown up here.)")
print()

print("=== TEST 2: do the mode names exist as STRINGS in the binary? ===")
blob = ORACLE.read_bytes()
print("  oracle: %s bytes" % format(len(blob), ","))
for nm in MODES:
    for enc, label in ((nm.encode("utf-16-le"), "utf16"), (nm.encode("latin-1"), "ascii")):
        n = blob.count(enc)
        if n:
            at = blob.find(enc)
            print("  %-20s %-6s %d occurrence(s), first at file offset 0x%X" % (nm, label, n, at))
            break
    else:
        print("  %-20s -      ABSENT from the binary's bytes" % nm)
print()
print("=== control: are the STATE names in the binary? (they should be, the script is data) ===")
for nm in ["DevLauncher", "GalaxyGameEntry", "TribalAccessoriesEditor", "EffectsViewer"]:
    present = nm.encode("latin-1") in blob
    print("  %-26s ascii in the binary: %s" % (nm, present))
