"""Enumerate EVERY nested `package` resource in the whole tree - no filename filter.

Why this tool exists: session 13's hand-off listed "three shapes of the 'data does not exist'
error", and this is the same mistake a fourth time. nested_all_langs.py scoped its search to files
named *Spore_EP1_Locale*, because that is where the first nested package was found. The engine,
scanning ALL containers for type 0x06EFC6AA, then reported a THIRD en-us nested package with
DIFFERENT bytes - inside Data/PatchData.package, a name filter could never match.

So: enumerate by TYPE across the whole corpus, never by filename.

Implementation note (the first version of this tool timed out at 15 minutes): index the records
LAZILY. Only the resources we actually inspect get RefPack-decoded. A patch container can hold
hundreds of nested packages, and eagerly decoding every resource inside every one of them is a
whole-game extraction disguised as an enumeration.
"""
import sys, struct, hashlib
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

PKG_TYPE, LOCALE_TYPE = 0x06EFC6AA, 0x02FAC0B6
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")


def index(raw):
    """Yield (type, group, instance, meta) for every record. meta decodes on demand. No I/O here."""
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
            shared[nm] = pw[q]
            q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
        g = shared.get("group", w[fld["group"]] if "group" in fld else 0)
        i = w[fld["instance"]] if "instance" in fld else 0
        yield (t, g, i, (raw, w, fld))


def payload(meta):
    """Decode one indexed record's bytes, lazily."""
    raw, w, fld = meta
    off = w[fld["offset"]]
    d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
    if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
        try:
            d = decode(d, w[fld["usize"]])
        except Exception:
            pass
    return d


files, seen = [], set()
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        k = str(p.resolve())
        if k in seen:
            continue
        seen.add(k)
        files.append(p)
print("containers scanned: %d (canonical-deduped)" % len(files), flush=True)

nested = []          # (container, instance, size, sha1, {table_instance: table_size})
for n, p in enumerate(files):
    if n % 10 == 0:
        print("  ... %d/%d %s" % (n, len(files), p.name), file=sys.stderr, flush=True)
    try:
        raw = p.read_bytes()
    except Exception:
        continue
    if raw[:4] != b"DBPF":
        continue
    for t, g, i, meta in index(raw):
        if t != PKG_TYPE:
            continue
        d = payload(meta)
        if len(d) < HDR or d[:4] != b"DBPF":
            nested.append((str(p.relative_to(root)), i, len(d), hashlib.sha1(d).hexdigest()[:12], None))
            continue
        tabs = {}
        for t2, g2, i2, m2 in index(d):
            if t2 != LOCALE_TYPE:
                continue
            tabs[i2] = len(payload(m2))
        nested.append((str(p.relative_to(root)), i, len(d), hashlib.sha1(d).hexdigest()[:12], tabs))

print("nested `package` resources found: %d" % len(nested), flush=True)
print("", flush=True)

by_inst = {}
for where, i, sz, dg, tabs in nested:
    by_inst.setdefault(i, []).append((where, sz, dg, tabs))
multi = {i: r for i, r in by_inst.items() if len(r) > 1}
print("=== instances shipped in MORE THAN ONE container: %d ===" % len(multi), flush=True)
for i, rows in sorted(multi.items(), key=lambda kv: -max(len(r[3] or {}) for r in kv[1])):
    print("  0x%08X  %d copies" % (i, len(rows)), flush=True)
    for where, sz, dg, tabs in rows:
        print("     %-46s %9d B  sha1=%s  %s" % (
            where[:46], sz, dg,
            ("%d locale tables" % len(tabs)) if tabs is not None else "NOT a DBPF"), flush=True)
    live = [r for r in rows if r[3] is not None]
    if len(live) > 1:
        sets = [set(r[3]) for r in live]
        common = set.intersection(*sets)
        union = set.union(*sets)
        print("     -> %d table instances common, %d distinct overall, %d unique to one copy"
              % (len(common), len(union), len(union) - len(common)), flush=True)
        for where, sz, dg, tabs in live:
            only = set(tabs) - common
            if only:
                print("        only in %-42s: %s" % (
                    Path(where).name, ", ".join("0x%08X" % x for x in sorted(only)[:14])
                    + (" ...(+%d)" % (len(only) - 14) if len(only) > 14 else "")), flush=True)
        for tid in sorted(common):
            variants = {}
            for where, sz, dg, tabs in live:
                variants.setdefault(tabs[tid], []).append(Path(where).name)
            if len(variants) > 1:
                print("        table 0x%08X SIZE DIFFERS: %s" % (
                    tid, "; ".join("%d B in [%s]" % (k, ", ".join(v))
                                   for k, v in sorted(variants.items(), reverse=True))), flush=True)
print("", flush=True)
print("=== the two caption tables we could not find, anywhere in the tree ===", flush=True)
for name, tid in (("mvj", 0x277DEE6A), ("legal", 0xADB810C4)):
    hits = []
    for where, i, sz, dg, tabs in nested:
        if tabs and tid in tabs:
            hits.append("%s (in nested 0x%08X, %d B)" % (where, i, tabs[tid]))
    print("  %-6s 0x%08X : %s" % (name, tid, "; ".join(hits) if hits else "NOT in any nested package"),
          flush=True)
