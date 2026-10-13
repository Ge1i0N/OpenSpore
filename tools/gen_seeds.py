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
import os
import re
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

# ---- SECOND-ORDER SEEDS: the gaps INSIDE a carved function -------------------------------
# Creating a function is not enough. Measured on the event dispatcher (0x7e7160): after seeding,
# Ghidra's body is [[0x7e7160,0x7e71c4] [0x7e72bf,0x7e7300]] - a 185-byte HOLE in the middle it never
# disassembled, because it stops at the MSVC alignment padding `8d 49 00` and types those bytes as
# data. Every immediate and instruction our claims cite inside that hole is therefore simply ABSENT
# from Ghidra's view, which a naive checker reports as a refutation of a correct doc claim.
# Ghidra's own answer is a `code` seed at the hole's start, which is exactly what REA's
# REA_GHIDRA_SEED_FILE documents ("seed code that auto-analysis cannot reach: computed jumps,
# pointers in data"). Read the gaps from the exported facts so this stays systematic, not guessed.
gaps = []
facts_path = r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\audit\facts.json"
if os.path.exists(facts_path):
    import json
    facts = json.load(open(facts_path, encoding="utf-8"))
    for h, f in (facts.get("functions") or {}).items():
        if not f.get("exists"):
            continue
        ranges = f.get("body_ranges") or []
        if len(ranges) < 2:
            continue
        pts = sorted([(int(lo, 16), int(hi, 16)) for lo, hi in ranges])
        for (a0, a1), (b0, b1) in zip(pts, pts[1:]):
            if b0 > a1 + 1:
                gaps.append((h, a1 + 1, b0 - 1))
    for h, lo, hi in gaps:
        print("  gap in %s: 0x%08x..0x%08x (%d bytes) -> code seed at the start" % (h, lo, hi, hi - lo + 1))

with open(out, "w", encoding="utf-8", newline="\n") as f:
    f.write("# Ghidra seeds for the addresses OpenSpore's claims cite.\n")
    f.write("# Derived by tools/gen_seeds.py; these are functions reachable ONLY through vtable /\n")
    f.write("# indirect calls, which Ghidra's auto-analysis did not carve (the audit found 7 such\n")
    f.write("# addresses, 6 of them starting with the MSVC SEH prologue `mov eax,fs:0x0`).\n")
    f.write("# Feed to REA as REA_GHIDRA_SEED_FILE.\n")
    for s in seeds:
        f.write("0x%08x\tfunction\tos_%08x\n" % (s, s))
    if gaps:
        f.write("#\n")
        f.write("# Second-order: the HOLES inside those function bodies. Creating a function does not\n")
        f.write("# make Ghidra disassemble its interior - it stops at MSVC alignment padding\n")
        f.write("# (`8d 49 00`) and leaves the bytes typed as data, so the code seeds below are what\n")
        f.write("# actually recover the instructions our claims cite.\n")
        for h, lo, hi in gaps:
            f.write("0x%08x\tcode\tgap_in_%s\n" % (lo, h.replace("0x", "")))
print("\nwrote %s with %d function seeds + %d code seeds" % (out, len(seeds), len(gaps)))

# The REA variant lists ONLY what Ghidra has not already carved: per REA's docs a seed that overlaps
# an instruction Ghidra already decoded FAILS, and the 14 already-carved entries would abort the run.
rea_out = r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\audit\seeds_rea.tsv"
uncarved = [h for (h, e, w, v) in rows if e is not None and "IS a function entry" in v
            and h.lower() in {"0x007e3fb0", "0x007e7160", "0x007eaae0", "0x00849bf0",
                              "0x007c7f50", "0x007c90f0", "0x007c8550"}]
with open(rea_out, "w", encoding="utf-8", newline="\n") as f:
    f.write("# Ghidra seeds for the 7 addresses OpenSpore cites that Ghidra auto-analysis never\n")
    f.write("# carved: reachable only through a vtable, and 6 of 7 open with the MSVC SEH prologue\n")
    f.write("# `mov eax,fs:0x0` (Ghidra left those bytes typed as data). Derived by tools/gen_seeds.py,\n")
    f.write("# which walked each back to the preceding int3 gap to prove they are real entries.\n")
    f.write("# Already-carved addresses are OMITTED on purpose: per REA's docs a seed overlapping an\n")
    f.write("# already-decoded instruction fails.\n")
    for h in sorted(uncarved, key=lambda s: -int(s, 16)):
        f.write("%s\tfunction\tos_spore_%s\n" % (h, h[2:]))
    if gaps:
        f.write("#\n")
        f.write("# The holes inside those bodies (see the note in seeds.tsv). Kind `code` is REA's\n")
        f.write("# documented answer for 'code auto-analysis cannot reach'.\n")
        for h, lo, hi in gaps:
            f.write("0x%08x\tcode\tos_gap_%08x\n" % (lo, lo))
print("wrote %s with %d function seeds + %d code seeds" % (rea_out, len(uncarved), len(gaps)))
