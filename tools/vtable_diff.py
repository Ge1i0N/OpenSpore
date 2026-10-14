"""Compare mode vtables slot by slot: which methods are SHARED (the base class) and which DIFFER.

Why this and not "decompile the mode": most vtable targets are UNCARVED (the standing vtable-only wall
- Ghidra only reaches functions that something calls directly). A decompiler cannot describe a function
that does not exist, but AGREEMENT does not need the functions: if three different modes have the same
pointer in slot +0x18, that slot is inherited, and the slots where they differ are exactly the methods
each mode overrides. That is the mode interface, read from data rather than from code, and it is
available today.

    python tools/vtable_diff.py <dir-with-vt_*.txt>
"""
import io
import re
import sys
from pathlib import Path


def read_text(path):
    """Read a text file whatever PowerShell wrote. `>` in Windows PowerShell writes UTF-16LE by
    default, so a utf-8-only read sees NUL between every character and every regex silently matches
    nothing - which is exactly how this comparison first reported '0 slots' from three non-empty
    files. Try the encodings in order and take the first that decodes without NULs."""
    raw = Path(path).read_bytes()
    for enc in ("utf-8-sig", "utf-16", "utf-16-le", "latin-1"):
        try:
            txt = raw.decode(enc)
        except Exception:
            continue
        if "\x00" not in txt:
            return txt
    return raw.decode("latin-1", "replace")


def slots(path):
    out = {}
    for ln in read_text(path).splitlines():
        m = re.match(r"\s*slot \+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)", ln)
        if m:
            out[int(m.group(1), 16)] = m.group(2).upper()
    return out


d = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(".")
files = {p.stem.replace("vt_", ""): p for p in sorted(d.glob("vt_*.txt"))}
if not files:
    print("no vt_*.txt in %s" % d)
    sys.exit(2)
tabs = {name: slots(p) for name, p in files.items()}
print("vtables read: %s" % ", ".join("%s(%d)" % (k, len(v)) for k, v in tabs.items()))
print()

keys = sorted(set().union(*[set(t) for t in tabs.values()]))
names = list(tabs)
print("%-7s %s   %s" % ("slot", "  ".join("%-10s" % n for n in names), "verdict"))
print("-" * 78)
shared, diff = 0, []
for k in keys:
    vals = [tabs[n].get(k, "-") for n in names]
    uniq = set(v for v in vals if v != "-")
    if len(uniq) == 1:
        verdict = "SHARED  (inherited from the base class)"
        shared += 1
    else:
        verdict = "OVERRIDDEN by %d of %d" % (len(uniq), len(names))
        diff.append(k)
    print("+0x%02X    %s   %s" % (k, "  ".join("%-10s" % v for v in vals), verdict))
print()
print("%d of %d slots are identical across every mode read, so they belong to the shared base class."
      % (shared, len(keys)))
print("The overridden slots are the mode-specific behaviour: %s"
      % ", ".join("+0x%02X" % k for k in diff))
