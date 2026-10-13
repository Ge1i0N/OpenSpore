"""Read a `spui` layout into a window tree.

Everything this relies on was MEASURED in this session, not assumed:
  * the shipped designer XML (type `xml` 0xCF6C21B8, 14 resources) supplies 306 property/class ids;
    748/748 spui contain them, and 227 of 306 appear in the binaries (tools/spui_schema.py).
  * a record is  id(4) | code:u16 | marker:u16(==1) | payload .  The u16 is a TYPE CODE, stable per id
    (94.5% of the high ids have exactly one value; only the 13 block-scoped low ids vary), and the
    marker is 0x0001 in 98.4% of 1,098,129 anchors (tools/spui_typecode.py).
  * the payload length is FIXED BY THE CODE for most codes (Area/rectf -> 24-byte record = 8+16,
    Image Outline -> 14, the 4-byte scalars -> 12), and VARIABLE for code 18 (string: u16 character
    count then UTF-16) and code 19 (arrays / interface refs).
  * the ids are NOT 4-byte aligned in the file because record sizes are not multiples of 4, so the
    cursor drifts. That drift, not bit packing, is what the uniform mod-4 histogram showed.

So: derive the code -> payload table from the corpus by majority vote, then walk a layout with it and
report whether the walk reaches EOF cleanly. A clean walk is the correctness proof; a derail names the
code that needs a better rule.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2

# ---------------------------------------------------------------- the schema
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
        df = re.search(r'default="([^"]*)"', attrs)
        (CLS if kind == "Class" else PROP)[int(pm.group(1), 16)] = (
            nmm.group(1) if nmm else "?", ty.group(1) if ty else "",
            df.group(1) if df else "")
VOCAB = {}
VOCAB.update({k: ("Class",) + v for k, v in CLS.items()})
VOCAB.update({k: ("Property",) + v for k, v in PROP.items()})
IB = {}
for v in VOCAB:
    IB.setdefault(struct.pack("<I", v), v)
print("schema: %d ids" % len(VOCAB))

# ---------------------------------------------------------------- the corpus
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

# ---------------------------------------------------------------- code -> payload size
# Anchor on (schema id, marker==1) and vote the distance to the next anchor minus 8.
vote = defaultdict(Counter)
for _pkg, _r, _i, d in spuis:
    n = len(d)
    hits = []
    for pat, v in IB.items():
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            if d[k + 6:k + 8] == b"\x01\x00":
                hits.append((k, v, int.from_bytes(d[k + 4:k + 6], "little")))
            pos = k + 1
    hits.sort()
    dedup, last = [], -99
    for k, v, a in hits:
        if k < last + 4:
            continue
        dedup.append((k, v, a))
        last = k
    for idx, (k, v, a) in enumerate(dedup):
        if idx + 1 < len(dedup):
            g = dedup[idx + 1][0] - k
            if 8 <= g <= 512:
                vote[a][g - 8] += 1
CODE_SIZE = {}
VARIABLE = set()
print()
print("=== the code -> payload-size table, by majority vote over the corpus ===")
print("  %-6s %-9s %-9s %-8s %s" % ("code", "payload", "votes", "agree%", "verdict"))
for a, c in sorted(vote.items(), key=lambda kv: -sum(kv[1].values())):
    tot = sum(c.values())
    if tot < 20:
        continue
    size, best = c.most_common(1)[0]
    agree = 100.0 * best / tot
    if agree < 80.0:
        VARIABLE.add(a)
        CODE_SIZE[a] = None
        print("  %-6d %-9s %-9d %-8.1f VARIABLE (top sizes %s)"
              % (a, "?", tot, agree, ", ".join("%d:%d" % (s, n) for s, n in c.most_common(4))))
    else:
        CODE_SIZE[a] = size
        print("  %-6d %-9d %-9d %-8.1f fixed" % (a, size, tot, agree))
print("  fixed-size codes: %s" % sorted(k for k, v in CODE_SIZE.items() if v is not None))
print("  variable codes:   %s" % sorted(VARIABLE))

# the code -> schema-type legend, for readability
by_code = defaultdict(Counter)
for _pkg, _r, _i, d in spuis[:60]:
    n = len(d)
    for pat, v in IB.items():
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            if d[k + 6:k + 8] == b"\x01\x00":
                by_code[int.from_bytes(d[k + 4:k + 6], "little")][VOCAB[v][2]] += 1
            pos = k + 1
print()
print("=== what the schema calls each code's values ===")
for a in sorted(by_code, key=lambda x: -sum(by_code[x].values()))[:12]:
    print("  code %-4d %s" % (a, ", ".join("%s x%d" % (t, c) for t, c in by_code[a].most_common(5))))

# ---------------------------------------------------------------- the walker
def read_string(d, pos, n):
    if pos + 2 > n:
        return None, pos
    ln = int.from_bytes(d[pos:pos + 2], "little")
    end = pos + 2 + 2 * ln
    if end > n:
        return None, pos
    return d[pos + 2:end].decode("utf-16-le", "replace"), end


def walk(d, start=0, limit=4000):
    """Walk records from `start`. Returns (records, pos, derail_reason)."""
    n = len(d)
    recs = []
    pos = start
    while pos + 8 <= n and len(recs) < limit:
        v = int.from_bytes(d[pos:pos + 4], "little")
        if v not in VOCAB:
            return recs, pos, "id 0x%08x not in schema" % v
        code = int.from_bytes(d[pos + 4:pos + 6], "little")
        marker = int.from_bytes(d[pos + 6:pos + 8], "little")
        p0 = pos + 8
        if code == 18:                       # string / string_resource
            s, p1 = read_string(d, p0, n)
            if s is None:
                return recs, pos, "code 18 string runs past EOF"
            recs.append((pos, v, code, marker, p0, p1, s))
            pos = p1
            continue
        if code in VARIABLE or CODE_SIZE.get(code) is None:
            return recs, pos, "code %d has no fixed size" % code
        p1 = p0 + CODE_SIZE[code]
        if p1 > n:
            return recs, pos, "payload runs past EOF"
        recs.append((pos, v, code, marker, p0, p1, None))
        pos = p1
    if pos == n:
        return recs, pos, None
    return recs, pos, "stopped at %d of %d" % (pos, n)


def payload_value(d, v, code, p0, p1, sval):
    """Decode the payload by its code, using the schema type for interpretation."""
    raw = d[p0:p1]
    ty = VOCAB[v][2]
    if code == 18:
        return "string %r" % sval
    if len(raw) == 4:
        f = struct.unpack("<f", raw)[0]
        u = int.from_bytes(raw, "little")
        if ty == "float" or (f == f and 1e-4 < abs(f) < 1e5 and u not in (0, 1)):
            return "float %.6g" % f
        if ty in ("color",):
            return "color 0x%08X" % u
        if ty == "flagset":
            return "flags 0x%08X" % u
        return "u32 %d (0x%08X)" % (u, u)
    if len(raw) == 8:
        x, y = struct.unpack("<ff", raw)
        return "pointf (%.6g, %.6g)" % (x, y)
    if len(raw) == 16:
        vals = struct.unpack("<4f", raw)
        if ty == "rectf":
            return "rectf x=%.6g y=%.6g w=%.6g h=%.6g" % vals
        return "4 floats %s" % ", ".join("%.6g" % f for f in vals)
    return "%d bytes %s" % (len(raw), raw[:12].hex(" "))


# ---------------------------------------------------------------- walk the corpus, score it
print()
print("=== walking every layout with the derived table ===")
# A layout is not guaranteed to begin with a record: try pos 0 first, and if that derails
# immediately, start at the first (schema id, marker==1) anchor instead and report the header size.
clean = derail = 0
reasons = Counter()
per_file = []
hdr_sizes = Counter()
for pkg, r, inst, d in spuis:
    recs, end, bad = walk(d, 0)
    start = 0
    if bad is not None and len(recs) < 2:
        # look for the first plausible record start
        for pat, v in IB.items():
            k = d.find(pat)
            if k >= 0 and d[k + 6:k + 8] == b"\x01\x00":
                start = k
                break
        if start:
            hdr_sizes[start] += 1
            recs, end, bad = walk(d, start)
    if bad is None:
        clean += 1
    else:
        derail += 1
        reasons[bad.split(" 0x")[0]] += 1
    per_file.append((len(recs), pkg, r, inst, d, recs, end, bad, start))
print("  reached EOF cleanly : %d / %d" % (clean, len(spuis)))
print("  derailed            : %d" % derail)
for k, c in reasons.most_common(6):
    print("     %-46s %d" % (k[:46], c))
if hdr_sizes:
    print("  header sizes where pos 0 failed: %s" % ", ".join(
        "%d:%d" % (s, c) for s, c in hdr_sizes.most_common(5)))

# ---------------------------------------------------------------- dump real layouts
per_file.sort(key=lambda x: -x[0])
print()
print("=== the richest layouts, decoded ===")
for nrec, pkg, r, inst, d, recs, end, bad, start in per_file[:3]:
    print()
    print("--- %s rec[%d] inst=0x%08x: %d records, %d/%d bytes, derail=%s ---"
          % (pkg, r, inst, nrec, end, len(d), bad))
    depth = 0
    shown = 0
    for pos, v, code, marker, p0, p1, sval in recs:
        kind, nm, ty, df = VOCAB[v]
        if kind == "Class":
            print("  %sCLASS %s (%s)" % ("  " * depth, nm, ty))
            continue
        val = payload_value(d, v, code, p0, p1, sval)
        print("  %s%-18s %-11s code=%-3d %s" % ("  " * depth, nm[:18], ty[:11], code, val))
        shown += 1
        if shown >= 46:
            print("  ... (%d more records)" % (len(recs) - shown))
            break
