"""Byte-granular scan: where do the schema's property ids ACTUALLY sit in a `spui` resource?

The word-aligned matcher found 227 of the 306 schema ids in 748/748 spui, but the smallest layout
showed the ids appearing as `0x09eec1b0` / `0x13eec1b0` at 4-byte offsets -- which is the id
0xeec1b0xx ROTATED by one byte. Reading the same bytes at offset 47 gives `00 b0 c1 ee` = 0xeec1b000
= WindowFlags, exactly. So the ids are real and the stream is simply not 4-byte aligned, which is what
a bit-packed token stream predicts. This scan measures the true alignment distribution instead of
assuming it.
"""
import sys, os, re, struct, glob
from collections import Counter
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2
CLASS_ID, PROP_ID = {}, {}
for path in sorted(glob.glob(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\xml\*.xml")):
    txt = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r'<(Class|Property)\b([^>]*)>', txt):
        kind, attrs = m.group(1), m.group(2)
        pm = re.search(r'proxy="(0x[0-9A-Fa-f]{1,8})"', attrs)
        if not pm:
            continue
        nm = re.search(r'name="([^"]*)"', attrs)
        ty = ""
        for k in ("type", "base", "enum"):
            em = re.search(r'%s="([^"]*)"' % k, attrs)
            if em:
                ty += " %s=%s" % (k, em.group(1))
        (CLASS_ID if kind == "Class" else PROP_ID)[int(pm.group(1), 16)] = (
            nm.group(1) if nm else "?", ty.strip())
VOCAB = {}
VOCAB.update({k: ("Class",) + v for k, v in CLASS_ID.items()})
VOCAB.update({k: ("Property",) + v for k, v in PROP_ID.items()})

# Only the ids whose 4-byte little-endian form is UNAMBIGUOUS and not a plausible payload value.
PATTERNS = {}
for v in VOCAB:
    PATTERNS.setdefault(struct.pack("<I", v), []).append(v)
print("vocab ids: %d, distinct 4-byte patterns: %d" % (len(VOCAB), len(PATTERNS)))

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
print("spui decoded:", len(spuis))

# ---- 1. the alignment distribution across ALL spui -----------------------------------------
mod4 = Counter()
mod8 = Counter()
total_hits = 0
for _pkg, _r, _i, d in spuis:
    for pat, ids in PATTERNS.items():
        start = 0
        while True:
            k = d.find(pat, start)
            if k < 0:
                break
            total_hits += 1
            mod4[k % 4] += 1
            mod8[k % 8] += 1
            start = k + 1
print()
print("total byte-granular id hits: %d" % total_hits)
print("offset mod 4:", dict(sorted(mod4.items())))
print("offset mod 8:", dict(sorted(mod8.items())))

# ---- 2. annotate the smallest layout at BYTE granularity ------------------------------------
pkg, r, inst, d = min(spuis, key=lambda x: len(x[3]))
print()
print("=== smallest spui: %s rec[%d] inst=0x%08x %d B ===" % (pkg, r, inst, len(d)))
marks = []
for pat, ids in PATTERNS.items():
    start = 0
    while True:
        k = d.find(pat, start)
        if k < 0:
            break
        marks.append((k, ids[0]))
        start = k + 1
marks.sort()
prev = 0
for k, v in marks:
    kind, nm, ty = VOCAB[v]
    gap = k - prev
    raw = d[prev:k].hex(" ") if gap <= 24 else ("...%d bytes..." % gap)
    print("  byte %3d  gap %3d [%s]  -> 0x%08x %s %s %s" % (k, gap, raw, v, kind, nm, ty))
    prev = k + 4
tail = d[prev:]
print("  ...tail %d bytes after the last id: %s" % (len(tail), tail[:32].hex(" ")))
print()
print("id count in this one layout: %d over %d bytes" % (len(marks), len(d)))
gaps = Counter(k - m for (_, k), (_, m) in zip(list(enumerate(marks))[1:], list(enumerate(marks))[:-1])) if len(marks) > 1 else Counter()
print("byte distances between consecutive ids (top 10):",
      ", ".join("%dB x%d" % (g, c) for g, c in gaps.most_common(10)))
