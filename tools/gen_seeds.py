"""Derive REAL function entries for the addresses our claims cite, so Ghidra can be told to carve
them (REA's REA_GHIDRA_SEED_FILE), instead of reporting "no function there".

Why this is needed: the audit found seven cited addresses where Ghidra carved NO function at all.
They are not wrong addresses - they are functions reachable only through vtable/indirect calls, and
Ghidra's auto-analysis does not follow those. Six of the seven begin with the MSVC SEH prologue
`mov eax,fs:0x0`, which Ghidra left typed as data.

An address can also land MID-function (a call site, e.g. 0x7e40f2 `push 0x811c9dc5`). Seeding that
as a function would be wrong, so this walks back to the preceding int3 (0xCC) gap, which is where a
real entry lives in this binary.
"""
import sys, struct
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pe import PE

pe = PE()

TEXT = None
for s in pe._sec_list:
    if s["name"] == ".text":
        TEXT = s
assert TEXT

CITED = [
    ("0x007e3fb0", "statemgr add-state (vtable+0x00)"),
    ("0x007e7160", "app event dispatcher"),
    ("0x007eaae0", "AppMode +0x20 shader/script config"),
    ("0x00849bf0", "Window::Create"),
    ("0x007c7f50", "config executor setVariables"),
    ("0x007c90f0", "config executor card/vendor"),
    ("0x007e40f2", "the state-hash CALL SITE (mid-function)"),
    ("0x007c85e7", "config command registrar"),
    ("0x007c8e90", "config executor setOption"),
    ("0x007c8f00", "config executor alert"),
    ("0x0092c660", "RefPack decoder"),
    ("0x008de4b0", "resource manager request site"),
    ("0x0080fff0", "UI layout request"),
    ("0x00932e50", "the FNV hash itself"),
    ("0x007e6850", "AppMode +0x14 subsystem init"),
    ("0x007e93d0", "AppMode +0x18 plugins"),
    ("0x007e7430", "AppMode +0x1c create device"),
    ("0x007e19e0", "statemgr +0x24"),
    ("0x007ab7f0", "model manager handler +0x6c"),
    ("0x007ab050", "model manager handler +0x70"),
    ("0x007ac4d0", "model manager factory"),
    ("0x006ac3e7", "registry dispatcher (0 direct callers)"),
]


def in_text(va):
    return TEXT["va"] <= va < TEXT["va"] + TEXT["raw"]


def entry_of(va):
    """Walk back to the start of the basic block chain: the first address after an int3 gap, or the
    target of a jump/call into this region. Bounded so a pathological case cannot spin."""
    if not in_text(va):
        return None
    cur = va
    for _ in range(4096):
        if cur <= TEXT["va"]:
            return None
        b = pe.read(cur - 1, 1)
        if not b:
            return None
        # An int3 immediately before means `cur` is a boundary -> the entry is at or after it.
        if b == b"\xcc":
            # step forward past any int3 run to the first real byte
            probe = cur
            while True:
                bb = pe.read(probe, 1)
                if not bb or bb != b"\xcc":
                    return probe
                probe += 1
        cur -= 1
    return None


rows = []
for h, why in CITED:
    va = int(h, 16)
    e = entry_of(va)
    kind = "entry"
    if e is None:
        rows.append((h, None, why, "outside .text / unresolved"))
        continue
    if e == va:
        rows.append((h, e, why, "IS a function entry"))
    else:
        rows.append((h, e, why, "MID-function; real entry is %s" % ("0x%08x" % e)))

print("%-12s %-12s %-38s %s" % ("cited", "entry", "why", "verdict"))
print("-" * 118)
for h, e, why, verdict in rows:
    print("%-12s %-12s %-38s %s" % (h, ("0x%08x" % e) if e else "-", why, verdict))

# The seed file REA wants: address<TAB>kind[<TAB>name], addresses 0x-hex in the default space,
# functions seeded from the highest address down (REA's own order), so only real entries go in.
seeds = sorted({e for (_h, e, _w, _v) in rows if e is not None}, reverse=True)
out = r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\audit\seeds.tsv"
with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write("# Ghidra function seeds for the addresses OpenSpore's claims cite.\n")
    f.write("# Derived by tools/gen_seeds.py; these are functions reachable ONLY through vtable /\n")
    f.write("# indirect calls, which Ghidra's auto-analysis did not carve (the audit found 7 such\n")
    f.write("# addresses, 6 of them starting with the MSVC SEH prologue `mov eax,fs:0x0`).\n")
    f.write("# Feed to REA as REA_GHIDRA_SEED_FILE.\n")
    for s in seeds:
        f.write("0x%08x\tfunction\tos_%08x\n" % (s, s))
print("\nwrote %s with %d seeds" % (out, len(seeds)))
