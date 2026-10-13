"""Find the TRUE record framing by anchoring on records the schema already confirms.

The naive walk (anchor at the first reliable id anywhere, then consume sequentially) derailed in
748/748 files. That falsifies the ANCHOR, not the framing: the first reliable id hit in a file is
just as likely to be mid-record or inside a header as to be a record start.

So anchor on evidence instead. In PatchData rec[372] the id 0xeec1b005 (Area, declared `type="rectf"`
= 4 floats = 16 bytes) is followed by the u16 0x0010 = 16 and then `00 00 a8 42 00 00 7c 42`, which
decode as the floats 84.0 and 62.0 - real UI coordinates. That is a CONFIRMED record start, and there
are many like it: positions where a schema id is followed by a u16 exactly equal to the width its own
schema type implies. Anchor there, then walk outward and see which framing chains.

Framings tried, so the data picks rather than me:
  A  id(4) | size:u16 | tag:u16 | payload(size)          size = payload length
  B  id(4) | size:u16 | tag:u16 | payload(size-4)        size = the whole record minus the id
  C  id(4) | size:u16 | tag:u16 | payload(size-8)        size = id + header + payload
  D  id(4) | tag:u16 | size:u16 | payload(...)           the two u16 swapped
plus, for each, walking FORWARD and BACKWARD from the anchor.
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
WIDTH = {"int8": 1, "uint8": 1, "int16": 2, "uint16": 2, "int32": 4, "uint32": 4, "float": 4,
         "bool": 1, "color": 4, "image": 4, "textstyle": 4, "pointf": 8, "rectf": 16,
         "flagset": 4, "anchor": 4, "hitmask": 4, "Borders": 16}


def schema_width(ty):
    base = ty.split("[")[0].strip()
    am = re.match(r"^[^\[]*\[(\d+)\]$", ty)
    n = int(am.group(1)) if am else 1
    w = WIDTH.get(base)
    return None if w is None else w * n


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

# ---- 1. the confirmed anchors: id followed by a u16 == its own schema type width ------------
anchors = []          # (file_idx, pos, id, first_u16, second_u16)
for fi, (pkg, r, inst, d) in enumerate(spuis):
    n = len(d)
    for v in VOCAB:
        ty = VOCAB[v][2]
        sw = schema_width(ty)
        if sw is None or sw == 0:
            continue
        pat = struct.pack("<I", v)
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            a = int.from_bytes(d[k + 4:k + 6], "little")
            b = int.from_bytes(d[k + 6:k + 8], "little")
            if a == sw:
                anchors.append((fi, k, v, a, b))
            pos = k + 1
print("confirmed anchors (u16 == the schema type width): %d" % len(anchors))
ac = Counter(anchors[i][4] for i in range(len(anchors)))
print("  the SECOND u16 at an anchor:", ", ".join("0x%04x:%d" % (t, c) for t, c in ac.most_common(6)))
by_id = defaultdict(Counter)
for fi, k, v, a, b in anchors:
    by_id[v][b] += 1
print("  is the second u16 constant per id? ", end="")
const = [v for v in by_id if len(by_id[v]) == 1]
print("%d of %d ids have exactly one value" % (len(const), len(by_id)))
print()
print("  %-11s %-20s %-10s %-6s %s" % ("id", "name", "type", "width", "second-u16 values"))
for v, c in sorted(by_id.items(), key=lambda kv: -sum(kv[1].values()))[:20]:
    kind, nm, ty = VOCAB[v]
    print("  0x%08x %-20s %-10s %-6d %s" % (v, nm[:20], ty[:10], schema_width(ty),
          ", ".join("0x%04x:%d" % (t, n) for t, n in c.most_common(5))))

# ---- 2. walk outward from each anchor under every framing, and score chain length ----------
def fwd(d, pos, framing):
    """Returns (records, end, bad_reason)."""
    recs = []
    n = len(d)
    while True:
        if pos + 8 > n:
            return recs, pos, None if pos == n else "past EOF"
        v = int.from_bytes(d[pos:pos + 4], "little")
        if v not in VOCAB:
            return recs, pos, "id 0x%08x not in schema" % v
        a = int.from_bytes(d[pos + 4:pos + 6], "little")
        b = int.from_bytes(d[pos + 6:pos + 8], "little")
        sw = schema_width(VOCAB[v][2])
        if framing == "A":
            size, tag = a, b
        elif framing == "D":
            size, tag = b, a
        else:
            size, tag = a, b
        adj = {"A": 0, "B": 4, "C": 8, "D": 0}[framing]
        payload = size - adj
        if payload < 0 or payload > 4096:
            return recs, pos, "insane payload %d" % payload
        if pos + 8 + payload > n:
            return recs, pos, "runs past EOF"
        recs.append((pos, v, size, tag, payload))
        pos += 8 + payload
    return recs, pos, None


def bwd(d, pos, framing):
    """Step backwards: the previous record ends at `pos`, so find its start."""
    recs = []
    guard = 0
    while pos >= 8 and guard < 4000:
        guard += 1
        # Try every plausible record length ending at pos.
        found = None
        for back in range(8, min(pos, 4200) + 1):
            p = pos - back
            v = int.from_bytes(d[p:p + 4], "little")
            if v not in VOCAB:
                continue
            a = int.from_bytes(d[p + 4:p + 6], "little")
            b = int.from_bytes(d[p + 6:p + 8], "little")
            size, tag = (b, a) if framing == "D" else (a, b)
            adj = {"A": 0, "B": 4, "C": 8, "D": 0}[framing]
            payload = size - adj
            if payload < 0:
                continue
            if 8 + payload == back:
                found = (p, v, size, tag, payload)
                break
        if found is None:
            return recs, pos, "no record ends here"
        recs.append(found)
        pos = found[0]
    return recs, pos, None


print()
print("=== chain lengths from the confirmed anchors, per framing and direction ===")
FRAMES = ["A", "B", "C", "D"]
res = defaultdict(lambda: [0, 0, 0.0, 0])   # (framing, dir) -> [n_anchors, best_chain, mean, exact_eof]
for fi, k, v, a, b in anchors[:400]:
    d = spuis[fi][3]
    for fr in FRAMES:
        recs, end, bad = fwd(d, k, fr)
        res[(fr, "fwd")][0] += 1
        res[(fr, "fwd")][1] = max(res[(fr, "fwd")][1], len(recs))
        res[(fr, "fwd")][2] += len(recs)
        if bad is None and end == len(d):
            res[(fr, "fwd")][3] += 1
        recs, st, bad = bwd(d, k, fr)
        res[(fr, "bwd")][0] += 1
        res[(fr, "bwd")][1] = max(res[(fr, "bwd")][1], len(recs))
        res[(fr, "bwd")][2] += len(recs)
        if bad is None and st == 0:
            res[(fr, "bwd")][3] += 1
print("  %-8s %-5s %-10s %-11s %-9s %s" % ("framing", "dir", "anchors", "best chain", "mean", "reached the far end"))
for fr in FRAMES:
    for dr in ("fwd", "bwd"):
        n, best, mean, eof = res[(fr, dr)]
        print("  %-8s %-5s %-10d %-11d %-9.1f %d (%.1f%%)" % (fr + (" A=id|sz|tg|pl" if fr == "A" else ""),
              dr, n, best, mean / max(1, n), eof, 100.0 * eof / max(1, n)))

# ---- 3. show the longest chain found, so the framing is inspectable -------------------------
best_key, best_len, best_chain = None, 0, None
for fi, k, v, a, b in anchors[:400]:
    d = spuis[fi][3]
    for fr in FRAMES:
        for dr, fn in (("fwd", fwd), ("bwd", bwd)):
            recs, _e, _bad = fn(d, k, fr)
            if len(recs) > best_len:
                best_len, best_chain, best_key = len(recs), (fi, k, fr, recs), (fr, dr, fi, k)
if best_chain:
    fi, k, fr, recs = best_chain
    pkg, r, inst, d = spuis[fi]
    print()
    print("=== the longest chain: framing %s at %s rec[%d] inst=0x%08x anchor byte %d, %d records ==="
          % (fr, pkg, r, inst, k, len(recs)))
    for pos, v, size, tag, payload in recs[:30]:
        kind, nm, ty = VOCAB[v]
        pl = d[pos + 8:pos + 8 + payload]
        fl = []
        if payload % 4 == 0 and payload:
            try:
                fw = struct.unpack("<%df" % (payload // 4), pl)
                if all(f == f and abs(f) < 1e7 for f in fw):
                    fl = ["%.6g" % f for f in fw]
            except Exception:
                pass
        print("   @%-7d 0x%08x %-8s %-20s %-10s size=%-4d tag=0x%04x pl=%-3d %s%s"
              % (pos, v, kind, nm[:20], ty[:10], size, tag, payload, pl[:14].hex(" "),
                 ("  floats=" + ",".join(fl)) if fl else ""))
