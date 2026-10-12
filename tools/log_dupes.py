"""Count duplicate package opens in a run log, to prove/disprove the double-scan suspicion.

The loader screen listed NameGeneration/Text/UI triples that looked too numerous for the
number of locales, and the dev-mode search list contains both `Data=<exe>/Data` and
`cwd=<exe>`, so a recursive scan of cwd re-visits everything under Data/. This script reads
the log and says so with numbers instead of vibes.
"""
import collections
import pathlib
import re
import sys

log = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "OpenSpore.log")
t = log.read_text(encoding="utf-8", errors="replace")

# lines look like:  opened E:\...\UI.package: ver=3 count=3 ...
paths = re.findall(r"opened (\S+\.package):", t)
c = collections.Counter(paths)
print("opened lines :", len(paths))
print("distinct files:", len(c))
dups = {k: v for k, v in c.items() if v > 1}
print("duplicated    :", len(dups))
for k, v in list(dups.items())[:8]:
    print("   x%d %s" % (v, k))

# also the var-width / describe lines from loader.cpp
desc = re.findall(r"loader\] (\S+\.package): ver=", t)
print("loader describe lines:", len(desc), "distinct:", len(set(desc)))
lc = collections.Counter(desc)
print("loader dups:", sum(1 for v in lc.values() if v > 1))
