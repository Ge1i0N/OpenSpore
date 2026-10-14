"""Does the u16 after a code-19 record mean "this many RECORDS follow"?

That is the one question standing between us and the real widget tree. The `Children` property
(0xEEC1B00B) appears 13 times in E3LaunchScreen with code=19 and a u16 right after the header, and the
bytes that follow are a nested block of the children's own property records. If the u16 counts RECORDS,
then walking exactly that many records from the end of the header must land on a coherent boundary -
the next record of the enclosing scope, with a valid id, a known code and marker==1.

This is a falsifiable test, not a guess: walk N records and report where it lands and whether the
stream is still coherent. If it derails, the count means something else (elements, bytes, or a length
prefix for a sub-stream) and the tree has to be walked structurally instead.

Payload sizes are the ones measured over 1,098,129 anchors (tools/spui_read.py, analysis/spui-schema.md):
  9, 11 -> 4 bytes      16 -> 16 (rectf)     17 -> 8 (pointf)     20 -> 6
  19    -> u16 count, then a NESTED block (recursed here)
  18    -> u16 char count then UTF-16, or 0xFFFF then table:u32|key:u32

    python tools/spui_tree.py
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
CHILDREN = 0xEEC1B00B

# code -> fixed payload size in bytes, or None when the payload needs its own logic
FIXED = {9: 4, 11: 4, 16: 16, 17: 8, 20: 6}
KNOWN_CODES = set(FIXED) | {18, 19}


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


def read_record(off, depth=0):
    """Return (id, code, marker, payload_len, next_off) or None when the bytes are not a record."""
    if off + 8 > len(blob):
        return None
    pid, code, marker = struct.unpack_from("<IHH", blob, off)
    if marker != 0x0001 or code not in KNOWN_CODES:
        return None
    if code in FIXED:
        plen = FIXED[code]
    elif code == 18:
        n = struct.unpack_from("<H", blob, off + 8)[0]
        plen = 10 if n == 0xFFFF else 2 + 2 * n
    elif code == 19:
        n = struct.unpack_from("<H", blob, off + 8)[0]
        plen = 2 + n            # count field, then n bytes?? - reported, not assumed
        # A code-19 record's payload is just the u16 count in the flat reading; the nested block is
        # handled by the caller so the count's meaning can be tested rather than baked in.
        plen = 2
    return pid, code, marker, plen, off + 8 + plen


print("layout %d bytes; walking records from a Children record" % len(blob))
print()

# Find the first Children record and test the count as a RECORD count.
at = blob.find(struct.pack("<I", CHILDREN))
tests = []
while at >= 0:
    tests.append(at)
    at = blob.find(struct.pack("<I", CHILDREN), at + 1)

for idx0, off in enumerate(tests[:4]):
    pid, code, marker = struct.unpack_from("<IHH", blob, off)
    cnt = struct.unpack_from("<H", blob, off + 8)[0]
    print("--- Children #%d at %d: code=%d count=%d ---" % (idx0, off, code, cnt))
    # Walk `cnt` records starting right after the count field, and see where we land.
    cur = off + 10
    ok = 0
    for k in range(cnt):
        r = read_record(cur)
        if r is None:
            print("    derailed after %d of %d records at offset %d (bytes %s)"
                  % (k, cnt, cur, blob[cur:cur + 8].hex()))
            break
        _pid, _code, _mk, plen, nxt = r
        cur = nxt
        ok += 1
    else:
        nxt = read_record(cur)
        land = "COHERENT next record id=0x%08X code=%d" % (nxt[0], nxt[1]) if nxt else "no record"
        print("    consumed all %d records, landed at %d: %s" % (cnt, cur, land))
        print("    => the u16 reads as a RECORD COUNT" if nxt else "    => landed on nothing readable")
    if ok < cnt:
        print("    => the u16 does NOT count records (derailed at %d)" % ok)
    print()
