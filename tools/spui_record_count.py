"""Decide ONE record: is the third u16 a count?  Target the record that broke the walk.

A coherent walk through the `Children` block of E3LaunchScreen consumed 10 records and then hit
`0B D0 C1 EE | 09 00 | 08 00` - id 0xEEC1D00B (`CaptionColors`, a LIST of colours per the schema),
code 9, third u16 = 8.

Under the project's framing (third u16 is a constant marker 1, payload = element_size) this record is
12 bytes and the next one starts at +12. Under the count hypothesis it is 8 + 4*8 = 40 bytes and the
next one starts at +40. Only one of those lands on a well-formed record (known id, known code, third
u16 plausible), and that is the whole test. The same is then applied to every record of the layout
whose third u16 is not 1, walking from each one, to see which reading keeps the stream coherent.

    python tools/spui_record_count.py
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

CH = struct.pack("<I", 0xEEC1B00B)
start = blob.find(CH)
# walk 10 records to reach the record of interest (sizes: third==1 assumed, which is what got us there)
cur = start + 10
for _ in range(10):
    pid, code, third = struct.unpack_from("<IHH", blob, cur)
    cur += 8 + (ELEM.get(code, 2) if code in ELEM else 2)
pid, code, third = struct.unpack_from("<IHH", blob, cur)
print("record of interest at %d: id=0x%08X code=%d third=%d" % (cur, pid, code, third))
print()


def coherent(off):
    if off + 8 > len(blob):
        return None
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code not in KNOWN:
        return None
    return "id=0x%08X code=%-3d third=%d" % (pid, code, third)


print("if third is a CONSTANT MARKER (today's framing): next record at %d -> %s"
      % (cur + 8 + ELEM[code], coherent(cur + 8 + ELEM[code])))
print("if third is a COUNT (hypothesis):               next record at %d -> %s"
      % (cur + 8 + ELEM[code] * third, coherent(cur + 8 + ELEM[code] * third)))
print()

# Same question for every record in the layout whose third u16 is neither 0 nor 1.
print("=== all records in this layout with third u16 not in {0,1} ===")
cnt_bad = cnt_good = 0
for off in range(0, len(blob) - 8):
    pid, code, third = struct.unpack_from("<IHH", blob, off)
    if code not in ELEM or third in (0, 1):
        continue
    if third > 4096:
        continue
    a = coherent(off + 8 + ELEM[code])            # marker reading
    b = coherent(off + 8 + ELEM[code] * third)    # count reading
    if a is None and b is not None:
        cnt_good += 1
        if cnt_good <= 8:
            print("  @%6d id=0x%08X code=%d third=%d  marker-reading lands on NOTHING; "
                  "count-reading lands on %s" % (off, pid, code, third, b))
    elif a is not None and b is None:
        cnt_bad += 1
        if cnt_bad <= 5:
            print("  @%6d id=0x%08X code=%d third=%d  marker-reading OK (%s); count-reading NOTHING"
                  % (off, pid, code, third, a))
print()
print("records where the COUNT reading is coherent and the marker reading is not: %d" % cnt_good)
print("records where the MARKER reading is coherent and the count reading is not: %d" % cnt_bad)
