"""Is the widget HIERARCHY in the layout data? Look for the schema's structural properties.

The designer schema (extracted from the shipped `xml` resources) defines, in one group:

    ControlID   0xeec1b001
    CommandID   0xeec1b002
    Children    0xeec1b00b

`Children` is the one that matters: if the `spui` record stream carries it, the layout's real
parent/child tree is available, and the engine's current GUESSES (paint largest-first, hit-test
smallest-containing) can be replaced by the tree the designer actually built.

Record framing, measured over 1,098,129 anchors:  id:u32 | code:u16 | marker:u16(==1) | payload[code]
so a property record is found by its id followed by a plausible code and the marker 0x0001. This scans
for the three ids and prints the following bytes raw, because the INTERPRETATION is what we are here to
find out - printing an interpretation first would be assuming the answer.

    python tools/spui_children.py
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF   # noqa: E402
from tools.refpack import decode                                       # noqa: E402

SPUI = 0x250FE9A2
WANT = {0xEEC1B001: "ControlID", 0xEEC1B002: "CommandID", 0xEEC1B00B: "Children",
        0xEEC1D005: "ButtonGroupID", 0xEEC1B007: "SliderDrawable"}
LAYOUT = 0xE932BD44          # E3LaunchScreen, the screen the state script names
ROOT = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")


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


found = None
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
                found = (p.name, payload(meta))
                break
        if found:
            break
    if found:
        break

if not found:
    print("layout 0x%08X not found" % LAYOUT)
    sys.exit(1)

name, blob = found
print("layout 0x%08X from %s, %d bytes" % (LAYOUT, name, len(blob)))
print()

counts = {k: 0 for k in WANT}
for pid, label in WANT.items():
    needle = struct.pack("<I", pid)
    hits, at = [], blob.find(needle)
    while at >= 0:
        hits.append(at)
        at = blob.find(needle, at + 1)
    counts[pid] = len(hits)
    print("=== %s (0x%08X): %d occurrence(s) ===" % (label, pid, len(hits)))
    for h in hits[:6]:
        code, marker = struct.unpack_from("<HH", blob, h + 4)
        tail = blob[h + 8:h + 8 + 24]
        hexs = " ".join("%02X" % c for c in tail)
        u32 = struct.unpack_from("<6I", blob + b"\0" * 24, h + 8) if h + 32 <= len(blob) else ()
        print("   @%6d  code=%-3d marker=0x%04X  next24: %s" % (h, code, marker, hexs))
        if u32:
            print("             as u32: %s" % " ".join("0x%08X" % v for v in u32))
        # For the array code (19) the payload is a u16 COUNT; what follows decides whether the
        # elements are inline records or a run of ids. Print the bytes and the u32 view so the shape
        # is read rather than assumed.
        if code == 19:
            cnt = struct.unpack_from("<H", blob, h + 8)[0]
            after = blob[h + 10:h + 10 + 96]
            print("             code 19: u16 count = %d (0x%X); next 96 bytes:" % (cnt, cnt))
            for r in range(0, len(after) - 3, 16):
                row = after[r:r + 16]
                print("               %s   | %s" % (
                    " ".join("%02X" % c for c in row),
                    " ".join("0x%08X" % v for v in struct.unpack_from("<4I", row + b"\0" * 4))))
    print()
