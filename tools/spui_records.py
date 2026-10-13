"""Audit my OWN bit-packing claim before building a decoder on it, then walk the records properly.

The claim under test, written into analysis/spui-schema.md an hour ago:
    "1,441,987 of 1,911,484 hits are off-boundary, so the stream is not word-aligned, exactly as a
     token stream predicts."
Why it is suspicious: the aggregate mod-4 histogram was NEARLY UNIFORM (469,497 / 472,622 / 488,922 /
480,443). Two mechanisms produce that, and they imply opposite decoder designs:

  (a) the stream really is bit-packed, so a 4-byte id lands at all four alignments; or
  (b) the LOW ids (58 of the 602 schema properties are 0x00000001..0x0000000c) have patterns like
      `01 00 00 00`, which match inside any run of zero padding AT EVERY ALIGNMENT. A UI layout is
      mostly padding, so those alone can flatten the histogram however the real ids are framed.

The discriminator is the pattern's own bytes, measured as the NUMBER OF ZERO BYTES it contains: a run
of padding can only supply zero-ish windows, so an id with 3-4 zero bytes matches trivially
everywhere, while an id with 0-1 zero bytes (0xeec1b000 -> `00 b0 c1 ee`; 0x039a69e6 -> `e6 69 9a 03`)
needs real structure to appear. This script measures the alignment distribution of the reliable ids,
then walks records using only them.

The session's own lesson, applied to myself: a number that looks like confirmation is first a
hypothesis about the measuring tool.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI = 0x250FE9A2

# ---- the schema vocabulary -----------------------------------------------------------------
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
print("vocab: %d ids (%d Class + %d Property)" % (len(VOCAB), len(CLS), len(PROP)))
ZC = {v: struct.pack("<I", v).count(0) for v in VOCAB}
for n in (0, 1, 2, 3, 4):
    print("   ids with %d zero byte(s): %d" % (n, sum(1 for v in VOCAB if ZC[v] == n)))
RELIABLE = {v for v in VOCAB if ZC[v] <= 1}
STRICT = {v for v in VOCAB if ZC[v] == 0}
print("reliable (<=1 zero byte): %d    strict (0 zero bytes): %d" % (len(RELIABLE), len(STRICT)))

# ---- the spui corpus -----------------------------------------------------------------------
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
total_bytes = sum(len(x[3]) for x in spuis)
print("spui corpus: %d resources, %d bytes total" % (len(spuis), total_bytes))
print("random-coincidence floor for %d patterns over %d bytes: %.2f hits"
      % (len(VOCAB), total_bytes, total_bytes * len(VOCAB) / 4294967296.0))

# ---- 1. the alignment audit, per id --------------------------------------------------------
align_by_id = defaultdict(Counter)
for _pkg, _r, _i, d in spuis:
    for v in VOCAB:
        pat = struct.pack("<I", v)
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0:
                break
            align_by_id[v][k % 4] += 1
            pos = k + 1

agg = Counter()
for v, ac in align_by_id.items():
    for m in range(4):
        agg[m] += ac[m]
print()
print("=== aggregate mod-4 (the number that misled me) ===")
for m in range(4):
    print("   mod 4 == %d : %-9d %6.2f%%" % (m, agg[m], 100.0 * agg[m] / max(1, sum(agg.values()))))
print("   -> dominated by the 3-or-4-zero-byte ids, which padding satisfies at every alignment")

print()
print("=== per-id, split by zero-byte count (the decisive cut) ===")
for n in (0, 1, 2, 3, 4):
    tot = Counter()
    seen = 0
    for v in VOCAB:
        if ZC[v] != n:
            continue
        s = sum(align_by_id[v].values())
        if s:
            seen += 1
        for m in range(4):
            tot[m] += align_by_id[v][m]
    g = sum(tot.values())
    if not g:
        print("   zc=%d  ids seen 0/%d" % (n, sum(1 for v in VOCAB if ZC[v] == n)))
        continue
    print("   zc=%d  ids seen %3d/%3d  hits %-9d  mod4: %s" % (
        n, seen, sum(1 for v in VOCAB if ZC[v] == n), g,
        " ".join("%d:%d(%.1f%%)" % (m, tot[m], 100.0 * tot[m] / g) for m in range(4))))

print()
print("=== the reliable ids individually ===")
print("  %-11s %-22s %-11s %-4s %-9s %s" % ("id", "name", "type", "zc", "hits", "mod-4 [0 1 2 3]"))
for v in sorted(RELIABLE, key=lambda x: -sum(align_by_id[x].values()))[:28]:
    ac = align_by_id[v]
    tot = sum(ac.values())
    if not tot:
        continue
    kind, nm, ty = VOCAB[v]
    print("  0x%08x %-22s %-11s %-4d %-9d [%d %d %d %d]" % (v, nm[:22], ty[:11], ZC[v], tot,
          ac[0], ac[1], ac[2], ac[3]))

rel_tot = Counter()
for v in RELIABLE:
    for m in range(4):
        rel_tot[m] += align_by_id[v][m]
strict_tot = Counter()
for v in STRICT:
    for m in range(4):
        strict_tot[m] += align_by_id[v][m]
grand, sg = sum(rel_tot.values()), sum(strict_tot.values())
print()
if not grand:
    print("  VERDICT: no reliable id appears in the corpus - the vocabulary does not match.")
else:
    verdict = 100.0 * rel_tot[0] / grand
    print("  VERDICT  reliable set: %.2f%% of %d hits are 4-byte ALIGNED" % (verdict, grand))
    if sg:
        print("           strict  set: %.2f%% of %d hits are aligned" % (100.0 * strict_tot[0] / sg, sg))
    if verdict >= 0.85:
        print("  => RETRACT the bit-packing claim: the off-boundary mass was the LOW ids matching in")
        print("     zero padding. Reading the IDS needs no bit cursor.")
    elif verdict < 0.35:
        print("  => the stream genuinely is not byte-aligned; a bit cursor is real.")
    else:
        print("  => mixed; go per id before concluding.")

# ---- 2. the record walk, aligned reliable ids only -----------------------------------------
print()
print("=== RECORD WALK (aligned hits of reliable ids) ===")
gap_by_id = defaultdict(Counter)
gap_samples = defaultdict(list)
files_with = 0
for _pkg, _r, _i, d in spuis:
    hits = []
    for v in RELIABLE:
        pat = struct.pack("<I", v)
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0:
                break
            if k % 4 == 0:
                hits.append((k, v))
            pos = k + 1
    if not hits:
        continue
    files_with += 1
    hits.sort()
    dedup, last = [], -99
    for k, v in hits:
        if k < last + 4:
            continue
        dedup.append((k, v))
        last = k
    for idx, (k, v) in enumerate(dedup):
        if idx + 1 >= len(dedup):
            continue
        g = dedup[idx + 1][0] - (k + 4)
        if g < 0 or g > 1024:
            continue
        gap_by_id[v][g] += 1
        if len(gap_samples[v]) < 5:
            gap_samples[v].append(d[k + 4:dedup[idx + 1][0]])
print("files with >=1 aligned reliable hit: %d / %d" % (files_with, len(spuis)))
print()
print("  %-11s %-22s %-11s %-7s %-6s %s" % ("id", "name", "type", "recs", "widths", "width -> count"))
for v, gc in sorted(gap_by_id.items(), key=lambda kv: -sum(kv[1].values()))[:28]:
    kind, nm, ty = VOCAB[v]
    print("  0x%08x %-22s %-11s %-7d %-6d %s" % (v, nm[:22], ty[:11], sum(gc.values()), len(gc),
          ", ".join("%d:%d" % (w, c) for w, c in gc.most_common(6))))
print()
print("=== ids whose record width is UNIQUE across the whole corpus ===")
uniq = 0
for v, gc in sorted(gap_by_id.items(), key=lambda kv: -sum(kv[1].values())):
    if len(gc) != 1:
        continue
    uniq += 1
    kind, nm, ty = VOCAB[v]
    w = list(gc.keys())[0]
    print("  0x%08x %-22s %-11s width=%-3d hits=%-6d %s"
          % (v, nm[:22], ty[:11], w, gc[w], " | ".join(b.hex(" ") for b in gap_samples[v][:4])))
print("  unique-width ids: %d of %d seen" % (uniq, len(gap_by_id)))
