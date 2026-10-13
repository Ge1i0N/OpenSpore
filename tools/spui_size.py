"""Measure what the u16 after a schema id actually means, instead of assuming it is a payload size.

Evidence so far (tools/spui_frame.py):
  * 22,750 positions hold a schema id followed by a u16 equal to the width its own schema type
    implies; at those positions the SECOND u16 is 0x0001 in 22,396 cases (98.4%). So the second u16
    is a constant marker, not a variable field, and `Area` (rectf=16) dominates the anchor set.
  * A sequential walk `id(4) | u16 | u16 | payload(u16)` chained exactly 3 records and then derailed,
    and the derail was diagnostic: the record read as ShadeColor (a 4-byte color) carried u16=9, yet
    its payload bytes were `ff ff ff ff` (a real color) immediately followed by the NEXT schema id
    0xeec1b00e (TextFont). So the true record length was 12 = 8 + 4, and the u16 said 9.

So the u16 is NOT the payload length. Three candidate meanings remain, and they are separable by
measurement rather than argument:
  (i)  a payload length in some other unit (bits? dwords? elements?)
  (ii) a field count or member index
  (iii) a length that includes the header, or that is padded/rounded up
The decisive data is the ACTUAL gap to the next anchor, measured over thousands of samples per id,
compared against both the u16 and the schema width. This script produces that table.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
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
IDS_BY_BYTES = {}
for v in VOCAB:
    IDS_BY_BYTES.setdefault(struct.pack("<I", v), v)
print("schema: %d ids" % len(VOCAB))

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

# ---- anchors: a schema id whose following u16 is the constant marker 0x0001 ------------------
# This is the strongest independent signal available: it does not presuppose what the FIRST u16
# means, so the first u16 can be measured freely against the gap to the next anchor.
gap_vs_u16 = defaultdict(Counter)     # id -> (u16, gap) -> count
u16_hist = defaultdict(Counter)       # id -> u16 -> count
mark_pos = Counter()
n_anchor = 0
for _pkg, _r, _i, d in spuis:
    n = len(d)
    hits = []
    for pat, v in IDS_BY_BYTES.items():
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
    if not hits:
        continue
    hits.sort()
    dedup, last = [], -99
    for k, v, a in hits:
        if k < last + 4:
            continue
        dedup.append((k, v, a))
        last = k
    for idx, (k, v, a) in enumerate(dedup):
        n_anchor += 1
        u16_hist[v][a] += 1
        if idx + 1 < len(dedup):
            g = dedup[idx + 1][0] - k          # the FULL record length
            gap_vs_u16[v][(a, g)] += 1
print("anchors (schema id followed by u16==1): %d" % n_anchor)

# regroup: id -> u16 -> record length -> count
by_id = defaultdict(lambda: defaultdict(Counter))
for v, pairs in gap_vs_u16.items():
    for (a, g), c in pairs.items():
        by_id[v][a][g] += 1

print()
print("=== per id: the u16 values, and the ACTUAL record length to the next anchor ===")
print("  %-11s %-20s %-11s %-8s %s" % ("id", "name", "type", "n", "(u16 -> {record length: count})"))
rows = sorted(by_id.items(), key=lambda kv: -sum(sum(gc.values()) for gc in kv[1].values()))
for v, us in rows[:22]:
    kind, nm, ty = VOCAB[v]
    n = sum(sum(gc.values()) for gc in us.values())
    top = sorted(us.items(), key=lambda kv: -sum(kv[1].values()))[:4]
    print("  0x%08x %-20s %-11s %-8d %s" % (v, nm[:20], ty[:11], n,
          "   ".join("u16=%d -> {%s}" % (a, ", ".join("%d:%d" % (g, c) for g, c in gc.most_common(4)))
                     for a, gc in top)))

print()
print("=== the key question: is record_length - 8 == u16, or something else? ===")
buckets = Counter()
for v, pairs in gap_vs_u16.items():
    for (a, g), c in pairs.items():
        pl = g - 8
        if pl == a:
            buckets["payload == u16"] += c
        elif pl * 2 == a:
            buckets["payload == u16/2"] += c
        elif pl * 8 == a:
            buckets["payload == u16/8 (u16 is BITS)"] += c
        elif a - pl == 4:
            buckets["payload == u16-4 (u16 counts id+hdr?)"] += c
        elif a - pl == 5:
            buckets["payload == u16-5"] += c
        elif pl == 0:
            buckets["payload 0 (u16 is not a length at all)"] += c
        else:
            buckets["other (u16=%d, payload=%d)" % (a, pl) if False else "other"] += c
tot = sum(buckets.values())
for k, c in buckets.most_common(12):
    print("   %-42s %-8d %5.1f%%" % (k, c, 100.0 * c / max(1, tot)))
print("   total measured (id, next-id) pairs: %d" % tot)

print()
print("=== and separately: is record_length - 8 == the SCHEMA type width? ===")
WIDTH = {"int8": 1, "uint8": 1, "int16": 2, "uint16": 2, "int32": 4, "uint32": 4, "float": 4,
         "bool": 1, "color": 4, "image": 4, "textstyle": 4, "pointf": 8, "rectf": 16,
         "flagset": 4, "anchor": 4, "hitmask": 4, "Borders": 16}
sb = Counter()
for v, pairs in gap_vs_u16.items():
    ty = VOCAB[v][2]
    base = ty.split("[")[0].strip()
    am = re.match(r"^[^\[]*\[(\d+)\]$", ty)
    mult = int(am.group(1)) if am else 1
    sw = WIDTH.get(base)
    for (a, g), c in pairs.items():
        pl = g - 8
        if sw is None:
            sb["schema width unknown (%s)" % ty[:14]] += c
        elif pl == sw:
            sb["payload == schema width"] += c
        elif pl == 0:
            sb["payload 0"] += c
        else:
            sb["payload %d vs schema %d" % (pl, sw) if False else "payload != schema width"] += c
for k, c in sb.most_common(8):
    print("   %-42s %-8d" % (k, c))
