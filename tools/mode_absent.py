"""The 6 mode names that are NOT standalone strings in the binary - are they substrings?

tools/mode_where.py established the shape of the mode system: 11 of the 17 mode names exist as
standalone ASCII strings in the oracle and each is referenced EXACTLY ONCE, from a function that
begins ~0x2f bytes before the reference - the signature of a per-class factory registering itself by
name. The remaining 6 (AnimViewer, EffectsViewer, Launcher, ModelViewer, MultiverseJournal, and
`Editors` which appears only as UTF-16) are absent as standalone strings.

Two very different readings of that absence, and they must not be confused:
  * the name is a SUBSTRING of a longer literal ("AppModeEffectsViewer", "EffectsViewerMode", or a
    format string), so the mode IS a registered class and our exact-match test was too strict; or
  * the mode is not a class at all - it is a dev/viewer alias resolved some other way, or it exists
    only in a build we do not have.
The first is a hole in our TEST, the second is a fact about the game. Search substrings before
believing the absence.

Every hit is reported with its byte offset and the surrounding printable context, because a hit inside
some unrelated literal ("...Launcher..." in a comment or a file path) must not be counted as a class
registration.
"""
import re
import sys
from pathlib import Path

ORACLE = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe")
ABSENT = ["AnimViewer", "EffectsViewer", "Launcher", "ModelViewer", "MultiverseJournal", "Editors"]
PRESENT = ["Game_Creature", "Game_Space", "Game_Tribe", "Game_Civ", "Game_Cell", "Game_EditMode",
           "GalaxyGameEntry", "GameCinematics", "LoadGame", "ScenarioMode", "TerrainEditor"]

blob = ORACLE.read_bytes()
print("oracle: %s bytes" % format(len(blob), ","))
print()
print("=== the 6 names absent as STANDALONE strings: do they appear as substrings? ===")
for nm in ABSENT:
    for enc in ("ascii", "utf16"):
        needle = nm.encode("latin-1", "replace") if enc == "ascii" else nm.encode("utf-16-le")
        hits = [m.start() for m in re.finditer(re.escape(needle), blob)]
        if not hits:
            continue
        print("  %-20s %s: %d occurrence(s)" % (nm, enc, len(hits)))
        for h in hits[:6]:
            lo, hi = max(0, h - 40), min(len(blob), h + len(needle) + 40)
            ctx = blob[lo:hi]
            txt = "".join(chr(c) if 32 <= c < 127 else "." for c in ctx)
            print("     @0x%08X  ...%s..." % (h, txt))
        break
    else:
        print("  %-20s : NOWHERE, not even as a substring - the mode is not a named class" % nm)
print()
print("=== control: the 11 that ARE standalone, and how many times each occurs ===")
for nm in PRESENT:
    needle = nm.encode("latin-1")
    print("  %-20s %d occurrence(s)" % (nm, len(re.findall(re.escape(needle), blob))))
print()
print("=== is there an AppMode class-name prefix at all? ===")
for pref in ["AppMode", "ModeGame", "GameMode", "CMode", "SPORE_MODE"]:
    n = len(re.findall(re.escape(pref.encode("latin-1")), blob))
    if n:
        at = blob.find(pref.encode("latin-1"))
        lo, hi = max(0, at - 20), min(len(blob), at + 60)
        txt = "".join(chr(c) if 32 <= c < 127 else "." for c in blob[lo:hi])
        print("  %-10s %5d occurrence(s), first @0x%08X ...%s..." % (pref, n, at, txt))
    else:
        print("  %-10s     0" % pref)
