"""Does count-aware sizing fix the sequential walk that derails in 637 of 748 layouts?

The project's framing treated the third u16 as a constant marker (==1) and therefore sized every record
as 8 + element_size. That is now disproven for multi-element properties (24 records where the count
reading is coherent and the marker reading is not, 0 the other way - tools/spui_record_count.py).

If the mis-sizing is what threw the walk off, then walking with `8 + element_size * count` should get
substantially further. If it does not, the wall is the VOCABULARY (unknown ids), as previously believed,
and this correction is about coverage rather than about the walk. Either answer is worth having, and
they have different consequences - so measure instead of assuming.

Walks start from every position that looks like a record header and the longest run is reported, since
the stream has no framing header at offset 0.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF   # noqa: E402
from tools.refpack import decode                                       # noqa: E402

SPUI = 0x250FE9A2
ROOT = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")
ELEM = {9: 4, 11: 4, 16: 16, 17: 8, 20: 6}
VAR = {18, 19}
KNOWN = set(ELEM) | VAR


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


def step(blob, off, aware):
    if off + 8 > len(blob):
        return None
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code not in KNOWN:
        return None
    if code in VAR:
        if third != 1:
            return None
        n = struct.unpack_from("<H", blob, off + 8)[0]
        return off + 10 + (n if code == 19 else (10 if n == 0xFFFF else 2 * n))
    cnt = third if aware else 1
    if cnt == 0 or cnt > 4096:
        return None
    return off + 8 + ELEM[code] * cnt


# a sample of layouts, so the answer is not one screen's quirk
want = [0xE932BD44, 0x93AB9D4B, 0x562AB2CC]
blobs = {}
for pat in ("*.package", "*.pck"):
    for p in sorted(ROOT.rglob(pat)):
        try:
            raw = p.read_bytes()
        except Exception:
            continue
        if raw[:4] != b"DBPF":
            continue
        for t, i, meta in index(raw):
            if t == SPUI and i in want and i not in blobs:
                blobs[i] = (p.name, payload(meta))
        if len(blobs) == len(want):
            break
    if len(blobs) == len(want):
        break

for inst, (pkg, blob) in blobs.items():
    print("=== layout 0x%08X from %s (%d bytes) ===" % (inst, pkg, len(blob)))
    for label, aware in (("count-aware", True), ("marker==1  ", False)):
        best, best_at, best_end = 0, 0, 0
        # try every 4-byte offset as a start; the stream has no header at 0
        for start in range(0, min(len(blob) - 8, 4096), 4):
            off, n = start, 0
            while True:
                nxt = step(blob, off, aware)
                if nxt is None:
                    break
                off, n = nxt, n + 1
                if off > len(blob):
                    break
            if n > best:
                best, best_at, best_end = n, start, off
        frac = 100.0 * best_end / len(blob)
        print("   %s: longest run %4d records from offset %5d, reaching %6d = %5.1f%% of the layout"
              % (label, best, best_at, best_end, frac))
    print()
