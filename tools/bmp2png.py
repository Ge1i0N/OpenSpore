r"""Convert a 32-bit BMP (the in-engine capture format) to PNG, no third-party deps.

The engine's capture_frame writes 32bpp BI_RGB, bottom-up, BGRA (D3D A8R8G8B8 in memory). This
is a viewing aid for that file; it is not part of the renderer.

Usage: python tools/bmp2png.py cap.bmp cap.png
"""
from __future__ import annotations

import struct
import sys
import zlib
from pathlib import Path


def chunk(tag: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


def main() -> int:
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    b = src.read_bytes()
    if b[:2] != b"BM":
        print("not a BMP")
        return 1
    off = struct.unpack_from("<I", b, 10)[0]
    w = struct.unpack_from("<i", b, 18)[0]
    h = struct.unpack_from("<i", b, 22)[0]
    bpp = struct.unpack_from("<H", b, 28)[0]
    if bpp != 32:
        print(f"only 32bpp supported, got {bpp}")
        return 1
    bottom_up = h > 0
    h = abs(h)
    row = w * 4
    raw = bytearray()
    for y in range(h):
        sy = (h - 1 - y) if bottom_up else y
        line = b[off + sy * row : off + sy * row + row]
        raw.append(0)  # filter: none
        for x in range(w):
            bb, gg, rr, aa = line[x * 4 : x * 4 + 4]
            raw += bytes((rr, gg, bb))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    png += chunk(b"IEND", b"")
    dst.write_bytes(png)
    print(f"wrote {dst} ({w}x{h})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
