"""The integration test: is a layout's `string_resource` payload a KEY into the locale tables?

spui_windows.py extracted real window geometry from shipped layouts (Area rects (618,183,802,209),
(49,6,161,23), (0,0,40,26); WindowFlags 0x1B/0x0B/0x13). Two things it produced are diagnostic:

  * `Caption` (0xeec1b00a, schema type `string_resource`, code 18) yielded a payload whose first u16
    is 0xFFFF, which is implausible as a character count. `string_resource` in the schema means the
    value is an IDENTITY, not inline text - so code 18 is overloaded: inline `string` AND a
    `string_resource` reference, distinguished by the first u16 (0xFFFF = "not inline").
  * `HitMask` (type `hitmask`) and `TextFont` (type `textstyle`) came out as values that look like
    resource ids (0x0800090F, 0x00AEBB69, 0x04A0AD7A).

If a `string_resource` payload is a key into the `locale` tables, then the menu path closes today
without any more format work: layout gives geometry + a key, and source/engine/text.cpp already
resolves keys to the game's own strings. This script tests exactly that, by collecting every u32 that
appears at a string_resource/anchor payload and intersecting it with the 10,246 parsed en-us keys.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI, LOCALE = 0x250FE9A2, 0x02FAC0B6

# ---- the locale key set, from the shipped en-us tables (the same rule as source/engine/text.cpp)
keys = {}
for p in all_packages():
    if p.parent.name.lower() != "en-us":
        continue
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
        if t != LOCALE:
            continue
        off = w[fld["offset"]]
        d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                continue
        txt = d.decode("utf-8", "replace")
        if txt.startswith("\ufeff"):
            txt = txt[1:]
        cur = None
        for line in txt.replace("\r\n", "\n").split("\n"):
            if not line.strip():
                continue
            m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
            if m:
                cur = int(m.group(1), 16)
                body = m.group(2)
                mk = re.match(r"^~([^~]*)~[ \t]*", body)
                keys.setdefault(cur, body[mk.end():] if mk else body)
print("en-us locale keys parsed: %d" % len(keys))

# ---- the spui corpus -----------------------------------------------------------------------
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
print("spui corpus: %d resources" % len(spuis))

# ---- collect every u32 that sits at a code-18 (string) payload, and at the id-valued codes ----
hits18 = Counter()          # u32 -> count, from code 18 payloads
hits_any = Counter()        # every u32 in every anchor payload
n18 = 0
for _pkg, _r, _i, d in spuis:
    n = len(d)
    for pat, v in IB.items():
        ty = VOCAB[v][2]
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            pos = k + 1
            if d[k + 6:k + 8] != b"\x01\x00":
                continue
            code = int.from_bytes(d[k + 4:k + 6], "little")
            p0 = k + 8
            if code == 18:
                n18 += 1
                ln = int.from_bytes(d[p0:p0 + 2], "little") if p0 + 2 <= n else 0
                if ln == 0xFFFF and p0 + 6 <= n:
                    # the "not inline" form: the reference follows the 0xFFFF marker
                    hits18[int.from_bytes(d[p0 + 2:p0 + 6], "little")] += 1
                elif p0 + 6 <= n:
                    hits18[int.from_bytes(d[p0 + 2:p0 + 6], "little")] += 1
            # also gather plain u32 payloads for every property type
            if code in (9, 11) and p0 + 4 <= n:
                hits_any[int.from_bytes(d[p0:p0 + 4], "little")] += 1

print()
print("=== code 18 (string / string_resource) payloads: %d anchors ===" % n18)
inter = {u: c for u, c in hits18.items() if u in keys}
print("  distinct u32 at a code-18 payload: %d" % len(hits18))
print("  of which are EN-US LOCALE KEYS   : %d  (%d occurrences)"
      % (len(inter), sum(inter.values())))
for u, c in sorted(inter.items(), key=lambda kv: -kv[1])[:14]:
    print("     0x%08X x%-6d -> locale %r" % (u, c, keys[u][:58]))
print("  top non-key u32 (for contrast):")
for u, c in sorted(((u, c) for u, c in hits18.items() if u not in keys),
                   key=lambda kv: -kv[1])[:6]:
    print("     0x%08X x%d" % (u, c))

print()
print("=== and the plain u32 payloads (codes 9/11): do THEY hit the locale keys? ===")
inter2 = {u: c for u, c in hits_any.items() if u in keys}
print("  distinct u32: %d, of which locale keys: %d (%d occurrences)"
      % (len(hits_any), len(inter2), sum(inter2.values())))
for u, c in sorted(inter2.items(), key=lambda kv: -kv[1])[:10]:
    print("     0x%08X x%-6d -> locale %r" % (u, c, keys[u][:58]))

print()
print("=== control: how often would a RANDOM u32 be a locale key? ===")
# 10,246 keys out of 2^32, so the accidental rate is ~2.4e-6. Any real intersection is signal.
print("  10,246 / 2^32 = %.2e" % (len(keys) / 4294967296.0))
print("  code-18 intersection rate: %.4f%%  (%d/%d)"
      % (100.0 * len(inter) / max(1, len(hits18)), len(inter), len(hits18)))
print("  codes 9/11 intersection rate: %.4f%%  (%d/%d)"
      % (100.0 * len(inter2) / max(1, len(hits_any)), len(inter2), len(hits_any)))
