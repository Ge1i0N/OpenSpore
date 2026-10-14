"""Audit the generated spui schema: which entries carry an IMPLAUSIBLE code?

The reader matches an anchor only when the u16 after the id equals the code the schema stores for that
id. Relaxing the third u16 from "must be 1" to "any sane count" therefore admits an anchor whenever the
schema happens to hold a garbage code for that id - and one such anchor appeared in a dump:
`id=0x4EC1B8D8 "Window" code=24565 count=40`, which is not a record shape at all.

Measured codes in the corpus are small and few: 3, 5, 7, 9, 11, 16, 17, 18, 19, 20 (plus 0xFFFF meaning
"unmeasured, skip"). Anything above 32 is not a code. This lists them so the reader can require a
plausible code, which is the missing guard.
"""
import io
import re
from collections import Counter

P = r"E:\_DPT_Projects_E\_Harness\OpenSpore\source\engine\spui_schema.inc"
t = io.open(P, encoding="utf-8").read()
rows = re.findall(r'\{0x([0-9A-Fa-f]+)u,\s*"([^"]*)",\s*"([^"]*)",\s*(\d+)u,\s*(\d+)u', t)
print("schema entries parsed: %d" % len(rows))

codes = Counter(int(r[3]) for r in rows)
print("code histogram: %s" % ", ".join("%s=%d" % kv for kv in sorted(codes.items())))

bad = [r for r in rows if int(r[3]) > 32 and int(r[3]) != 0xFFFF]
print()
print("entries whose code is > 32 and not 0xFFFF: %d" % len(bad))
for r in bad[:20]:
    print("  0x%s  %-26s %-14s code=%s size=%s" % (r[0], r[1], r[2], r[3], r[4]))
print()
print("entries with code == 0xFFFF (unmeasured, skipped by the reader): %d"
      % sum(1 for r in rows if int(r[3]) == 0xFFFF))
