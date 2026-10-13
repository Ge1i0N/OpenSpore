"""Establish that the u16 after a schema id is a TYPE CODE (stable per id), and measure how each
code's payload is sized - the last thing needed to walk a layout.

Observation that motivates this (tools/spui_size.py): anchoring on (schema id, marker u16 == 1) gives
1,098,129 anchors, and for the HIGH ids the first u16 is a SINGLE value per id across the whole
corpus - TextFont always 9, Caption always 18, WinProcs and Children always 19, Proportions always 7 -
while the record LENGTH varies. A length field would vary and the type would not. So the u16 is a
type code, and the schema's own `type=` attribute should explain the mapping:
    9  <- textstyle / color / uint32   (4-byte scalars)
    11 <- float / int8 / flagset
    18 <- string_res / string          (variable)
    19 <- IWindow[] / IWinProc[]       (arrays)
    7  <- float[4]                     (fixed arrays)
Meanwhile the LOW ids (<0x100) show several u16 values per id, which is exactly the block-scoped
false-friend behaviour established for the locale tables.

This script (1) proves the stability split high vs low, (2) derives the code -> schema-type map from
the data rather than from my reading, (3) measures whether each (id, code)'s record length is fixed,
and (4) for the variable ones, finds how the payload declares its own length.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2
PROP, CLS = {}, {}
for path in sorted(glob.glob(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\xml\*.xml")):
    txt = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"<(Class|Property)\b([^>]*)>", txt):
        kind, attrs = m.group(1), m.group(2)
        pm = re.search(r'proxy="(0x[0-9A-Fa-f]{1,8})"', attrs)
        if not pm:
            continue
        nmm = re.search(r'name="([^"]*)"', attrs)
        ty = re.search(r'type="([^"]*)"', attrs)
        (CLS if kind == "Class" else PROP)[int(pm.group(1), 16)] = (
            nmm.group(1) if nmm else "?", ty.group(1) if ty else "")
VOCAB = {}
VOCAB.update({k: ("Class",) + v for k, v in CLS.items()})
VOCAB.update({k: ("Property",) + v for k, v in PROP.items()})
IB = {}
for v in VOCAB:
    IB.setdefault(struct.pack("<I", v), v)
HIGH = {v for v in VOCAB if v >= 0x1000}
LOW = {v for v in VOCAB if v < 0x1000}
print("schema: %d ids; high (>=0x1000) %d, low (<0x100) %d" % (len(VOCAB), len(HIGH), len(LOW)))

spuis = []
for p in all_packages():
    raw = p.read_bytes()
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT:OFF_COUNT + 4])[0]
    use = struct.unpack("<I", h[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", h[OFF_ALT:OFF_ALT + 4])[0]
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        continue
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]; q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        if t != SPUI:
            continue
        off = w[fld["offset"]]
        d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                continue
        spuis.append((p.name, r, w[fld["instance"]] if "instance" in fld else 0, d))
print("corpus: %d resources, %d bytes" % (len(spuis), sum(len(x[3]) for x in spuis)))

# ---- collect anchors once, reuse everywhere -------------------------------------------------
codes = defaultdict(Counter)          # id -> u16 -> count
lens = defaultdict(Counter)           # (id, u16) -> record length -> count
samples = defaultdict(list)           # (id, u16) -> [(pos, reclen, 64 bytes of lookahead)]
for _pkg, _r, _i, d in spuis:
    n = len(d)
    hits = []
    for pat, v in IB.items():
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            a = int.from_bytes(d[k + 4:k + 6], "little")
            b = int.from_bytes(d[k + 6:k + 8], "little")
            if b == 1:
                hits.append((k, v, a))
            pos = k + 1
    hits.sort()
    dedup, last = [], -99
    for k, v, a in hits:
        if k < last + 4:
            continue
        dedup.append((k, v, a))
        last = k
    for idx, (k, v, a) in enumerate(dedup):
        codes[v][a] += 1
        if idx + 1 >= len(dedup):
            continue
        g = dedup[idx + 1][0] - k
        lens[(v, a)][g] += 1
        if len(samples[(v, a)]) < 40:
            samples[(v, a)].append((k, g, d[k + 8:min(n, k + 8 + 64)]))
print("anchors: %d" % sum(sum(c.values()) for c in codes.values()))

# ---- 1. the stability split ---------------------------------------------------------------
print()
print("=== 1. is the u16 stable per id? the high/low split ===")
for group, label in ((HIGH, "HIGH ids (>=0x1000)"), (LOW, "LOW ids (<0x100)")):
    tot = uni = 0
    for v in group:
        c = codes.get(v)
        if not c:
            continue
        tot += 1
        if len(c) == 1:
            uni += 1
    print("  %-24s ids seen %3d, with a SINGLE u16: %3d (%.1f%%)"
          % (label, tot, uni, 100.0 * uni / max(1, tot)))
print("  -> high stable + low unstable  ==  the u16 is a TYPE CODE the schema assigns per property,")
print("     and the low ids are the block-scoped false friends (same rule as the locale tables).")

# ---- 2. the code -> schema-type map, derived ------------------------------------------------
print()
print("=== 2. code -> schema type, derived from the data ===")
by_code = defaultdict(Counter)
for v, c in codes.items():
    for a, n in c.items():
        by_code[a][(VOCAB[v][0], VOCAB[v][2])] += n
for a in sorted(by_code, key=lambda x: -sum(by_code[x].values()))[:12]:
    tot = sum(by_code[a].values())
    kinds = ", ".join("%s:%s x%d" % (k, t, n) for (k, t), n in by_code[a].most_common(5))
    print("  code %-6d n=%-8d %s" % (a, tot, kinds[:100]))

# ---- 3. fixed or variable length per (id, code) --------------------------------------------
print()
print("=== 3. per (id, code): fixed or variable record length? ===")
print("  %-11s %-20s %-11s %-5s %-7s %s" % ("id", "name", "type", "code", "recs", "lengths"))
fc = Counter()
for v, c in sorted(codes.items(), key=lambda kv: -sum(kv[1].values()))[:18]:
    kind, nm, ty = VOCAB[v]
    for a, _n in c.most_common(2):
        lc = lens.get((v, a))
        if not lc:
            continue
        print("  0x%08x %-20s %-11s %-5d %-7d %s" % (v, nm[:20], ty[:11], a, sum(lc.values()),
              ", ".join("%dB:%d" % (g, cc) for g, cc in lc.most_common(4))))
        fc["single" if len(lc) == 1 else "variable(%d)" % len(lc)] += 1
print("  ", dict(fc))

# ---- 4. how does a variable payload declare its length? ------------------------------------
# The Caption sample showed the shape: payload `02 00 31 00 30 00` = u16 2 then FOUR bytes of UTF-16
# ("10"), i.e. the u16 counts CHARACTERS. Test every candidate reading; 64 bytes of lookahead are
# kept so a check may look past the record's own payload.
print()
print("=== 4. the variable codes: how is the payload sized? ===")
tally = Counter()
shown = 0
for (v, a), ss in sorted(samples.items(), key=lambda kv: -len(kv[1])):
    lc = lens.get((v, a))
    if not lc or len(lc) < 3:
        continue                      # fixed-length records do not test this
    kind, nm, ty = VOCAB[v]
    per = Counter()
    for k, g, pl in ss:
        if len(pl) < 6:
            continue
        L = int.from_bytes(pl[0:2], "little")
        if 2 + 2 * L + 4 <= len(pl) and pl[2 + 2 * L:2 + 2 * L + 4] in IB:
            per["u16 = chars (UTF-16)"] += 1
        if 2 + L + 4 <= len(pl) and pl[2 + L:2 + L + 4] in IB:
            per["u16 = bytes"] += 1
        if len(pl) >= 8:
            L4 = int.from_bytes(pl[0:4], "little")
            if 4 + L4 + 4 <= len(pl) and pl[4 + L4:4 + L4 + 4] in IB:
                per["u32 = bytes"] += 1
        if g >= 8 and g - 4 <= len(pl) + 8 and pl[g - 8:g - 4] in IB:
            per["no len field (fixed by code)"] += 1
    for kk, cc in per.items():
        tally[kk] += cc
    if shown < 12:
        print("  0x%08x %-18s %-11s code=%-4d recs=%-4d %s"
              % (v, nm[:18], ty[:11], a, len(ss),
                 ", ".join("%s:%d" % (kk, cc) for kk, cc in per.most_common(4)) or "nothing fit"))
        shown += 1
print("  TOTALS over all variable (id, code) pairs:")
for kk, cc in tally.most_common():
    print("     %-30s %d" % (kk, cc))

# Emit the measured id -> code map for tools/gen_spui_schema.py, so the C++ reader carries the
# MEASURED code rather than a guess. Majority vote per id; a tie is recorded as the winner plus the
# agreement fraction so a consumer can see how solid it is.
import json as _json
out = {}
for v, c in codes.items():
    if not c:
        continue
    a, n = c.most_common(1)[0]
    out[str(v)] = {"code": a, "count": n, "agree": n / float(sum(c.values())),
                   "alternatives": len(c)}
p = os.path.join(ROOT, "analysis", "audit", "spui_codes.json")
os.makedirs(os.path.dirname(p), exist_ok=True)
with open(p, "w", encoding="utf-8") as f:
    _json.dump(out, f, indent=1, sort_keys=True)
print("wrote %s (%d ids)" % (p, len(out)))

# ---- 5. judge a variable record by eye ----------------------------------------------------
print()
print("=== 5. the actual bytes of a variable-code record ===")
for (v, a), ss in sorted(samples.items(), key=lambda kv: -len(kv[1])):
    lc = lens.get((v, a))
    if not lc or len(lc) < 3:
        continue
    kind, nm, ty = VOCAB[v]
    print("  --- 0x%08x %s (%s, code %d) ---" % (v, nm, ty, a))
    for k, g, pl in ss[:6]:
        L = int.from_bytes(pl[0:2], "little") if len(pl) >= 2 else -1
        s16 = "?"
        try:
            s16 = pl[2:2 + 2 * L].decode("utf-16-le", "replace")
        except Exception:
            pass
        print("     @%-6d reclen=%-4d payload=%-44s u16=%-6d utf16=%r" % (k, g, pl[:22].hex(" "), L, s16))
    break
