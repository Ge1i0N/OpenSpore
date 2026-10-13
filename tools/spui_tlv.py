"""Test the TLV framing hypothesis by actually WALKING a layout, and score several framings.

What the raw contexts showed (PatchData rec[372], the smallest hits of 0xeec1b005 = Area, rectf):

    02 b0 c1 ee | 09 00 01 00 00 00 00 00      <- CommandID, then 8 bytes
    05 b0 c1 ee | 10 00 01 00 | 00 00 a8 42 00 00 7c 42 ...
                  ^^^^^^^^^^    ^^^^^^^^^^^^^^^^^^^^^^^^^^^
                  size=16,tag=1  four LE floats: 0x42A80000=84.0, 0x427C0000=62.0

`Area` is declared `type="rectf"` in the shipped designer schema, i.e. FOUR floats = 16 bytes, and the
observed size field is exactly 16. `Image Outline` (an Outline Format struct) shows size=0x14=20. So
the record is a TLV: id(4) | size:u16 | tag:u16 | payload[size].

If that is right, a walk from a record boundary should consume the file cleanly: every id resolves in
the schema, every tag is one small value, and the walk never drifts. If it is wrong, the walk derails
within a few records. This script tries several framings, scores them, and prints the winning walk so
the record structure is READ rather than guessed.

It also settles the alignment question honestly: the ids appear at all four alignments NOT because the
stream is bit-packed but because record sizes are not multiples of 4, so the id cursor drifts by
(size+8) mod 4 each record. The measured 415-byte spacing between consecutive Area hits in that file
is exactly that drift accumulating (415 = 4*103+3).
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2

# ---- schema: ids, names, types, and the type -> byte width the schema itself implies ---------
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
print("schema: %d ids (%d Class, %d Property)" % (len(VOCAB), len(CLS), len(PROP)))

WIDTH = {"int8": 1, "uint8": 1, "int16": 2, "uint16": 2, "int32": 4, "uint32": 4, "float": 4,
         "bool": 1, "color": 4, "image": 4, "textstyle": 4, "pointf": 8, "rectf": 16,
         "flagset": 4, "anchor": 4, "hitmask": 4, "Borders": 16, "IDrawable": 4, "IWindow": 4,
         "IWinProc": 4, "uint8": 1}


def schema_width(ty):
    """The byte width the schema's TYPE implies, arrays included (`color[8]`, `image[8]`)."""
    base = ty.split("[")[0].strip()
    n = 1
    am = re.match(r"^[^\[]*\[(\d+)\]$", ty)
    if am:
        n = int(am.group(1))
    w = WIDTH.get(base)
    return None if w is None else w * n


# ---- corpus -------------------------------------------------------------------------------
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


def walk(d, start, hdr, idw):
    """Walk records from `start`. hdr = header bytes between id and payload, idw = id width."""
    recs = []
    pos = start
    n = len(d)
    bad = None
    while pos + idw + hdr <= n:
        v = int.from_bytes(d[pos:pos + idw], "little")
        if v not in VOCAB:
            bad = ("id not in schema", pos, v)
            break
        size = int.from_bytes(d[pos + idw:pos + idw + 2], "little")
        tag = int.from_bytes(d[pos + idw + 2:pos + idw + hdr], "little") if hdr > 2 else 0
        if size > 4096:
            bad = ("insane size", pos, size)
            break
        if pos + idw + hdr + size > n:
            bad = ("runs past EOF", pos, size)
            break
        recs.append((pos, v, size, tag, d[pos + idw + hdr:pos + idw + hdr + size]))
        pos += idw + hdr + size
    return recs, pos, bad


# ---- score the framings, anchored on the FIRST reliable id hit of each file -----------------
RELIABLE = {v for v in VOCAB if struct.pack("<I", v).count(0) <= 1}
print()
print("=== framing search: anchor at the first reliable id, try (header bytes) 0..8 ===")
scores = defaultdict(lambda: [0, 0, 0])  # hdr -> [files_walked, total_recs, total_bad]
for pkg, r, inst, d in spuis:
    anchor = None
    for v in sorted(RELIABLE):
        k = d.find(struct.pack("<I", v))
        if k >= 0 and (anchor is None or k < anchor):
            anchor = k
    if anchor is None:
        continue
    for hdr in (0, 2, 4, 6, 8):
        recs, end, bad = walk(d, anchor, hdr, 4)
        scores[hdr][1] += len(recs)
        if bad is None and len(recs) >= 4:
            scores[hdr][0] += 1
        else:
            scores[hdr][2] += 1
print("  %-8s %-14s %-12s %s" % ("hdr", "files clean", "records", "derailed"))
anchored = sum(1 for _ in scores) and len(spuis)
for hdr in sorted(scores):
    c, nrec, b = scores[hdr]
    print("  hdr=%-5d %-14s %-12d %d" % (hdr, "%d / %d" % (c, anchored), nrec, b))

# ---- the detailed walk under the best framing ----------------------------------------------
best = max(scores, key=lambda h: (scores[h][0], scores[h][1]))
print()
print("=== best framing: id(4) | size:u16 | tag:u16 | payload(size)   [hdr=%d] ===" % best)
tag_hist = Counter()
size_vs_schema = Counter()
examples = []
clean = 0
for pkg, r, inst, d in spuis:
    anchor = None
    for v in sorted(RELIABLE):
        k = d.find(struct.pack("<I", v))
        if k >= 0 and (anchor is None or k < anchor):
            anchor = k
    if anchor is None:
        continue
    recs, end, bad = walk(d, anchor, best, 4)
    for pos, v, size, tag, payload in recs:
        tag_hist[tag] += 1
        kind, nm, ty = VOCAB[v]
        sw = schema_width(ty)
        if sw is None:
            size_vs_schema["type width unknown"] += 1
        elif size == sw:
            size_vs_schema["size == schema width"] += 1
        elif size == 0:
            size_vs_schema["size 0 (absent/default?)"] += 1
        else:
            size_vs_schema["size %d vs schema %d" % (size, sw) if False else
                            "size != schema width"] += 1
    if bad is None and len(recs) > 8 and len(examples) < 3:
        examples.append((pkg, r, inst, recs, end, len(d)))
    if bad is None:
        clean += 1
print("  tag values seen:", ", ".join("0x%04x:%d" % (t, c) for t, c in tag_hist.most_common(8)))
print("  size vs the schema's own type width:")
for k, c in size_vs_schema.most_common(6):
    print("     %-28s %d" % (k, c))
print("  files whose walk reached EOF with no derail: %d / %d" % (clean, len(spuis)))

for pkg, r, inst, recs, end, n in examples:
    print()
    print("  --- %s rec[%d] inst=0x%08x: %d records, walked %d/%d bytes ---" % (pkg, r, inst, len(recs), end, n))
    for pos, v, size, tag, payload in recs[:22]:
        kind, nm, ty = VOCAB[v]
        sw = schema_width(ty)
        floats = []
        if size % 4 == 0 and size:
            fw = struct.unpack("<%df" % (size // 4), payload)
            if all(f == f and abs(f) < 1e6 for f in fw):
                floats = ["%.4g" % f for f in fw]
        print("     @%-6d 0x%08x %-8s %-20s %-11s size=%-4d tag=0x%04x %s%s"
              % (pos, v, kind, nm[:20], ty[:11], size, tag,
                 payload[:16].hex(" ") + ("..." if len(payload) > 16 else ""),
                 ("   floats=%s" % ",".join(floats)) if floats else ""))
