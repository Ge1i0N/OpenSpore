"""Refine the spui<->schema match and read the RECORD FRAMING off a small layout.

The first pass proved the ids are there (748/748 spui, 469,497 hits, dominant stride 3 words), but it
also matched EnumVal proxies, whose values are small integers (0x1..0xb) that collide with every
little counter in the file. So restrict the vocabulary to Class + Property ids, then dump a small
layout word-by-word with the names annotated, which is how the record framing gets read from DATA
rather than guessed.
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
        v = int(pm.group(1), 16)
        nm = re.search(r'name="([^"]*)"', attrs)
        nm = nm.group(1) if nm else "?"
        ty = ""
        for k in ("type", "base", "enum"):
            em = re.search(r'%s="([^"]*)"' % k, attrs)
            if em:
                ty += " %s=%s" % (k, em.group(1))
        (CLASS_ID if kind == "Class" else PROP_ID)[v] = (nm, ty.strip())
VOCAB = {}
VOCAB.update({k: ("Class",) + v for k, v in CLASS_ID.items()})
VOCAB.update({k: ("Property",) + v for k, v in PROP_ID.items()})
print("vocab: %d Class + %d Property = %d ids" % (len(CLASS_ID), len(PROP_ID), len(VOCAB)))

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

# ---- the id histogram across ALL spui, restricted to the real vocabulary --------------------
hist = Counter()
for _pkg, _r, _i, d in spuis:
    n = len(d) // 4
    words = struct.unpack("<%dI" % n, d[:n * 4])
    for v in set(words):
        if v in VOCAB:
            hist[v] += 1
print()
print("=== the real vocabulary, by how many spui use it (Class + Property only) ===")
for v, c in hist.most_common(30):
    k, nm, ty = VOCAB[v]
    print("  0x%08x in %3d/%d spui   %-9s %-24s %s" % (v, c, len(spuis), k, nm[:24], ty[:40]))
print("distinct vocab ids present in the binaries:", len(hist))

# ---- read the framing off the SMALLEST spui ------------------------------------------------
pkg, r, inst, d = min(spuis, key=lambda x: len(x[3]))
n = len(d) // 4
words = struct.unpack("<%dI" % n, d[:n * 4])
print()
print("=== smallest spui: %s rec[%d] inst=0x%08x %d B = %d words ===" % (pkg, r, inst, len(d), n))
for i, v in enumerate(words):
    if v in VOCAB:
        k, nm, ty = VOCAB[v]
        print("  [%4d] 0x%08x  <<< %s %s %s" % (i, v, k, nm, ty))
    else:
        f = struct.unpack("<f", struct.pack("<I", v))[0]
        note = ""
        if 0.0 < abs(f) < 8000.0 and f == f:
            note = "   (as float: %.4f)" % f
        print("  [%4d] 0x%08x  %10d%s" % (i, v, v, note))
print()
print("tail bytes not covered by whole words: %d" % (len(d) - n * 4))
