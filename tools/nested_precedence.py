"""Which nested en-us locale wins where PatchData and EP1 disagree?

tools/nested_every.py found that Data/PatchData.package carries its OWN nested locale packages, one
per language, with 42 en-us tables against EP1's 65 - and that the two overlap on exactly ONE table
instance, 0xADE7FFAA, with different bytes (8,414 B patched vs 10,543 B in EP1). Every other table
instance is unique to one source, so this single table IS the entire precedence question for the
corpus.

text.cpp fills m_by_table by assignment, so whichever container is parsed LAST wins. That order comes
from fs::search_paths(), not from any rule we chose - which means the winner is accidental. A patch
exists to override base content, so PatchData SHOULD win; if EP1 wins we are displaying pre-patch
text and would never notice, because both versions are plausible strings.

So: dump both versions of the table, key by key, and show what each side says.
"""
import sys, struct, re
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

PKG_TYPE, LOCALE_TYPE = 0x06EFC6AA, 0x02FAC0B6
EN, HOT = 0x7080DB55, 0xADE7FFAA
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")


def index(raw):
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
        yield (shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0),
               w[fld["instance"]] if "instance" in fld else 0, (raw, w, fld))


def payload(meta):
    raw, w, fld = meta
    off = w[fld["offset"]]
    d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
    if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
        try:
            d = decode(d, w[fld["usize"]])
        except Exception:
            pass
    return d


def keys_of(text_bytes):
    """Parse a `locale` resource into {key: text}, using the documented grammar."""
    b = text_bytes
    if b[:3] == b"\xef\xbb\xbf":
        b = b[3:]
    out, cur = {}, None
    for line in b.decode("utf-8", "replace").replace("\r\n", "\n").split("\n"):
        s = line.lstrip(" \t")
        if not s:
            cur = None
            continue
        if s.startswith("#"):
            continue
        m = re.match(r"^0x([0-9A-Fa-f]{8})[\t ]*(.*)$", s)
        if m:
            body = m.group(2)
            mk = re.match(r"^~([^~]*)~[\t ]*", body)
            markup = ""
            if mk:
                markup, body = mk.group(1), body[mk.end():]
            cur = int(m.group(1), 16)
            out[cur] = (body.strip(), markup)
        elif cur is not None:
            t, _mk = out[cur]
            out[cur] = (t + "\n" + s.strip(), _mk)
    return out


want = {}
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        if "PatchData" not in p.name and "EP1_Locale" not in p.name:
            continue
        raw = p.read_bytes()
        if raw[:4] != b"DBPF":
            continue
        for t, i, meta in index(raw):
            if t != PKG_TYPE or i != EN:
                continue
            d = payload(meta)
            if len(d) < HDR or d[:4] != b"DBPF":
                continue
            for t2, i2, m2 in index(d):
                if t2 == LOCALE_TYPE and i2 == HOT:
                    want.setdefault(p.name, []).append(keys_of(payload(m2)))

print("en-us nested copies of table 0x" + format(HOT, "08X") + ":")
for nm, vers in want.items():
    print("   %-34s %d keys" % (nm, len(vers[0])))
if len(want) < 2:
    print("not a conflict after all - only one source has it")
    sys.exit(0)

a = list(want["PatchData.package"])[0]
b = list(v for nm, vs in want.items() if nm.startswith("Spore_EP1") for v in vs)[0]
common = set(a) & set(b)
print()
print("   PatchData keys : %d" % len(a))
print("   EP1 keys       : %d" % len(b))
print("   common         : %d" % len(common))
print("   differing text : %d" % sum(1 for k in common if a[k] != b[k]))
print()
print("=== the differences (this is what precedence decides) ===")
n = 0
for k in sorted(common):
    if a[k] == b[k]:
        continue
    n += 1
    if n > 14:
        print("   ... and %d more" % (len(common) - n + 1))
        break
    print("   key 0x%08X" % k)
    print("      patch : %r" % (a[k][0][:78],))
    print("      ep1   : %r" % (b[k][0][:78],))
print()
print("=== keys only on one side ===")
print("   only PatchData: %s" % (", ".join("0x%08X" % k for k in sorted(set(a) - set(b))[:12]) or "none"))
print("   only EP1      : %s" % (", ".join("0x%08X" % k for k in sorted(set(b) - set(a))[:12]) or "none"))
