"""Settle the u16 after a code-19 record: is it a BYTE LENGTH of the nested block?

"Records" is already ruled out (tools/spui_tree.py: walking that many records derails after 10). The
remaining readings are "byte length", "element count" and "child count". Test byte length, because it
is the one that makes a walk possible: if the nested block is exactly N bytes, the record after it
starts at off + 10 + N and must be well formed.

Applied to every code-19 record in E3LaunchScreen, not just `Children`, so the answer is about the
FORMAT rather than about one property. A hypothesis that holds for all of them is a fact; one that
holds for some is a coincidence to report as such.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF   # noqa: E402
from tools.refpack import decode                                       # noqa: E402

SPUI = 0x250FE9A2
LAYOUT = 0xE932BD44
ROOT = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
ELEM = {2: 1, 5: 4, 7: 7, 9: 4, 11: 4, 16: 16, 17: 8, 20: 6}
VAR = {18, 19}
KNOWN = set(ELEM) | VAR
CHILDREN = 0xEEC1B00B


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


blob = None
for pat in ("*.package", "*.pck"):
    for p in sorted(ROOT.rglob(pat)):
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, i, meta in index(raw):
            if t == SPUI and i == LAYOUT:
                blob = payload(meta)
                break
        if blob:
            break
    if blob:
        break


def coherent(off):
    if off + 8 > len(blob):
        return None
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code not in KNOWN or third == 0 or third > 4096:
        return None
    return "id=0x%08X code=%-3d count=%d" % (pid, code, third)


# every code-19 record whose header is a known id with count==1 (the array form)
hits = []
for off in range(0, len(blob) - 10):
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code == 19 and third == 1 and pid in (CHILDREN, 0xEEC1B007, 0x0F3AC75E):
        hits.append((off, pid))
# dedupe overlapping positions
clean = []
last = -1
for off, pid in hits:
    if off < last:
        continue
    clean.append((off, pid))
    last = off + 10

print("code-19 records examined: %d (Children=%d)"
      % (len(clean), sum(1 for _o, p in clean if p == CHILDREN)))
print()
print("%-8s %-12s %-6s %-34s %-34s" % ("off", "id", "u16", "next if u16 = BYTE LENGTH",
                                       "next if u16 = ELEMENT COUNT(4B)"))
bl_ok = ec_ok = 0
for off, pid in clean[:14]:
    n = struct.unpack_from("<H", blob, off + 8)[0]
    a = coherent(off + 10 + n)
    b = coherent(off + 10 + 4 * n)
    bl_ok += 1 if a else 0
    ec_ok += 1 if b else 0
    print("%-8d 0x%08X %-6d %-34s %-34s" % (off, pid, n, a or "-", b or "-"))
print()
print("BYTE LENGTH reading lands on a well-formed record: %d of %d" % (bl_ok, min(14, len(clean))))
print("ELEMENT-COUNT(4B) reading lands on a well-formed record: %d of %d"
      % (ec_ok, min(14, len(clean))))
