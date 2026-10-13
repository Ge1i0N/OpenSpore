"""The link this session has been building toward: a layout names its LOCALE TABLE.

tools/spui_locale.py produced a decisive NEGATIVE: code-18 (`string`/`string_resource`) payloads are
NOT keys into the locale tables (1 of 280 matched, and that one was 0x00000000). But its top values
were instantly recognizable:

     0x2B7A0311 x394   <- the en-us locale TABLE INSTANCE of the state GalaxyGameEntry
     0xAD7B2086 x309   <- the locale table instance whose record contains the string 'Credits'
     0x006F0054 x1837  <- UTF-16 "To" (a genuinely inline short string)

So code 18 is OVERLOADED: sometimes an inline UTF-16 string, sometimes a REFERENCE to a whole locale
table. That is a much stronger finding than a per-string key, because it binds three systems that were
reversed independently: the STATE MACHINE (analysis/scripts/SporeAppStates.txt), the LOCALIZED TEXT
(analysis/text-locale.md) and now the UI LAYOUT. It also explains why the four states whose names hash
to a table instance (GalaxyGameEntry, SpaceGame, CreatureGame, TribeGame) were the ones owning text.

This script measures the code-18 payloads against the TABLE INSTANCE set rather than the key set.
"""
import sys, os, re, struct, glob
from collections import Counter
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI, LOCALE = 0x250FE9A2, 0x02FAC0B6
PRIME = 0x01000193


def fnv1_lower(s):
    h = 0x811C9DC5
    for c in s.lower():
        h = ((h * PRIME) & 0xFFFFFFFF) ^ (ord(c) & 0xFF)
    return h


# ---- the en-us table instances, and what each table's first strings are ---------------------
tables = {}       # instance -> (pkg, rec, first few texts)
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
        samples = []
        for line in txt.replace("\r\n", "\n").split("\n"):
            m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
            if not m:
                continue
            body = m.group(2)
            mk = re.match(r"^~([^~]*)~[ \t]*", body)
            s = (body[mk.end():] if mk else body).strip()
            if s:
                samples.append(s)
            if len(samples) >= 3:
                break
        tables.setdefault(inst, (os.path.basename(p.name), r, samples))
print("en-us locale TABLE instances: %d" % len(tables))

# ---- the spui corpus + the code-18 payloads -------------------------------------------------
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

vals = Counter()
refs = Counter()
inline = Counter()
per_layout = {}
for pkg, r, inst, d in spuis:
    n = len(d)
    found = set()
    inline_samples = []
    for pat, v in IB.items():
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            pos = k + 1
            if d[k + 6:k + 8] != b"\x01\x00":
                continue
            if int.from_bytes(d[k + 4:k + 6], "little") != 18:
                continue
            p0 = k + 8
            if p0 + 6 > n:
                continue
            # Split the two forms of code 18 honestly: a leading u16 of 0xFFFF is the "not inline"
            # marker, so the u32 after it is a reference. Anything else is an inline UTF-16 length,
            # and reading a reference out of it would be measuring garbage.
            ln = int.from_bytes(d[p0:p0 + 2], "little")
            u = int.from_bytes(d[p0 + 2:p0 + 6], "little")
            if ln == 0xFFFF:
                refs[u] += 1
                found.add(u)
            else:
                inline[ln] += 1
                inline_samples.append((u, ln))
    if found:
        per_layout[(pkg, r, inst)] = found
print("spui: %d resources; %d carry a code-18 payload" % (len(spuis), len(per_layout)))

print()
print("=== code 18 splits into two forms ===")
print("  'not inline' (leading u16 == 0xFFFF): %d anchors, %d distinct refs"
      % (sum(refs.values()), len(refs)))
print("  inline UTF-16 (leading u16 = length): %d anchors, %d distinct lengths"
      % (sum(inline.values()), len(inline)))
print("  top inline lengths: %s" % ", ".join("%d chars x%d" % (l, c) for l, c in inline.most_common(6)))
print("  top inline first-u32 values (should read as UTF-16 text):")
for u, l in sorted(set(inline_samples), key=lambda t: -t[1])[:6]:
    b = struct.pack("<I", u)
    print("     len=%-5d u32=0x%08X  utf16=%r" % (l, u, b.decode("utf-16-le", "replace")))

print()
print("=== are the 'not inline' refs TABLE INSTANCES? ===")
inter = {u: c for u, c in refs.items() if u in tables}
print("  distinct refs: %d" % len(refs))
print("  of which are en-us TABLE INSTANCES: %d  (%d occurrences)"
      % (len(inter), sum(inter.values())))
print("  accidental rate would be %d/2^32 = %.2e" % (len(tables), len(tables) / 4294967296.0))
print()
for u, c in sorted(inter.items(), key=lambda kv: -kv[1])[:14]:
    pkg, rec, samples = tables[u]
    # which state name hashes to this instance?
    st = [s for s in re.findall(r"^state\s+(\S+)",
         open(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\SporeAppStates.txt",
              "rb").read().decode("utf-8", "replace").replace("\r\n", "\n"), re.M)
        if fnv1_lower(s) == u]
    print("  0x%08X x%-6d table in %-26s rec[%d]  %s%s" % (u, c, pkg, rec,
          ("STATE=" + ",".join(st) + "   ") if st else "", (samples[0][:44] if samples else "")))

print()
print("=== and the layouts that reference a state-owned table ===")
STATE_TABLES = {}
txt = open(r"E:\_DPT_Projects_E\_Harness\OpenSpore\analysis\scripts\SporeAppStates.txt",
           "rb").read().decode("utf-8", "replace").replace("\r\n", "\n")
for s in re.findall(r"^state\s+(\S+)", txt, re.M):
    STATE_TABLES[fnv1_lower(s)] = s
for (pkg, r, inst), fs in sorted(per_layout.items(), key=lambda kv: -len(kv[1]))[:10]:
    hit = [STATE_TABLES[u] for u in fs if u in STATE_TABLES]
    tbl = [u for u in fs if u in tables]
    print("  %-26s rec[%-6d] inst=0x%08x  code18 vals=%-4d  table refs=%-3d  state refs=%s"
          % (os.path.basename(pkg)[:26], r, inst, len(fs), len(tbl), ",".join(hit) or "-"))
