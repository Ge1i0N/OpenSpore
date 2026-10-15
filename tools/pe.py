"""PE recon helpers for the Spore GA binary (refs/bins/ReleaseGA/SporeApp.exe).

Authority: this module reads the *original shipped executable* only. Nothing here
guesses; every helper returns bytes/instructions from the binary.

Usage (from repo root):
    python -c "from tools.pe import PE; pe = PE(); print(pe.cstr(0x01234567))"

Design notes (field guide §2 traps):
  * pefile stores RVA in section headers -> always subtract ImageBase first.
  * Section names are truncated/padded -> rstrip b'\x00'.
  * Misc_VirtualSize can be smaller than SizeOfRawData -> use max() for the range test.
"""
from __future__ import annotations

import re
import struct
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parent.parent
# The GA build ships with an **encrypted .text**, so the retail exe cannot be disassembled
# (entropy 8.000; every metric in analysis/unpacked_verification.md fails on it). The
# post-decryption dump is byte-identical in every section except .text and the removed
# EncryptPE `.bind`, so it is the level-1 oracle for static reversal. See MEMORY.md.
RELEASE_ENCRYPTED = ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe"
DEFAULT_EXE = ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe.unpacked.exe"

# Every reference binary we know about, keyed for the "which build is this?" question.
BINS = {
    "ga_encrypted": RELEASE_ENCRYPTED,
    "ga": DEFAULT_EXE,
    "beta_pdb": ROOT / "refs" / "bins" / "Beta190208" / "SporeApp.pdb",
    "base_also1": ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also1" / "SporeApp.exe",
    "ga_also1": ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also1" / "SporeAppEP1.exe",
    "ga_also2_wtf": ROOT / "refs" / "bins" / "FindTheUnencryptedDotText" / "Also2" / "SporeAppEP1.wtf",
    "proto_gonzago": ROOT / "refs" / "bins" / "Proto" / "GonzagoGL.exe",
}

_ASCII = re.compile(rb"[\x20-\x7e]{4,}")


def _ts(value: int) -> str:
    import datetime

    try:
        return datetime.datetime.fromtimestamp(value, datetime.timezone.utc).strftime("%Y-%m-%d %H:%M:%S UTC")
    except (OverflowError, OSError, ValueError):
        return f"<invalid {value:#x}>"


class PE:
    def __init__(self, path: Path | str = DEFAULT_EXE):
        self.path = Path(path)
        self.pe = pefile.PE(str(self.path), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = {}
        for s in self.pe.sections:
            name = s.Name.rstrip(b"\x00").decode("latin-1")
            self.sections[name] = {
                "name": name,
                "va": self.base + s.VirtualAddress,
                "rva": s.VirtualAddress,
                "vsize": s.Misc_VirtualSize,
                "raw": s.SizeOfRawData,
                "off": s.PointerToRawData,
                "chars": s.Characteristics,
                "end_va": self.base + s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData),
            }
        self._sec_list = sorted(self.sections.values(), key=lambda s: s["rva"])

    # ------------------------------------------------------------------ addressing
    def section_of(self, va: int):
        rva = va - self.base
        for s in self._sec_list:
            if s["rva"] <= rva < s["rva"] + max(s["vsize"], s["raw"]):
                return s
        return None

    def file_off(self, va: int) -> int | None:
        s = self.section_of(va)
        # Virtual zero-fill tails have no bytes in the executable file.
        if s is None or va - s["va"] >= s["raw"]:
            return None
        return s["off"] + (va - self.base - s["rva"])

    def read(self, va: int, size: int) -> bytes:
        """Read `size` bytes at virtual address `va` (file-backed content)."""
        off = self.file_off(va)
        section = self.section_of(va)
        if size < 0:
            raise ValueError("negative read size")
        if off is None or section is None or size > section["raw"] - (va - section["va"]):
            raise ValueError(f"VA 0x{va:08x} is not backed by file data")
        data = self.raw[off : off + size]
        if len(data) != size:
            raise ValueError(f"short read at 0x{va:08x}: {len(data)}/{size}")
        return data

    def u32(self, va: int) -> int:
        return struct.unpack("<I", self.read(va, 4))[0]

    def f32(self, va: int) -> float:
        return struct.unpack("<f", self.read(va, 4))[0]

    def cstr(self, va: int, limit: int = 512) -> bytes:
        raw = self.read(va, limit)
        return raw.split(b"\x00", 1)[0]

    def wstr(self, va: int, limit: int = 512) -> str:
        """Read a UTF-16LE string.

        The terminator is a NUL *code unit* at an even offset relative to `va`.
        Splitting on the byte pair b'\\x00\\x00' is wrong: for an odd-length prefix it can
        match across a real character (e.g. "devDirs" ends ...72 00 73 00 00 00, and the
        first 00 00 pair sits at the low byte of 'r'), which truncated the last character
        and produced a replacement glyph in the report.
        """
        raw = self.read(va, limit)
        units = []
        for i in range(0, len(raw) - 1, 2):
            u = raw[i] | (raw[i + 1] << 8)
            if u == 0:
                break
            units.append(u)
        return "".join(chr(u) if 32 <= u <= 0x7E else f"\\u{u:04x}" for u in units)

    # ------------------------------------------------------------------ bulk
    @property
    def raw(self) -> bytes:
        if not hasattr(self, "_raw"):
            self._raw = self.path.read_bytes()
        return self._raw

    def section_bytes(self, name: str) -> bytes:
        s = self.sections[name]
        return self.raw[s["off"] : s["off"] + max(s["vsize"], s["raw"])]

    def strings(self, section: str = ".text", min_len: int = 4):
        """Yield (va, text) for ASCII runs in a section."""
        s = self.sections[section]
        data = self.section_bytes(section)
        for m in _ASCII.finditer(data):
            if len(m.group()) < min_len:
                continue
            yield s["va"] + m.start(), m.group().decode("latin-1")

    def find_bytes(self, needle: bytes, section: str | None = None):
        """Yield virtual addresses where `needle` occurs."""
        if section:
            s = self.sections[section]
            data = self.section_bytes(section)
            start = 0
            while True:
                i = data.find(needle, start)
                if i < 0:
                    return
                yield s["va"] + i
                start = i + 1
            return
        data = self.raw
        start = 0
        while True:
            i = data.find(needle, start)
            if i < 0:
                return
            yield i
            start = i + 1

    def refs_to(self, va: int):
        """Virtual addresses in .text whose 4-byte little-endian content is `va`.

        This is the cheap 'who references this global?' query from the field guide.
        """
        packed = struct.pack("<I", va)
        for addr in self.find_bytes(packed, ".text"):
            yield addr

    def summary(self) -> str:
        oh = self.pe.OPTIONAL_HEADER
        lines = [
            f"file            : {self.path}",
            f"machine         : {hex(self.pe.FILE_HEADER.Machine)} (0x14c = i386/x86)",
            f"image_base      : 0x{self.base:08x}",
            f"entry_point     : 0x{self.base + oh.AddressOfEntryPoint:08x}",
            f"size_of_image   : 0x{oh.SizeOfImage:x}",
            f"subsystem       : {oh.Subsystem} (2 = Windows GUI)",
            f"linker          : {oh.MajorLinkerVersion}.{oh.MinorLinkerVersion}",
            f"timestamp       : {self.pe.FILE_HEADER.TimeDateStamp} ({_ts(self.pe.FILE_HEADER.TimeDateStamp)})",
            f"dll_characteristics: {hex(oh.DllCharacteristics)}",
            "sections:",
        ]
        for s in self._sec_list:
            lines.append(
                f"  {s['name']:<8} va=0x{s['va']:08x} vsize=0x{s['vsize']:08x} "
                f"off=0x{s['off']:08x} raw=0x{s['raw']:08x}"
            )
        return "\n".join(lines)


if __name__ == "__main__":
    pe = PE()
    print(pe.summary())
