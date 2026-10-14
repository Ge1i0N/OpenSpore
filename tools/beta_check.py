"""Is the beta build usable, and does it carry the mode names the GA build lacks?

The user supplied refs/bins/Beta190208_Also/SporeApp.exe (32,616,448 B, dated 2008-02-19 - the build
the Beta190208 PDB belongs to). Two questions, and they are DIFFERENT questions:

  1. IS THE CODE READABLE? The retail GA SporeApp.exe is SecuROM-encrypted and must never be
     disassembled; this beta may or may not be. Shannon entropy per PE section answers it cheaply: an
     encrypted or compressed .text sits near 8.0 bits/byte, ordinary x86 sits around 6.0-6.6, and
     zero-filled or mostly-ASCII sections much lower. Report every section so the claim is not
     "probably fine".

  2. DOES IT CARRY THE NAMES WE ARE MISSING? The GA oracle contains 10 of the 17 modes the shipped
     script asks for; five appear nowhere in it at all (EffectsViewer, Launcher, MultiverseJournal,
     AnimViewer, ModelViewer) and two exist but their registration was not located (TerrainEditor,
     Editors). A beta is EXACTLY where cut or renamed features would still be present, so search this
     binary for all seven, in ASCII and UTF-16, and report every hit with its offset.

WHAT THIS MUST NOT BE USED FOR: addresses. Project rule - the reversal oracle is the GA
`SporeApp.exe.unpacked.exe` only, and addresses from a beta build (like the Beta190208 PDB) are not
the target's. This script deliberately reports only FILE OFFSETS and names, never a VA.
"""
import math
import struct
import sys
from collections import Counter
from pathlib import Path

BETA = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\refs\bins\Beta190208_Also\SporeApp.exe")
GA = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\refs\bins\ReleaseGA\SporeApp.exe")

MISSING = ["EffectsViewer", "Launcher", "MultiverseJournal", "AnimViewer", "ModelViewer",
           "TerrainEditor", "Editors"]
PRESENT_IN_GA = ["Game_Creature", "Game_Space", "GalaxyGameEntry", "ScenarioMode", "LoadGame"]


def entropy(b):
    if not b:
        return 0.0
    c = Counter(b)
    n = len(b)
    return -sum((v / n) * math.log2(v / n) for v in c.values())


def sections(p):
    """Parse the PE section table by hand - no dependencies, and it keeps the output auditable."""
    data = p.read_bytes()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b"PE\0\0", "not a PE"
    nsec = struct.unpack_from("<H", data, e_lfanew + 6)[0]
    opt = e_lfanew + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    opt_size = struct.unpack_from("<H", data, e_lfanew + 20)[0]
    sec_off = opt + opt_size
    out = []
    for i in range(nsec):
        o = sec_off + i * 40
        name = data[o:o + 8].rstrip(b"\0").decode("latin-1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, o + 8)
        out.append((name, vaddr, vsize, rawptr, rawsize, data[rawptr:rawptr + rawsize]))
    return data, out


for label, path in (("BETA 2008-02-19", BETA), ("GA (oracle's source)", GA)):
    if not path.exists():
        print("%s: NOT PRESENT (%s)" % (label, path))
        continue
    data, secs = sections(path)
    print("=== %s  %s  %s bytes ===" % (label, path.name, format(len(data), ",")))
    print("  %-10s %10s %10s %10s   %s" % ("section", "virt", "rawsize", "entropy", "read"))
    for name, vaddr, vsize, rawptr, rawsize, body in secs:
        e = entropy(body)
        verdict = ("ENCRYPTED/COMPRESSED (>=7.5)" if e >= 7.5 else
                   "ordinary code/data" if e >= 4.5 else "sparse/padding")
        print("  %-10s 0x%08X %10d %10.4f   %s" % (name, vaddr, rawsize, e, verdict))
    print()

print("=== do the SEVEN names the GA oracle lacks exist in the beta? ===")
if BETA.exists():
    bdata = BETA.read_bytes()
    gdata = GA.read_bytes() if GA.exists() else b""
    _, bsecs = sections(BETA)
    # Map a FILE offset to its containing section, so a hit can be classified: a mode NAME literal
    # lives in .rdata next to the other registration strings, whereas the same word inside .text is
    # code (an immediate) and inside .rsrc is unrelated resource text. Without this, "the name is
    # present" would not distinguish a registration from a coincidence.
    def section_of(off):
        for name, vaddr, vsize, rawptr, rawsize, _b in bsecs:
            if rawptr <= off < rawptr + rawsize:
                return name
        return "?"

    for nm in MISSING:
        rows = []
        for enc, tag in (("latin-1", "ascii"), ("utf-16-le", "utf16")):
            needle = nm.encode(enc)
            hits, i = [], bdata.find(needle)
            while i >= 0 and len(hits) < 4:
                hits.append(i)
                i = bdata.find(needle, i + 1)
            if hits:
                rows.append("%s x%d in %s" % (tag, len(hits),
                                              ",".join(sorted({section_of(h) for h in hits}))))
        in_ga = nm.encode("latin-1") in gdata
        print("  %-18s beta: %-46s | GA: %s" % (nm, "; ".join(rows) or "ABSENT",
                                                "present" if in_ga else "absent"))
    print()
    print("=== control: names the GA oracle DOES have, do they appear in the beta too? ===")
    for nm in PRESENT_IN_GA:
        n = bdata.count(nm.encode("latin-1"))
        print("  %-18s beta occurrences: %d" % (nm, n))
else:
    print("  beta binary not found")
