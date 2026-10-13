"""Map the shipped Spore UI Designer schema (the `xml` resources) against the `spui` binaries.

The hypothesis this tests, raised by looking at both sides at once:
  * the designer XML gives every control CLASS and every PROPERTY a numeric `proxy` id, and property
    ids come in contiguous blocks per class (Animated Icon: 0xeec1d000..0xeec1d009);
  * the smallest `spui` resource (273 B) repeats the byte pattern `b0 c1 ee 09`, which read as a
    little-endian u32 is 0x09eec1b0 -- the SAME middle bytes (eec1) as those property ids.
If `spui` stores u32 property ids, then the format stops being a bit-packed mystery and becomes a
lookup problem we can settle with DATA: harvest the ids from the XML, find them in the binaries, and
see whether the hits land on a regular stride.

This is the method rule applied: do not reverse the tokenizer when the shipped schema can name the
fields for you.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2
XML = 0xCF6C21B8

# ---- 1. harvest every proxy id from the shipped designer XML -------------------------------
ids = {}          # id -> (kind, name, extra)
for path in sorted(glob.glob(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\xml\*.xml")):
    txt = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r'<(Class|Property|Enum|EnumVal|Struct)\b([^>]*)>', txt):
        kind, attrs = m.group(1), m.group(2)
        pm = re.search(r'proxy="(0x[0-9A-Fa-f]{1,8})"', attrs)
        if not pm:
            continue
        nm = re.search(r'name="([^"]*)"', attrs)
        extra = ""
        for k in ("type", "base", "enum"):
            em = re.search(r'%s="([^"]*)"' % k, attrs)
            if em:
                extra += " %s=%s" % (k, em.group(1))
        v = int(pm.group(1), 16)
        ids.setdefault(v, (kind, nm.group(1) if nm else "?", extra))
print("harvested proxy ids from XML: %d (%d Class, %d Property)"
      % (len(ids), sum(1 for k, _, _ in ids.values() if k == "Class"),
         sum(1 for k, _, _ in ids.values() if k == "Property")))

# ---- 2. collect the spui binaries ----------------------------------------------------------
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
        packed = w[fld["size"]] & 0x7FFFFFFF
        d = raw[off:off + packed]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                continue
        inst = w[fld["instance"]] if "instance" in fld else 0
        spuis.append((p.name, r, inst, d))
print("spui resources decoded:", len(spuis))

# ---- 3. find the harvested ids inside the binaries ---------------------------------------
id_words = set(ids)
hits_per = []
word_hist = Counter()
for pkg, r, inst, d in spuis:
    n = len(d) // 4
    words = struct.unpack("<%dI" % n, d[:n * 4])
    hits = [i for i, v in enumerate(words) if v in id_words]
    hits_per.append((pkg, r, inst, len(d), len(words), hits, words))

tot = sum(len(h[5]) for h in hits_per)
with_hits = [h for h in hits_per if h[5]]
print("spui with >=1 XML proxy id at a 4-byte-aligned offset: %d / %d  (total hits %d)"
      % (len(with_hits), len(hits_per), tot))

print()
print("=== the most frequent matching words across ALL spui (these are the format's vocabulary) ===")
for v, c in Counter(w for _p, _r, _i, _sz, _n, hs, ws in hits_per for w in set(ws[i] for i in hs)).most_common(22):
    k, nm, extra = ids[v]
    print("  0x%08x in %3d spui   %-9s %-26s%s" % (v, c, k, nm[:26], extra[:34]))

print()
print("=== per-file detail: are the hits at a regular stride? ===")
for pkg, r, inst, sz, n, hits, ws in sorted(hits_per, key=lambda x: -len(x[5]))[:10]:
    deltas = Counter(b - a for a, b in zip(hits, hits[1:]))
    top = deltas.most_common(3)
    print("  %s rec[%d] inst=0x%08x %5dB words=%4d hits=%3d  top strides=%s"
          % (pkg, r, inst, sz, n, len(hits),
             ", ".join("%dw x%d" % (d, c) for d, c in top)))
    for i in hits[:6]:
        v = ws[i]
        k, nm, extra = ids[v]
        print("      @word %-4d 0x%08x  %-9s %s%s" % (i, v, k, nm[:24], extra[:28]))
