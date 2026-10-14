"""What is actually inside a code-18 REFERENCE record: a table id alone, or table id + string key?

Measured so far (tools/spui_table_ref.py): code 18 splits on a leading u16 of 0xFFFF into 4,207 inline
UTF-16 strings and 3,107 references, and 21 of the 36 distinct reference values are real en-us locale
TABLE instances (accidental rate 4.66e-08). But a table holds ~90 keys on average, so a reference to a
TABLE cannot by itself select a STRING. Either:
  (a) the record carries a key too, further along its bytes, or
  (b) the widget's text is chosen at runtime by the code that owns the widget, or
  (c) the reference is not per-widget at all but per-LAYOUT (one table for the whole screen).
(c) is the cheapest to test and would still be useful: it tells us WHICH screen each layout is.

This script dumps the WHOLE record around each reference (not just the first 6 bytes) and looks for a
u32 that is a KEY of the referenced table. It also checks (c) by counting how many distinct table refs
one layout carries.
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI, LOCALE = 0x250FE9A2, 0x02FAC0B6

# ---- the en-us locale tables: instance -> {key: text} --------------------------------------
tables = defaultdict(dict)
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
        inst = w[fld["instance"]] if "instance" in fld else 0
        txt = d.decode("utf-8", "replace")
        if txt.startswith("\ufeff"):
            txt = txt[1:]
        for line in txt.replace("\r\n", "\n").split("\n"):
            m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
            if not m:
                continue
            body = m.group(2)
            mk = re.match(r"^~([^~]*)~[ \t]*", body)
            tables[inst][int(m.group(1), 16)] = (body[mk.end():] if mk else body).strip()
print("en-us tables: %d instances, %d keys total" % (len(tables),
      sum(len(v) for v in tables.values())))

# ---- the spui corpus ------------------------------------------------------------------------
PROP = {}
for path in sorted(glob.glob(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\xml\*.xml")):
    txt = open(path, encoding="utf-8", errors="replace").read()
    for m in re.finditer(r"<Property\b([^>]*)>", txt):
        a = m.group(1)
        pm = re.search(r'proxy="(0x[0-9A-Fa-f]{1,8})"', a)
        if not pm:
            continue
        nm = re.search(r'name="([^"]*)"', a)
        ty = re.search(r'type="([^"]*)"', a)
        PROP[int(pm.group(1), 16)] = (nm.group(1) if nm else "?", ty.group(1) if ty else "")
CAPTION = 0xEEC1B00A

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
        spuis.append((os.path.basename(p.name), r, w[fld["instance"]] if "instance" in fld else 0, d))
print("spui: %d resources" % len(spuis))

# ---- every Caption (0xeec1b00a) record, with the FULL bytes after it ------------------------
recs = []
for pkg, r, inst, d in spuis:
    n = len(d)
    pat = struct.pack("<I", CAPTION)
    pos = 0
    while True:
        k = d.find(pat, pos)
        if k < 0 or k + 8 > n:
            break
        pos = k + 1
        if int.from_bytes(d[k + 4:k + 6], "little") != 18:
            continue
        if d[k + 6:k + 8] != b"\x01\x00":
            continue
        # the next anchor of ANY known property, to bound the record honestly
        nxt = n
        for v in PROP:
            j = d.find(struct.pack("<I", v), k + 8)
            if 0 <= j < nxt:
                nxt = j
        body = d[k + 8:nxt]
        if len(body) > 48:
            body = body[:48]
        recs.append((pkg, r, inst, k, nxt - k, body))
print("Caption records (code 18, marker 1): %d" % len(recs))

refs = [x for x in recs if len(x[5]) >= 6 and x[5][0:2] == b"\xff\xff"]
print("  of which the 'not inline' form (leading u16 == 0xFFFF): %d" % len(refs))
inline = [x for x in recs if len(x[5]) >= 2 and x[5][0:2] != b"\xff\xff"]
print("  inline form: %d" % len(inline))

print()
print("=== (c) is the reference PER LAYOUT (one table for the whole screen)? ===")
per_layout = defaultdict(set)
for pkg, r, inst, k, ln, body in refs:
    per_layout[(pkg, r, inst)].add(int.from_bytes(body[2:6], "little"))
sizes = Counter(len(v) for v in per_layout.values())
print("  layouts with >=1 table ref: %d" % len(per_layout))
print("  distinct table refs per layout:", dict(sorted(sizes.items())))
multi = [kk for kk, v in per_layout.items() if len(v) > 1]
print("  layouts with MORE THAN ONE table ref: %d  -> so it is not purely per-layout" % len(multi))

print()
print("=== (a) does the record also carry a KEY of that table? ===")
found_key = 0
shown = 0
for pkg, r, inst, k, ln, body in refs:
    tbl = int.from_bytes(body[2:6], "little")
    keys = tables.get(tbl)
    if not keys:
        continue
    # scan every u32 in the record AFTER the table id for a key of that table
    hit = None
    for off in range(6, len(body) - 3):
        u = int.from_bytes(body[off:off + 4], "little")
        if u in keys:
            hit = (off, u, keys[u])
            break
    if hit:
        found_key += 1
        if shown < 12:
            print("  %s rec[%d] @%d reclen=%-3d tbl=0x%08X  key at byte +%d = 0x%08X -> %r"
                  % (pkg, r, k, ln, tbl, hit[0], hit[1], hit[2][:46]))
            print("      record bytes: %s" % body.hex(" "))
            shown += 1
print("  records carrying a key of the referenced table: %d / %d refs" % (found_key, len(refs)))

print()
print("=== the raw shape of the reference records (first 10) ===")
for pkg, r, inst, k, ln, body in refs[:10]:
    tbl = int.from_bytes(body[2:6], "little")
    known = "TABLE" if tbl in tables else "     "
    print("  %s rec[%d] @%-7d reclen=%-4d %s  tbl=0x%08X %s" % (pkg, r, k, ln, body.hex(" ")[:44], tbl, known))
    if len(body) > 6:
        rest = body[6:]
        print("        after the table id: %s" % rest.hex(" "))
        for off in range(0, len(rest) - 3, 2):
            u = int.from_bytes(rest[off:off + 4], "little")
            if u in tables:
                print("        +%d: 0x%08X is ALSO a table instance (%d keys)" % (off, u, len(tables[u])))
