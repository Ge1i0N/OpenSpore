"""Decisive test of the `spui` RECORD SHAPE, using the shipped schema as the id vocabulary.

The byte-granular scan showed the smallest layout's ids sitting at a repeating 12-byte pitch
(id@47, 59, 71, ... 159, 171, 183) with 8 bytes between one id's end and the next id's start. If that
is the real record shape rather than an accident of one file, then across all 748 layouts:
  * the id->id distance should cluster on multiples of 12 (values wider than 4 bytes -> 24, 36...),
  * the 4 bytes immediately BEFORE every id hit should be a small set of markers,
  * and the same property id should always carry the same width (the schema's type decides it).
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
    for m in re.finditer(r'<(Class|Property)\b([^>]*)>', txt):
        kind, attrs = m.group(1), m.group(2)
        pm = re.search(r'proxy="(0x[0-9A-Fa-f]{1,8})"', attrs)
        if not pm:
            continue
        nm = re.search(r'name="([^"]*)"', attrs)
        ty = re.search(r'type="([^"]*)"', attrs)
        (CLS if kind == "Class" else PROP)[int(pm.group(1), 16)] = (
            nm.group(1) if nm else "?", ty.group(1) if ty else "")
VOCAB = {}
VOCAB.update({k: ("Class",) + v for k, v in CLS.items()})
VOCAB.update({k: ("Property",) + v for k, v in PROP.items()})
PAT = {struct.pack("<I", v): v for v in VOCAB}

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
print("spui:", len(spuis))

# ---- collect hits, then look at the PRECEDING 4 bytes and the id->id distance ---------------
before = Counter()
pitch = Counter()
per_id_pitch = defaultdict(Counter)
n_files_with_hits = 0
for _pkg, _r, _i, d in spuis:
    hits = []
    for pat, v in PAT.items():
        start = 0
        while True:
            k = d.find(pat, start)
            if k < 0:
                break
            hits.append((k, v))
            start = k + 1
    if not hits:
        continue
    n_files_with_hits += 1
    hits.sort()
    # Deduplicate overlapping hits, keeping the earliest.
    kept, last = [], -99
    for k, v in hits:
        if k < last + 4:
            continue
        kept.append((k, v))
        last = k
    for idx, (k, v) in enumerate(kept):
        if k >= 4:
            before[d[k - 4:k].hex(" ")] += 1
        if idx + 1 < len(kept):
            dist = kept[idx + 1][0] - k
            pitch[dist] += 1
            per_id_pitch[v][dist] += 1

print("files with >=1 schema id: %d / %d" % (n_files_with_hits, len(spuis)))
print()
print("=== the 4 bytes IMMEDIATELY BEFORE an id hit (top 14) ===")
for b, c in before.most_common(14):
    v = struct.unpack("<I", bytes.fromhex(b.replace(" ", "")))[0]
    print("   %-12s x%-7d  as u32 0x%08x (%d)%s" % (b, c, v, v,
          "   <<< %s %s" % VOCAB[v][:2] if v in VOCAB else ""))
print()
print("=== id -> id distance (top 14) ===")
for dd, c in pitch.most_common(14):
    print("   %3d bytes  x%-7d  (%s12)" % (dd, c, "divisible by 12" if dd % 12 == 0 else "NOT "))
print()
tot = sum(pitch.values())
d12 = sum(c for dd, c in pitch.items() if dd % 12 == 0)
print("pitches divisible by 12: %d / %d (%.1f%%)" % (d12, tot, 100.0 * d12 / max(1, tot)))
print()
print("=== per-id pitch: does the SAME id always carry the SAME width? ===")
shown = 0
for v, pc in sorted(per_id_pitch.items(), key=lambda kv: -sum(kv[1].values())):
    if PROP.get(v, ("", ""))[1] == "" and v not in PROP:
        continue
    kinds = PROP if v in PROP else CLS
    nm, ty = kinds[v]
    top = pc.most_common(3)
    tot_i = sum(pc.values())
    frac = top[0][1] / float(tot_i)
    print("   0x%08x %-22s %-12s hits=%-6d pitch %s  (dominant %.0f%%)"
          % (v, nm[:22], ty[:12], tot_i,
             ", ".join("%dB x%d" % (dd, c) for dd, c in top), 100 * frac))
    shown += 1
    if shown >= 18:
        break
