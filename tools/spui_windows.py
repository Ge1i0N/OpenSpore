"""Extract real WINDOWS (geometry + text + art refs) from `spui` layouts, using the anchors directly.

A full sequential parser is not yet possible and, for the Main->playable path, not yet necessary:
637 of 748 walks derail because some record ids in the binaries are NOT in the shipped designer schema
(only 227 of its 306 ids appear at all), so an unknown id makes one gap span two records. Deriving the
schema further is a separate, larger job.

But the anchor set is already exact about what it does cover. An anchor is a position where a schema
id is followed by (code:u16, marker:u16==1), and the codes' payload sizes are measured
(tools/spui_read.py: 9/11 -> 4 bytes, 16 -> rectf 16, 17 -> pointf 8, 20 -> 6, 19 -> a u16 array count,
18 -> a u16 char count then UTF-16). So for every anchor I can read its VALUE with certainty even when
the bytes between two anchors contain an unidentifiable record. That is enough to answer the only
question the menu path needs: **where is each window, what does it say, and what art does it use.**

The window property block is known from the corpus (all 0xeec1b0xx, UTFWin's own window properties):
    WindowFlags 00 | ControlID 01 | CommandID 02 | ShadeColor 04 | Area 05 (rectf)
    BackgroundColor 06 | DialogDrawable 07 | Children 0b | WinProcs 0c | TextFont 0e | Caption 0a
So `ControlID` marks a window's identity and `Area` its geometry. Grouping anchors by the ControlID
that precedes them reconstructs the window list of a screen.

It also checks the two ids the recovered state script handed us:
    cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"
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
IB = {}
for v in VOCAB:
    IB.setdefault(struct.pack("<I", v), v)

# The measured code -> payload size table (tools/spui_read.py).
CODE_SIZE = {3: 1, 5: 4, 7: 7, 9: 4, 11: 4, 16: 16, 17: 8, 19: 2, 20: 6}
CODE_STRING, CODE_ARRAY, CODE_BOOL = 18, 19, 2

W = {
    "WindowFlags": 0xEEC1B000, "ControlID": 0xEEC1B001, "CommandID": 0xEEC1B002,
    "ShadeColor": 0xEEC1B004, "Area": 0xEEC1B005, "BackgroundColor": 0xEEC1B006,
    "DialogDrawable": 0xEEC1B007, "Caption": 0xEEC1B00A, "Children": 0xEEC1B00B,
    "WinProcs": 0xEEC1B00C, "TextFont": 0xEEC1B00E, "Image": 0x0F3AC75E,
    "HitMask": 0x0F3C000D, "Proportions": 0x0F3D0000, "Tileable": 0xEF3C000A,
    "ScaleFactor": 0xEF3C000B, "ScaleArea": 0xEF3C000C,
}

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
print("spui corpus: %d resources, %d bytes" % (len(spuis), sum(len(x[3]) for x in spuis)))

# ---- the two ids from the state script ----------------------------------------------------
print()
print("=== the ids the recovered state script names ===")
print('  cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"')
inst_set = {x[2] for x in spuis}
for cand in (0x027996D8, 0x0279BF9E):
    print("   0x%08x: a spui INSTANCE id? %s   (group field? cannot tell from the index alone)"
          % (cand, cand in inst_set))
# Are they referenced by any spui's bytes?
for cand in (0x027996D8, 0x0279BF9E):
    pat = struct.pack("<I", cand)
    n = sum(1 for _p, _r, _i, d in spuis if pat in d)
    print("   0x%08x appears inside %d spui resources' bytes" % (cand, n))


def anchors_of(d):
    """All (pos, id, code) where a schema id is followed by marker u16 == 1."""
    n = len(d)
    out = []
    for pat, v in IB.items():
        pos = 0
        while True:
            k = d.find(pat, pos)
            if k < 0 or k + 8 > n:
                break
            if d[k + 6:k + 8] == b"\x01\x00":
                out.append((k, v, int.from_bytes(d[k + 4:k + 6], "little")))
            pos = k + 1
    out.sort()
    dedup, last = [], -99
    for k, v, c in out:
        if k < last + 4:
            continue
        dedup.append((k, v, c))
        last = k
    return dedup


def value(d, v, code, p0, n):
    """Read one anchor's value by its code. Returns a display string, or None."""
    raw = lambda ln: d[p0:p0 + ln] if p0 + ln <= n else b""
    if code == CODE_STRING:
        if p0 + 2 > n:
            return None
        ln = int.from_bytes(d[p0:p0 + 2], "little")
        b = raw(2 + 2 * ln)
        if len(b) < 2 + 2 * ln:
            return None
        # ASCII-safe AND BOUNDED: L is read from the file, so a mis-aligned anchor can make it huge.
        # Cap it before slicing, or one bad record dumps megabytes (this bit me on the first run).
        ln = int.from_bytes(d[p0:p0 + 2], "little")
        if ln > 512:
            return "str? len=%d (implausible)" % ln
        end = p0 + 2 + 2 * ln
        if end > n:
            return None
        s = d[p0 + 2:end].decode("utf-16-le", "replace")
        return "str %a" % s.encode("unicode_escape").decode("ascii")
    if code == CODE_ARRAY:
        b = raw(2)
        if len(b) < 2:
            return None
        return "array[%d]" % int.from_bytes(b, "little")
    sz = CODE_SIZE.get(code)
    if sz is None:
        return None
    b = raw(sz)
    if len(b) < sz:
        return None
    ty = VOCAB[v][2]          # VOCAB[id] = (kind, name, type)
    if sz == 16:
        x, y, w, h = struct.unpack("<4f", b)
        return "rect (%.6g, %.6g, %.6g, %.6g)" % (x, y, w, h)
    if sz == 8:
        x, y = struct.unpack("<2f", b)
        return "pt (%.6g, %.6g)" % (x, y)
    if sz == 4:
        u = int.from_bytes(b, "little")
        f = struct.unpack("<f", b)[0]
        if ty == "float" or (f == f and 1e-3 < abs(f) < 1e5 and u not in (0, 1)):
            return "float %.6g" % f
        if ty in ("color",):
            return "color 0x%08X" % u
        if ty == "flagset":
            return "flags 0x%08X" % u
        return "u32 %d (0x%08X)" % (u, u)
    if sz == 7:
        return "7B %s" % b.hex(" ")
    if sz == 6:
        return "6B %s" % b.hex(" ")
    if sz == 1:
        return "u8 %d" % b[0]
    return "%dB %s" % (sz, b.hex(" "))


# ---- group anchors into windows ------------------------------------------------------------
print()
print("=== window extraction: how many layouts have a ControlID + Area pair? ===")
CTRL, AREA = W["ControlID"], W["Area"]
ranked = []
for pkg, r, inst, d in spuis:
    an = anchors_of(d)
    if not an:
        continue
    n = len(d)
    wins = []
    cur = None
    for k, v, code in an:
        if v == CTRL:
            cur = {"pos": k, "id": value(d, v, code, k + 8, n)}
            wins.append(cur)
        elif cur is not None:
            if VOCAB[v][0] != "Property":
                continue      # a Class id names a node type, not a window property
            nm = VOCAB[v][1]  # the property name
            val = value(d, v, code, k + 8, n)
            if val is not None and nm not in cur:
                cur[nm] = val
    ranked.append((len(wins), pkg, r, inst, d, wins, an))
ranked.sort(key=lambda x: -x[0])
tot_w = sum(x[0] for x in ranked)
print("  layouts with >=1 anchor: %d, windows found: %d" % (len(ranked), tot_w))
print("  layouts with a ControlID: %d" % sum(1 for x in ranked if x[0]))
print()
print("  %-26s %-6s %-10s %-7s %s" % ("package", "rec", "inst", "wins", "with Area / Caption / Image"))
for nw, pkg, r, inst, d, wins, an in ranked[:12]:
    wa = sum(1 for w in wins if "Area" in w)
    wc = sum(1 for w in wins if "Caption" in w)
    wi = sum(1 for w in wins if "Image" in w)
    print("  %-26s %-6d 0x%08x %-7d %d / %d / %d" % (os.path.basename(pkg)[:26], r, inst, nw, wa, wc, wi))

# ---- dump one real screen in full ----------------------------------------------------------
print()
for nw, pkg, r, inst, d, wins, an in ranked:
    if nw < 6:
        continue
    have = sum(1 for w in wins if "Area" in w and "Caption" in w)
    if have < 3:
        continue
    print("=== the first layout with both geometry and text: %s rec[%d] inst=0x%08x ===" %
          (os.path.basename(pkg), r, inst))
    print("    %d windows, %d anchors over %d bytes" % (nw, len(an), len(d)))
    for i, w in enumerate(wins[:26]):
        print("   win %-2d @%-6d %s" % (i, w["pos"], w.get("id", "")))
        for k in ("Area", "Caption", "Image", "BackgroundColor", "ShadeColor", "WindowFlags",
                  "CommandID", "TextFont", "Children", "WinProcs", "Proportions", "HitMask"):
            if k in w:
                print("        %-17s %s" % (k, w[k]))
    break
