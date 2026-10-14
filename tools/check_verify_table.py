"""Check VERIFY.md's main table: every numbered row must have exactly 6 cells.

Written as a file rather than a `python -c` one-liner because the shell collapses backslashes, which
makes a regex like `(?<!\\)\\|` report a BROKEN FILE when the file is fine and the CHECK is wrong - the
same class of mistake as the audit's twelve checker artifacts.
"""
import io
import re

P = r"E:\_DPT_Projects_E\_Harness\OpenSpore\VERIFY.md"
SEP = re.compile(r"(?<!\\)\|")          # a pipe that is a column separator, not an escaped one
ROW = re.compile(r"^\| (\d+) \|")

bad, total = [], 0
for ln, line in enumerate(io.open(P, encoding="utf-8").read().split("\n"), 1):
    m = ROW.match(line)
    if not m:
        continue
    total += 1
    cells = len(SEP.findall(line)) - 1
    if cells != 6:
        bad.append((m.group(1), cells, ln))

print("numbered rows: %d; rows that are not 6 cells: %d" % (total, len(bad)))
for n, c, ln in bad:
    print("   row %-4s cells=%-3d line %d" % (n, c, ln))
print("verdict:", "TABLE OK" if not bad else "SOME ROWS MISALIGNED")
