"""Hand-off step 1: is `instance == fnv1_lower(<lang dir>)` true for ALL 15 shipped locales?

text.cpp's nested pass currently selects the nested EP1 locale container by
`recs[n].instance() == naming::hash_lower(lang)`. That was VERIFIED for en-us only (0x7080DB55). If the
rule fails for any other language, `-locale:de-de` silently loses its EP1 text - the exact class of bug
this session already hit three times (a search that yields nothing and looks like missing data).

So enumerate all 21 nested packages across the 2 locale containers, and match each instance against
fnv1_lower of every shipped locale directory name AND against the registry. Report the full mapping,
including any nested package we cannot name.
"""
import sys, os, re, struct
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

PKG_TYPE, LOCALE = 0x06EFC6AA, 0x02FAC0B6
PRIME = 0x01000193


def fnv1(s, mode="lower"):
    h = 0x811C9DC5
    it = s.lower() if mode == "lower" else s.upper() if mode == "upper" else s
    for c in it:
        h = ((h * PRIME) & 0xFFFFFFFF) ^ (ord(c) & 0xFF)
    return h


def records(raw):
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
            shared[nm] = pw[q]; q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        off = w[fld["offset"]]
        d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                pass
        yield t, g, i, d


root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
langs = sorted(p.name for p in (root / "Data" / "Locale").iterdir() if p.is_dir())
by_hash = {}
for l in langs:
    for mode in ("lower", "raw", "upper"):
        by_hash.setdefault(fnv1(l, mode), (l, mode))

names = [x.strip() for x in open(
    r"E:\_DPT_Projects_E\_Harness\OpenSpore\upstream\SporeMaster3\data\reg_file.txt",
    encoding="utf-8", errors="replace") if x.strip() and not x.startswith("#")]
reg = {}
for nm in names:
    reg.setdefault(fnv1(nm), nm)

nested = {}
for tp in sorted(root.rglob("*.package")):
    if "Locale" not in str(tp) or not tp.name.lower().startswith("spore_ep1_locale"):
        continue
    raw = tp.read_bytes()
    for t, g, i, d in records(raw):
        if t != PKG_TYPE or len(d) < 0x60 or d[:4] != b"DBPF":
            continue
        n_loc, n_keys = 0, 0
        sample = ""
        for t2, g2, i2, d2 in records(d):
            if t2 != LOCALE:
                continue
            n_loc += 1
            txt = d2.decode("utf-8", "replace")
            if txt.startswith("\ufeff"):
                txt = txt[1:]
            for line in txt.replace("\r\n", "\n").split("\n"):
                m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
                if m:
                    n_keys += 1
                    if not sample:
                        body = m.group(2)
                        mk = re.match(r"^~([^~]*)~[ \t]*", body)
                        sample = (body[mk.end():] if mk else body).strip()
                    break
        nested.setdefault(i, []).append((tp.name, n_loc, n_keys, sample))

def safe(s, n=60):
    """The console here is cp1251; a real accented UI string would raise on print."""
    return str(s)[:n].encode("unicode_escape").decode("ascii")

print("shipped locale dirs (%d): %s" % (len(langs), ", ".join(langs)))
print("nested DBPF packages found: %d distinct instances across %d slots"
      % (len(nested), sum(len(v) for v in nested.values())))
print()
print("=== instance -> which language dir's hash is it? ===")
print("  %-11s %-9s %-8s %-7s %-9s %s" % ("instance", "in", "tables", "keys", "hash of", "sample"))
matched, unmatched = [], []
for i, rows in sorted(nested.items(), key=lambda kv: -max(r[1] for r in kv[1])):
    hit = by_hash.get(i)
    tot_loc = max(r[1] for r in rows)
    tot_keys = max(r[2] for r in rows)
    where = ",".join(sorted({r[0] for r in rows}))
    samp = rows[0][3][:34]
    if hit:
        matched.append((i, hit[0]))
        print("  0x%08X %-9s %-8d %-7d %-9s %s" % (i, where[:9], tot_loc, tot_keys,
              "%s (%s)" % (hit[0], hit[1]), safe(samp)))
    else:
        rn = reg.get(i, "")
        unmatched.append((i, rn, tot_loc, where))
        print("  0x%08X %-9s %-8d %-7d %-9s %s" % (i, where[:9], tot_loc, tot_keys,
              ("reg:%s" % rn[:12]) if rn else "NO MATCH", safe(samp)))

print()
print("=== verdict ===")
print("  language dirs matched by a nested package : %d / %d" % (len(matched), len(langs)))
seen_langs = {l for _i, l in matched}
missing = [l for l in langs if l not in seen_langs]
print("  language dirs with NO nested package      : %s" % (", ".join(missing) or "none"))
print("  nested packages we cannot name as a lang  : %d" % len(unmatched))
for i, rn, tl, where in unmatched[:8]:
    print("     0x%08X (%d tables, %s) registry name: %s" % (i, tl, where, rn or "<none>"))
print()
print("  => the rule `instance == fnv1_lower(<lang dir>)` holds for %d of %d languages"
      % (len(matched), len(langs)))
