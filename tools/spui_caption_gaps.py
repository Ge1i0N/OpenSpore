"""Why do 287 of the 1,426 (table,key) caption references NOT resolve?

The engine reports 1,139 resolving through engine/text.cpp. Three explanations are possible and they
lead to different fixes, so measure rather than guess:
  (1) the key belongs to a DIFFERENT table than the one the record names -> the pairing is loose and
      a reader should look the key up globally;
  (2) the key is real but in a locale we do not load (en-us only) -> nothing to fix, it is coverage;
  (3) the key is not a key at all -> our record SHAPE is wrong for those cases (e.g. the record is
      table-only and we read 4 bytes of the next record as a key, which is exactly what the
      "0 table-only" stat suspects).
"""
import sys, os, re, struct, glob
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.gmdl import all_packages
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode

SPUI, LOCALE = 0x250FE9A2, 0x02FAC0B6


def tables_for(lang):
    out = defaultdict(dict)
    for p in all_packages():
        if p.parent.name.lower() != lang:
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
                out[inst][int(m.group(1), 16)] = (body[mk.end():] if mk else body).strip()
    return out


en = tables_for("en-us")
all_keys = {}
for inst, kv in en.items():
    for k, v in kv.items():
        all_keys.setdefault(k, []).append(inst)
print("en-us: %d tables, %d distinct keys" % (len(en), len(all_keys)))

# A second locale, to test explanation (2) without loading all fifteen.
ru = tables_for("ru-ru")
ru_keys = {}
for inst, kv in ru.items():
    for k, v in kv.items():
        ru_keys.setdefault(k, []).append(inst)
print("ru-ru: %d tables, %d distinct keys" % (len(ru), len(ru_keys)))

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

bucket = Counter()
examples = defaultdict(list)
for pkg, r, inst, d in spuis:
    n = len(d)
    pat = struct.pack("<I", CAPTION)
    pos = 0
    while True:
        k = d.find(pat, pos)
        if k < 0 or k + 12 > n:
            break
        pos = k + 1
        if int.from_bytes(d[k + 4:k + 6], "little") != 18 or d[k + 6:k + 8] != b"\x01\x00":
            continue
        p0 = k + 8
        if int.from_bytes(d[p0:p0 + 2], "little") != 0xFFFF:
            continue
        tbl = int.from_bytes(d[p0 + 2:p0 + 6], "little")
        key = int.from_bytes(d[p0 + 6:p0 + 10], "little")
        in_own = key in en.get(tbl, {})
        global_hit = key in all_keys
        ru_hit = key in ru_keys
        if in_own:
            bucket["resolves in its own table"] += 1
            continue
        if global_hit:
            bucket["key is real but in ANOTHER en-us table"] += 1
            if len(examples["other_table"]) < 6:
                examples["other_table"].append((pkg, r, k, tbl, key,
                                                all_keys[key][0], en[all_keys[key][0]][key]))
        elif ru_hit:
            bucket["key exists only in another LOCALE (ru-ru)"] += 1
            if len(examples["other_locale"]) < 4:
                examples["other_locale"].append((pkg, r, k, tbl, key))
        else:
            bucket["key is not a key anywhere we parsed"] += 1
            if len(examples["bogus"]) < 8:
                examples["bogus"].append((pkg, r, k, tbl, key, d[p0:p0 + 14].hex(" ")))

print()
print("=== the 1,426 (table,key) caption references, classified ===")
tot = sum(bucket.values())
for kk, cc in bucket.most_common():
    print("   %-46s %-6d %5.1f%%" % (kk, cc, 100.0 * cc / max(1, tot)))
print("   total %d" % tot)

print()
for label, title in (("other_table", "keys that are real but in a DIFFERENT table"),
                     ("other_locale", "keys that exist only in ru-ru"),
                     ("bogus", "keys that are not keys anywhere")):
    print("=== %s ===" % title)
    for row in examples[label]:
        if label == "other_table":
            pkg, r, k, tbl, key, real_tbl, txt = row
            print("   %s rec[%d] @%d  named tbl=0x%08X  key=0x%08X  actually in tbl=0x%08X -> %r"
                  % (pkg, r, k, tbl, key, real_tbl, txt[:40]))
        elif label == "bogus":
            pkg, r, k, tbl, key, raw = row
            print("   %s rec[%d] @%d  tbl=0x%08X key=0x%08X  record bytes [%s]" % (pkg, r, k, tbl, key, raw))
        else:
            pkg, r, k, tbl, key = row
            print("   %s rec[%d] @%d  tbl=0x%08X key=0x%08X" % (pkg, r, k, tbl, key))
    if not examples[label]:
        print("   (none)")
    print()
