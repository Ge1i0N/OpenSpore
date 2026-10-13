"""RenderWare 4 stream header: what is CONFIRMED, and nothing more.

RenderWare 4 is not the RW 3.x chunk model the field guide's section 3 assumes, so this tool
only reports what has been proven from the binary and the shipped bytes. It does NOT guess the
object table that follows the header.

CONFIRMED SO FAR
  1. A 28-byte header template lives at .rdata 0x014f6480 and is copied wholesale by the
     constructor at 0x011e3f60:
         11e3f72: mov  ecx,0x7
         11e3f77: mov  esi,0x14f6480
         11e3f7c: mov  edi,ebx
         11e3f7e: rep  movs DWORD PTR es:[edi],DWORD PTR ds:[esi]      ; 7 dwords = 28 bytes
     The template dwords are:
         +0x00  0x34575289  "\x89RW4"
         +0x04  0x00323377  "w32\0"
         +0x08  0x0a1a0a0d  "\r\n\x1a\n"   (EA's usual end-of-header marker)
         +0x0c  0x00042000
         +0x10  0x00343534  "454\0"        (version/build string)
         +0x14  0x00303030  "000\0"
         +0x18  0x00000000
  2. After copying the template the constructor fills, in order:
         [obj+0x1c] = arg,  [obj+0x20] = arg,  [obj+0x24] = 0
         [obj+0x30] = a computed, 4-byte-aligned stream size
         [obj+0x34] = arg
         [obj+0x38] = obj (self pointer)
         [obj+0x44 .. +0x63] (8 dwords) from a sub-call, [obj+0x64 .. +0x83] (8 dwords), and
         [obj+0x84 .. +0x93] (4 dwords) from the caller's array
  3. The constructor's caller that writes an rw4 stream (0x006c24b3) pushes the sentinel
     0xcafed00d, which is RW4's own validation constant.
  4. The rw4 resource type id is 0x2F4E681B and is passed to a resource accessor at 0x004021b3.

NOT REVERSED: the object/chunk table after the header, the compile flags at +0x0c, and whether
+0x10 "454" is a version or a build tag. Do not fill those in from a third-party RW4 description
without testing against these bytes first.

Usage:
    python tools/rw4.py --pkg <i> --rec <n>     # header of one decoded rw4 resource
    python tools/rw4.py --file decoded.bin      # header of a raw decoded payload
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

TEMPLATE_VA = 0x014F6480
HEADER_LEN = 28


def parse_header(data: bytes) -> dict:
    if len(data) < HEADER_LEN:
        raise ValueError(f"too short: {len(data)} < {HEADER_LEN}")
    d = struct.unpack("<7I", data[:HEADER_LEN])
    return {
        "magic0": d[0],
        "magic0_ok": d[0] == 0x34575289,
        "plat": bytes(data[4:8]).rstrip(b"\x00").decode("latin-1"),
        "marker_ok": d[2] == 0x0A1A0A0D,
        "flags_0c": d[3],
        "version": bytes(data[16:20]).rstrip(b"\x00").decode("latin-1"),
        "build": bytes(data[20:24]).rstrip(b"\x00").decode("latin-1"),
        "tail_18": d[6],
        "dwords": d,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--file")
    ap.add_argument("--pkg", type=int)
    ap.add_argument("--rec", type=int, default=0)
    ap.add_argument("--len", type=int, default=128)
    a = ap.parse_args()

    if a.file:
        data = Path(a.file).read_bytes()
        label = a.file
    else:
        from tools.pkg_kinds import DIRS, HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE
        from tools.refpack import decode

        allpk: list[Path] = []
        seen = set()
        for d in DIRS:
            if not d.is_dir():
                continue
            for p in sorted(d.rglob("*.package")):
                k = str(p.resolve())
                if k not in seen:
                    seen.add(k)
                    allpk.append(p)
        if a.pkg is None:
            ap.print_help()
            return 2
        p = allpk[a.pkg]
        raw = p.read_bytes()
        h = raw[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        prefix, stride, fld = KINDS[kind]
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + a.rec * stride :][:stride])
        off = w[fld["offset"]]
        packed = w[fld["size"]] & 0x7FFFFFFF
        mem = w[fld["usize"]]
        comp = w[fld["sentinel"]] & 0xFFFF
        payload = raw[off : off + packed]
        data = decode(payload, mem) if comp == 0xFFFF else payload
        label = f"{p.name} rec[{a.rec}]"

    print(f"# {label}: {len(data)} bytes")
    for i in range(0, min(a.len, len(data)), 16):
        row = data[i : i + 16]
        print(f"  {i:06x}  {row.hex(' '):<47} |" + "".join(chr(b) if 32 <= b < 127 else "." for b in row) + "|")
    print()
    try:
        h = parse_header(data)
    except ValueError as e:
        print(f"! not an RW4 stream: {e}")
        return 1
    print("# header (28 bytes, template at .rdata 0x014f6480, copied by 0x011e3f60)")
    print(f"  magic0    0x{h['magic0']:08x}  {'OK' if h['magic0_ok'] else 'MISMATCH'}")
    print(f"  platform  {h['plat']!r}")
    print(f"  marker    {'OK' if h['marker_ok'] else 'MISMATCH'} (\\r\\n\\x1a\\n)")
    print(f"  flags_0c  0x{h['flags_0c']:08x}")
    print(f"  version   {h['version']!r}")
    print(f"  build     {h['build']!r}")
    print(f"  tail_18   0x{h['tail_18']:08x}")
    ok = h["magic0_ok"] and h["marker_ok"]
    print(f"\nVERDICT: {'valid RW4 header' if ok else 'NOT a valid RW4 header'}")
    print("(the object table after the header is NOT reversed yet; this tool stops here)")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
