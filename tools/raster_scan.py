r"""raster (RenderWare texture) header scan - efficient (reads each package once).

`raster` = type 0x2f4e681c (from the registry), 5,931 resources. Decoded payloads start with a
small header; this tool collects that header across many resources to derive the structure, the way
gmdl was derived. It does NOT re-read the package per record (the first version did, which is why a
full scan never finished).

Observed so far (Spore_Content rec[0]/rec[1]):
    01 00 00 00   u32 = 1
    00 02 00 00   u32 = 512   (height? width?)
    00 02 00 00   u32 = 512   (the other dimension)
    0a 00 00 00   u32 = 10    (mip/format code?)
    08 00 00 00   u32 = 8     (a count?)
    then either "DXT5\0" (block-compressed) or 8 bytes then raw BGRA-ish pixels

Usage: python tools/raster_scan.py [--limit 3000]
"""
from __future__ import annotations

import argparse
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

RASTER = 0x2F4E681C


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--limit", type=int, default=3000)
    a = ap.parse_args()

    from tools.gmdl import all_packages
    from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
    from tools.refpack import decode

    seen = Counter()
    dims = Counter()
    tags = Counter()
    n = 0
    for p in all_packages():
        if n >= a.limit:
            break
        raw = p.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        if kind not in KINDS:
            continue
        prefix, stride, fld = KINDS[kind]
        pw = struct.unpack("<%dI" % (prefix // 4), raw[use : use + prefix])
        shared, q = {}, 1
        for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
            if kind & bit:
                shared[nm] = pw[q]
                q += 1
        for r in range(count):
            w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride :][:stride])
            t = shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0)
            if t != RASTER:
                continue
            off = w[fld["offset"]]
            packed = w[fld["size"]] & 0x7FFFFFFF
            mem = w[fld["usize"]]
            comp = w[fld["sentinel"]] & 0xFFFF
            payload = raw[off : off + packed]
            if comp == 0xFFFF:
                try:
                    data = decode(payload, mem)
                except Exception:  # noqa: BLE001
                    continue
            else:
                data = payload
            if len(data) < 24:
                continue
            hdr = struct.unpack("<6I", data[:24])
            seen[(hdr[0], hdr[3], hdr[4], hdr[5])] += 1
            dims[(hdr[1], hdr[2])] += 1
            tag = data[20:24]
            if all(32 <= b < 127 for b in tag):
                tags[tag.decode("latin-1")] += 1
            else:
                tags["<binary:%02x%02x%02x%02x>" % tuple(tag)] += 1
            n += 1
            if n >= a.limit:
                break
    print(f"# raster records scanned: {n}")
    print("\n# header word combos (w0,w3,w4,w5) -> count")
    for k, v in seen.most_common(12):
        print(f"   {k}  x{v}")
    print("\n# (w1,w2) dimension-ish pairs -> count")
    for k, v in dims.most_common(12):
        print(f"   {k}  x{v}")
    print("\n# bytes[20:24] tag -> count")
    for k, v in tags.most_common(12):
        print(f"   {k!r}  x{v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
