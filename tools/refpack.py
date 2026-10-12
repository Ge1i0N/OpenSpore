r"""RefPack (EA LZRef) decoder, written from the original's own behaviour, not from a wiki.

Why this file exists: the first resource we ever dumped (res0.bin, from
`pkg:-dump,0,3` of Data\Locale\cs-cz\NameGeneration.package) looked like plaintext Czech
and I therefore concluded "0x80000000 is not compression". That was WRONG: the stream
begins `10 fb`, i.e. it IS RefPack, and the readable text was just the first literal block.

Header, verified against the shipped file rather than any document:
    10 FB <size:3 bytes>  ...
  size is BIG-ENDIAN: int.from_bytes(d[2:5],'big') == 2120 == the index record's memSize,
  while little-endian would give 4,720,640 (absurd for a 1371-byte payload).
  (The SporeMaster3 doc claims LE; it is wrong - recorded in IDEAS.md §5.)

Command forms, by control byte c:
    0x00..0x7F  2-byte: copy, size = (c>>2)+4, offset = ((c&3)<<8)|b1
    0x80..0xBF  3-byte: size = (c&0x3F)+4,  offset = (b1<<8)|b2
    0xC0..0xDF  4-byte: size = (c&0x3F)+1,  offset = (b1<<8)|b2
    0xE0..0xFB  literal block, size = ((c&0x1F)<<2)+4
    0xFC..0xFF  tail: (c&0x03) literal bytes, then end
Copies may overlap (RLE semantics), so a copy with size>offset is done byte by byte.

Usage: python tools/refpack.py <file> [--out out.bin]
"""
from __future__ import annotations

import sys
from pathlib import Path


def decode(data: bytes, expect: int | None = None) -> bytes:
    if len(data) < 5 or data[0] != 0x10 or data[1] != 0xFB:
        raise ValueError(f"not a RefPack stream: {data[:4].hex(' ')}")
    size = int.from_bytes(data[2:5], "big")
    if expect is not None and size != expect:
        raise ValueError(f"header size {size} != expected {expect}")
    out = bytearray()
    i = 5
    n = len(data)
    while i < n:
        c = data[i]
        i += 1
        if c < 0x80:
            cnt = (c >> 2) + 4
            off = ((c & 3) << 8) | data[i]
            i += 1
        elif c < 0xC0:
            cnt = (c & 0x3F) + 4
            off = (data[i] << 8) | data[i + 1]
            i += 2
        elif c < 0xE0:
            # 4-byte form carries a 16-bit length: ((c & 0x1F) << 8 | b0) + 1.
            # The 6-bit reading ((c&0x3F)+1) is what the first version of this decoder used,
            # and it faulted on res0.bin at input byte 28 with offset 522 > output length.
            cnt = (((c & 0x1F) << 8) | data[i]) + 1
            off = (data[i + 1] << 8) | data[i + 2]
            i += 3
        elif c < 0xFC:
            cnt = ((c & 0x1F) << 2) + 4
            out += data[i : i + cnt]
            i += cnt
            continue
        else:
            cnt = c & 0x03
            out += data[i : i + cnt]
            i += cnt
            break
        if off == 0 or off > len(out):
            raise ValueError(f"bad copy offset {off} at input {i}")
        src = len(out) - off
        if cnt > off:
            for k in range(cnt):
                out.append(out[src + k])
        else:
            out += out[src : src + cnt]
    if expect is not None and len(out) != expect:
        raise ValueError(f"decoded {len(out)} bytes, expected {expect}")
    return bytes(out)


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    p = Path(sys.argv[1])
    data = p.read_bytes()
    expect = None
    for a in sys.argv[2:]:
        if a.isdigit():
            expect = int(a)
    out = decode(data, expect)
    print(f"input  : {p} {len(data):,} bytes")
    print(f"header : 10 FB {data[2:5].hex(' ')} -> size(BE) = {int.from_bytes(data[2:5],'big'):,}")
    print(f"decoded: {len(out):,} bytes  ratio {len(data)/len(out):.3f}")
    dst = Path(sys.argv[sys.argv.index("--out") + 1]) if "--out" in sys.argv else p.with_suffix(".dec.bin")
    dst.write_bytes(out)
    print(f"wrote  : {dst}")
    print(f"first 32: {out[:32].hex(' ')}")
    printable = sum(1 for b in out[:256] if 32 <= b < 127 or b in (9, 10, 13))
    print(f"first 256 bytes: {printable} printable ({100*printable/256:.0f}%)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
