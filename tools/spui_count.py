"""Is the third u16 a COUNT, not a constant marker?  This changes how records are sized.

The project's framing has been `id:u32 | code:u16 | marker:u16(==1) | payload[code]`, with "marker
0x0001 in 98.4% of 1,098,129 anchors". A walk of the `Children` block derailed at a record whose third
u16 was **0x0008**, not 1 - id `0xEEC1D00B` = `CaptionColors`, which the schema says is a list of
colours. Eight colours, count 8.

So the hypothesis under test is: the third u16 is an ELEMENT COUNT, the payload is `count * element_size`
for fixed-size codes, and "marker == 1" was an artefact of nearly every scalar property having exactly
one element. If true, two things follow at once:
  * ~136,769 anchors we currently skip (1,098,129 - 961,360) become readable;
  * records whose count is not 1 are sized WRONGLY today, which mis-aligns everything after them -
    a strong candidate for the "sequential walk derails in 637 of 748 layouts" wall.

The test is falsifiable and cheap: walk forward under each hypothesis and count how many records stay
coherent (valid id, known code, plausible count). Report where each one derails.

    python tools/spui_count.py
"""
import struct
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF   # noqa: E402
from tools.refpack import decode                                       # noqa: E402

SPUI = 0x250FE9A2
LAYOUT = 0xE932BD44
ROOT = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")

# element size per code, from the measured sizes over 1,098,129 anchors
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
if blob is None:
    print("layout not found")
    sys.exit(1)


def step(off, count_aware):
    """Return the next offset if the record at `off` parses, else None."""
    if off + 8 > len(blob):
        return None
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code not in KNOWN:
        return None
    if code in VAR:
        if third != 1:               # a variable-length record is one element
            return None
        if code == 19:
            n = struct.unpack_from("<H", blob, off + 8)[0]
            return off + 10 + n
        n = struct.unpack_from("<H", blob, off + 8)[0]
        return off + 10 + (10 if n == 0xFFFF else 2 * n)
    cnt = third if count_aware else 1
    if count_aware and (cnt == 0 or cnt > 4096):
        return None
    return off + 8 + ELEM[code] * cnt


third_hist = Counter()
for off in range(0, len(blob) - 8, 4):
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code in KNOWN:
        third_hist[third] += 1

print("layout %d bytes" % len(blob))
print("third-u16 histogram over %d record-shaped positions (top 8):" % sum(third_hist.values()))
for v, n in third_hist.most_common(8):
    print("   third=%-6d %d" % (v, n))
print()

for label, aware in (("count-aware sizing", True), ("marker==1 sizing (today)", False)):
    best = 0
    best_at = 0
    for start in (0, 8, 16, 24, 32, 64, 128, 256, 512):
        off, n = start, 0
        while True:
            nxt = step(off, aware)
            if nxt is None:
                break
            off, n = nxt, n + 1
            if n > 100000:
                break
        if n > best:
            best, best_at = n, start
    print("%-26s best walk: %d records from offset %d, then derails at %d (bytes %s)"
          % (label, best, best_at, off, blob[off:off + 8].hex() if off + 8 <= len(blob) else "-"))
