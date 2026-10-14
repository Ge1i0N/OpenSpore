"""Map each NESTED EP1 locale package to the language it holds, so the engine can mount the right one.

tools/nested_locale.py found 21 nested DBPFs inside DataEP1/Spore_EP1_Locale_0{1,2}.package carrying
1,364 `locale` resources - i.e. the EP1 text for every language, one nested package each. Our loaders
never descend a level, which is why the EP1 caption tables looked absent.

To mount the RIGHT one we need instance -> language. The obvious candidate is the same rule that names
the top-level locale tables: instance == fnv1_lower(<name>). Test it against the 15 shipped locale
directory names, and against the registry, before assuming.
"""
import sys, struct, re
from collections import Counter, defaultdict
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

LOCALE = 0x02FAC0B6
PRIME = 0x01000193


def fnv1_lower(s):
    h = 0x811C9DC5
    for c in s.lower():
        h = ((h * PRIME) & 0xFFFFFFFF) ^ (ord(c) & 0xFF)
    return h


def records(raw):
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT:OFF_COUNT + 4])[0]
    use = struct.unpack("<I", h[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", h[OFF_ALT:OFF_ALT + 4])[0]
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        return
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]; q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        off = w[fld["offset"]]
        d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
        if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
            try:
                d = decode(d, w[fld["usize"]])
            except Exception:
                pass
        yield t, g, i, d


root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
langs = sorted(p.name for p in (root / "Data" / "Locale").iterdir() if p.is_dir())
print("shipped locale directories: %s" % ", ".join(langs))

# instance -> how many locale resources, and a text sample, per nested package
nested = {}
for tp in (root / "DataEP1" / "Spore_EP1_Locale_01.package",
           root / "DataEP1" / "Spore_EP1_Locale_02.package"):
    raw = tp.read_bytes()
    for t, g, i, d in records(raw):
        if t != LOCALE and t != 0x06EFC6AA:
            continue
        if t != 0x06EFC6AA or len(d) < 0x60 or d[:4] != b"DBPF":
            continue
        n_loc = 0
        sample = ""
        keys = 0
        for t2, g2, i2, d2 in records(d):
            if t2 != LOCALE:
                continue
            n_loc += 1
            txt = d2.decode("utf-8", "replace")
            if txt.startswith("\ufeff"):
                txt = txt[1:]
            for line in txt.replace("\r\n", "\n").split("\n"):
                m = re.match(r"^0x([0-9A-Fa-f]{8})[ \t]*(.*)$", line)
                if m:
                    keys += 1
                    if not sample:
                        body = m.group(2)
                        mk = re.match(r"^~([^~]*)~[ \t]*", body)
                        sample = (body[mk.end():] if mk else body).strip()
                    break
        nested.setdefault(i, []).append((tp.name, n_loc, keys, sample))

print()
print("=== nested packages: instance -> contents, and does fnv1_lower(<lang dir>) hit it? ===")
byname = {fnv1_lower(l): l for l in langs}
for i, rows in sorted(nested.items(), key=lambda kv: -sum(r[1] for r in kv[1])):
    tot_loc = sum(r[1] for r in rows)
    tot_keys = sum(r[2] for r in rows)
    hit = byname.get(i)
    print("  0x%08X  %4d locale  %6d keys  in %-28s  %s%s" % (
        i, tot_loc, tot_keys, ",".join(sorted({r[0] for r in rows})),
        ("LANGUAGE = %s" % hit) if hit else "not a language-dir hash",
        ("   sample=%a" % rows[0][3][:38].encode("unicode_escape").decode("ascii"))
        if rows and rows[0][3] else ""))

print()
print("=== so which nested package is en-us? ===")
enus = fnv1_lower("en-us")
print("  fnv1_lower('en-us') = 0x%08X  present=%s" % (enus, enus in nested))

# The registry may name the nested packages differently (e.g. 'EP1_en-us'). Try it.
names = [l.strip() for l in open(r"E:\_DPT_Projects_E\_Harness\OpenSpore\upstream\SporeMaster3\data\reg_file.txt",
                                 encoding="utf-8", errors="replace") if l.strip() and not l.startswith("#")]
print()
print("=== registry names whose fnv1_lower hits a nested package ===")
for nm in names:
    h = fnv1_lower(nm)
    if h in nested:
        print("   %-30s -> 0x%08X" % (nm, h))
