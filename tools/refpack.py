"""RefPack (EA LZRef) decoder, derived from the ORIGINAL's instructions.

Every rule below is quoted from refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe. Nothing here
comes from a wiki or a third-party document - two such documents were checked and BOTH had
the copy-form table wrong (see IDEAS.md), which is why this file re-derives it.

Entry points (the chain, verified with tools/callers.py):
  0x008d8b3b  package-side: calls the detector 0x92cd40, then 0x92ccd0
  0x0092cd40  detector: loads the word, byte-swaps it, tests (& 0x1FFF) == 0x10FB
  0x0092ccd0  header: reads the length, bounds-checks it, then calls 0x92c5b0
  0x0092c5b0  the copy/literal loop

HEADER (0x0092ccd0), quoted:
  92ccd5: mov  ax,WORD PTR [esi]        ; LE load of the 2 magic bytes
  92ccd8: mov  ch,al / mov cl,ah        ; byte-swap
  92ccdc: movzx eax,cx                  ; eax = big-endian magic, e.g. 0x10FB
  92cce1: and  edx,0x1fff
  92cce7: cmp  edx,0x10fb / jne fail    ; signature test on the low 13 bits
  92ccef: and  eax,0x8000 ... add eax,4 ; bit15 set -> 4, clear -> 3   (length width)
  92cd01: lea  eax,[esi+2]
  92cd05: call 0x92c4e0                 ; read that many bytes BIG-ENDIAN
So: magic = BE16, then a 3- or 4-byte BIG-endian uncompressed length at +2.
(0x92c4e0 is the BE reader: `shl eax,8 / add eax,next` per byte - big-endian by
construction. The upstream SporeMaster3 doc says the length is LITTLE-endian; it is wrong.)

COMMAND LOOP (0x0092c5b0), control byte c, quoted per form:

  c < 0x80  (bit7 clear)  -> 2-byte form, one extra byte b1:
      92c626: and edi,0x3               ; literals = c & 3      (copied first)
      92c652: shr esi,2 / and esi,0x7 / add esi,0x2   ; len = ((c>>2)&7) + 2
      92c63d: and eax,0x60 / add eax,eax x3 ; (c&0x60)*8 = ((c>>5)&3)*256
      92c64a: sub edi,b1 / dec edi      ; dist = ((c>>5)&3)*256 + b1 + 1

  0x80 <= c < 0xC0 (bit7 set, bit6 clear) -> 3-byte form, two extra bytes b0,b1:
      92c686: shr esi,6                 ; literals = b0 >> 6    (copied first)
      92c6b1: and edi,0x3f / add edi,0x3 ; len = (c & 0x3F) + 3
      92c69d: and edi,0x3f / shl edi,8 ... sub b1 / dec ; dist = ((b0&0x3F)<<8) + b1 + 1

  0xC0 <= c < 0xE0 (bits 7,6 set, bit5 clear) -> 4-byte form, three extra bytes b0,b1,b2:
      92c6e6: and esi,0x3               ; literals = c & 3      (copied first)
      92c718: and edi,0xc / shl edi,6 / lea edi,[edi+b2+4] ; len = ((c>>2)&3)*64 + b2 + 4
      92c6fd: and eax,0x10 / shl eax,0xc ; (c&0x10)<<12 = bit16 of the distance
      dist = ((c&0x10)<<12) + (b0<<8) + b1 + 1        ; 17-bit distance

  c >= 0xE0:
      92c73b: and esi,0x1f / lea esi,[esi*4+4]  ; len = (c & 0x1F)*4 + 4   (4..124)
      92c745: cmp esi,0x70 / ja tail            ; len > 112  <=>  c >= 0xFC
      so 0xE0..0xFB = literal block of len bytes;  0xFC..0xFF = tail:
      92c765: and esi,0x3                       ; literals = c & 3, then return

All copy loops are byte-by-byte (`mov al,[edi] / mov [edx],al / inc / dec / jne`), so a
match may overlap the destination - RLE semantics, not memmove semantics.

VERIFICATION: tools/refpack.py --verify decodes real records from the shipped packages and
requires the output length to equal the index's memSize for every one. A wrong table cannot
produce the exact declared length across tens of thousands of independent streams.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


class RefPackError(Exception):
    pass


def decode(data: bytes, expect: int | None = None) -> bytes:
    if len(data) < 2:
        raise RefPackError("too short for a RefPack marker")
    # The detector (0x0092cd40) byte-swaps the word and tests (& 0x1FFF) == 0x10FB, so the
    # top bits are FLAGS, not part of the signature: the shipped packages contain both
    # 0x10FB and 0x50FB streams (0x50FB has bit14 and bit12 set). A strict data[0]==0x10
    # check rejected 50 of the first 20,000 real records - all of them 0x50FB.
    word = (data[0] << 8) | data[1]
    if (word & 0x1FFF) != 0x10FB:
        raise RefPackError(f"not RefPack: {data[:4].hex(' ')!r}")
    # header: 3- or 4-byte BE length at +2, chosen by bit15 of the word
    nlen = 4 if (word & 0x8000) else 3
    if len(data) < 2 + nlen:
        raise RefPackError("truncated header")
    size = int.from_bytes(data[2 : 2 + nlen], "big")
    if expect is not None and size != expect:
        raise RefPackError(f"header size {size} != expected {expect}")
    out = bytearray()
    i = 2 + nlen
    n = len(data)
    while True:
        if i >= n:
            raise RefPackError(f"ran out of input at {i} (produced {len(out)}/{size})")
        c = data[i]
        i += 1
        if c < 0x80:
            if i >= n:
                raise RefPackError("truncated 2-byte form")
            b1 = data[i]
            i += 1
            lits = c & 3
            ln = ((c >> 2) & 7) + 3  # +3, not +2: the asm copy loop tests before decrementing
            dist = ((c >> 5) & 3) * 256 + b1 + 1
        elif c < 0xC0:
            if i + 1 >= n:
                raise RefPackError("truncated 3-byte form")
            b0, b1 = data[i], data[i + 1]
            i += 2
            lits = b0 >> 6
            ln = (c & 0x3F) + 4  # +4: copy loop tests before decrementing (0x92c6b8 loop)
            dist = ((b0 & 0x3F) << 8) + b1 + 1
        elif c < 0xE0:
            if i + 2 >= n:
                raise RefPackError("truncated 4-byte form")
            b0, b1, b2 = data[i], data[i + 1], data[i + 2]
            i += 3
            lits = c & 3
            ln = ((c & 0x0C) << 6) + b2 + 5  # (c&0xC)<<6 per 0x92c71f, +5 per the loop
            dist = ((c & 0x10) << 12) + (b0 << 8) + b1 + 1
        else:
            ln = (c & 0x1F) * 4 + 4
            if ln > 0x70:  # 0xFC..0xFF: tail - copy (c&3) literals and stop
                lits = c & 3
                if i + lits > n:
                    raise RefPackError("truncated tail")
                out += data[i : i + lits]
                i += lits
                break
            if i + ln > n:
                raise RefPackError("truncated literal block")
            out += data[i : i + ln]
            i += ln
            continue
        if lits:
            if i + lits > n:
                raise RefPackError("truncated literals")
            out += data[i : i + lits]
            i += lits
        if dist == 0 or dist > len(out):
            raise RefPackError(f"bad distance {dist} at out_len {len(out)}")
        src = len(out) - dist
        for k in range(ln):  # byte-by-byte: overlap is allowed (RLE semantics)
            out.append(out[src + k])
    if expect is not None and len(out) != expect:
        raise RefPackError(f"produced {len(out)} bytes, expected {expect}")
    return bytes(out)


def is_refpack(data: bytes) -> bool:
    """The detector at 0x0092cd40: byte-swapped word, (& 0x1FFF) == 0x10FB.

    Top bits are flags, so 0x10FB AND 0x50FB both pass - which is exactly why the original
    masks instead of comparing the whole word, and the shipped packages use both.
    """
    if len(data) < 2:
        return False
    word = (data[0] << 8) | data[1]
    return (word & 0x1FFF) == 0x10FB


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("file", nargs="?")
    ap.add_argument("--out")
    ap.add_argument("--expect", type=int)
    ap.add_argument("--verify", action="store_true", help="decode real records from the packages")
    ap.add_argument("--limit", type=int, default=400)
    args = ap.parse_args()

    if args.verify:
        return verify(args.limit)
    if not args.file:
        ap.print_help()
        return 2
    data = Path(args.file).read_bytes()
    out = decode(data, args.expect)
    print(f"input  : {args.file} {len(data):,} B")
    print(f"decoded: {len(out):,} B  ratio {len(data)/len(out):.4f}")
    if args.out:
        Path(args.out).write_bytes(out)
        print(f"wrote  : {args.out}")
    print(f"first 32: {out[:32].hex(' ')}")
    return 0


def verify(limit: int) -> int:
    """Decode `limit` real RefPack records and require out_len == memSize for every one."""
    sys.path.insert(0, str(ROOT))
    from tools.pkg_kinds import DIRS, HDR, OFF_ALT, OFF_COUNT, OFF_IOFF, OFF_ISIZE, KINDS

    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    checked = fails = 0
    first_fail: list[str] = []
    for f in files:
        if checked >= limit:
            break
        h = f.read_bytes()[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        raw = f.read_bytes()
        kind = struct.unpack("<I", raw[use : use + 4])[0]
        if kind not in KINDS:
            continue
        prefix, stride, fld = KINDS[kind]
        if isize != prefix + stride * count:
            continue
        blob = raw[use + prefix : use + prefix + stride * count]
        for i in range(count):
            w = struct.unpack("<%dI" % (stride // 4), blob[i * stride : (i + 1) * stride])
            off, szraw, mem = w[fld["offset"]], w[fld["size"]], w[fld["usize"]]
            packed = szraw & 0x7FFFFFFF
            # The last word is compression:u16 (low half) + flags:u16 (high half), per pkg.h.
            comp = w[fld["sentinel"]] & 0xFFFF
            if comp != 0xFFFF:
                continue  # only RefPack records
            payload = raw[off : off + packed]
            try:
                got = decode(payload, mem)
                checked += 1
            except RefPackError as e:
                fails += 1
                if len(first_fail) < 5:
                    first_fail.append(f"{f.name}[{i}] packed={packed} mem={mem}: {e}")
                continue
            if checked >= limit:
                break
    print(f"# RefPack verification against the shipped packages")
    print(f"# records decoded successfully : {checked:,}")
    print(f"# records that FAILED to decode: {fails:,}")
    for x in first_fail:
        print(f"   FAIL {x}")
    print(f"\nVERDICT: {'PASS - out_len == memSize for every record' if fails == 0 and checked else 'FAIL'}")
    return 0 if (fails == 0 and checked) else 1


if __name__ == "__main__":
    sys.exit(main())
