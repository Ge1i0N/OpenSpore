"""String scanners for the *plaintext* sections of SporeApp.exe (GA).

Why hand-rolled instead of `bytes.decode('utf-16-le', errors='ignore')`:
dropping unpaired surrogates silently shifts every subsequent 16-bit unit, so the
reported addresses drift. On the first run of `dump_strings.py` that produced a
phantom wide-string "devDirs" sitting inside a float constant pool at 0x013f8592.
Verified by a hex dump of 0x013f8560..0x013f8680: that region is `.eBT` behaviour-tree
records + IEEE-754 constants (0x40490fdb = 3.14159f, 0x3a83126f, ...), not text.

So: walk 16-bit units explicitly, one unit at a time, never decode-and-hope.
"""
from __future__ import annotations

import re
from typing import Iterator, Tuple

_ASCII_RUN = re.compile(rb"[\x20-\x7e]{4,}")


def ascii_strings(data: bytes, base_va: int, min_len: int = 4) -> Iterator[Tuple[int, str]]:
    for m in _ASCII_RUN.finditer(data):
        yield base_va + m.start(), m.group().decode("latin-1")


def utf16_strings(data: bytes, base_va: int, min_len: int = 4) -> Iterator[Tuple[int, str]]:
    """Yield (va, text) for runs of printable BMP chars encoded UTF-16LE.

    Scans both byte parities (a wide string may begin at an odd offset relative to
    the section start). Units outside 0x20..0x7E terminate a run; nothing is skipped
    or substituted, so addresses stay exact.
    """
    end = len(data) - 1
    for align in (0, 1):
        i = align
        while i < end:
            u = data[i] | (data[i + 1] << 8)
            if not 0x20 <= u <= 0x7E:
                i += 2
                continue
            start = i
            chars = []
            while i < end:
                u = data[i] | (data[i + 1] << 8)
                if not 0x20 <= u <= 0x7E:
                    break
                chars.append(chr(u))
                i += 2
            if len(chars) >= min_len:
                yield base_va + start, "".join(chars)


def all_strings(data: bytes, base_va: int, min_len: int = 4) -> list[Tuple[int, str, str]]:
    """Return sorted, de-duplicated (va, kind, text); kind is 'a' (ASCII) or 'u' (UTF-16LE)."""
    out: list[Tuple[int, str, str]] = [(va, "a", t) for va, t in ascii_strings(data, base_va, min_len)]
    out += [(va, "u", t) for va, t in utf16_strings(data, base_va, min_len)]
    return sorted(set(out))
