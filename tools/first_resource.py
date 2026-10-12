"""First real EA resource, read end-to-end through the reversed index.

res0.bin came from: OpenSpore.exe -exec pkg:-dump,0,3,res0.bin
  record[3] of Data\Locale\cs-cz\NameGeneration.package
  type=0x024a0e52 instance=0x65e81724 off=0x4c11 size=1371(flag) usize=2120 sentinel=0x1ffff
  -> dumped 1371 bytes, and fread returned exactly 1371.

So the index offset+size pair is confirmed by content, not just by arithmetic: the bytes
are Czech name-generation vocabulary (akademie, akvári…, partmán, bank, abiz…), which is
precisely what a cs-cz NameGeneration.package should contain.

Two conclusions and one open question, all recorded honestly:
1. The 0x80000000 bit is NOT "compressed": this payload is plaintext with no zlib header
   (first bytes 10 fb 00 08, not 78 9c / 78 da). It is set in 150,945/150,945 records, so
   it behaves like a validity/used flag.
2. usize (2120) > size (1371) here, and usize >= size in 150,945/150,945 records. Since the
   payload is uncompressed, "uncompressed size" is WRONG as a name for usize. Open question,
   to be answered by disassembly (the accessor at 0x008da6f0 and the vtable+0x38/+0x28 calls
   it makes), not by more hexdumps. Candidates: allocation size in memory, size including a
   header the package strips, or a per-type alignment/stride.
3. Strings inside are a Spore-internal encoding, not UTF-8: 0xe1 and 0x0d/0x0a/0x02/0x12/0x16
   appear as single-byte prefixes/separators before accented letters (c3 a1 = á does appear as
   real UTF-8 too, so the resource mixes both). Decoding it is a separate reversal job.

Usage: python tools/first_resource.py <res0.bin>
"""
from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    p = Path(sys.argv[1] if len(sys.argv) > 1 else "res0.bin")
    d = p.read_bytes()
    print(f"# {p}  {len(d):,} bytes")
    print(f"first 16: {d[:16].hex(' ')}")
    zlib_like = d[:2] in (b"\x78\x9c", b"\x78\xda", b"\x78\x01", b"\x78\x5e")
    print(f"zlib header present: {zlib_like}   -> 0x80000000 means: "
          f"{'compressed?' if zlib_like else 'NOT compression (plaintext payload)'}")
    # count printable-ASCII runs >= 3 as a crude vocabulary measure
    runs = []
    cur = bytearray()
    for b in d:
        if 32 <= b < 127:
            cur.append(b)
        else:
            if len(cur) >= 3:
                runs.append(cur.decode("latin-1"))
            cur = bytearray()
    if len(cur) >= 3:
        runs.append(cur.decode("latin-1"))
    print(f"printable runs >=3: {len(runs)}")
    print("sample:", ", ".join(repr(r) for r in runs[:24]))
    hi = sum(1 for b in d if b >= 0x80)
    print(f"high-bit bytes: {hi} ({100*hi/len(d):.1f}%) - non-ASCII content present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
