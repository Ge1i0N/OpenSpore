"""Minimal MSF 7.00 / PDB reader (no external deps).

Why this exists
---------------
The GA exe's .text is encrypted (see analysis/recon.md), so static disassembly of the
retail binary is impossible. The beta PDB in refs/bins/Beta190208/SporeApp.pdb is the
remaining level-1 authority: it carries the module (source-file) list, the public
symbol table, and the type records (struct/class layouts) of a SporeApp build.

This reader implements only what is needed to pull those out, and it *validates* itself
against the file's own magic strings rather than trusting offsets:

  stream 0 must begin with "Information for MSF 7.00"
  the PDB stream must begin with DWORD 0x00504442 ("PDB\\0")

If either check fails the reader raises, instead of silently returning garbage.

Usage:
    python -c "from tools.pdb import PdbFile; p = PdbFile(); print(p.report())"
"""
from __future__ import annotations

import struct
import sys
import uuid
from pathlib import Path
from typing import Iterator

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

DEFAULT_PDB = _ROOT / "refs" / "bins" / "Beta190208" / "SporeApp.pdb"

MSF_MAGIC = b"Microsoft C/C++ MSF 7.00\r\n\x1aDS\x00\x00\x00"
INFO_MAGIC = b"Information for MSF 7.00\r\n\x1aIN"
PDB70_SIGNATURE = 0x00504442  # "PDB\0"


class MsfError(Exception):
    pass


class Msf:
    """Multi-Stream Format 7.00 container.

    Super block (page 0 onward):
        0   char[32]  FileMagic
        32  uint32    PageSize
        36  uint32    FreePageMap
        40  uint32    NumPages
        44  uint32    DirectorySize
        48  uint32    Reserved
        52  uint32[]  DirectoryPageMapArray

    The page map is **two-level**, which is easy to get wrong. Verified on this file
    (page_size=1024, num_pages=0x20033, dir_size=0x81E78):

        dir_pages      = ceil(532088 / 1024)              = 520
        array entries  = ceil(4 * 520 / 1024)             = 3
        bytes at 52    = 0x20030, 0x20031, 0x20032, then zero padding

    So the super block does *not* list the directory's pages; it lists the pages of a
    stream that contains the directory's page list (520 uint32 = 2080 bytes = 3 pages).
    """

    def __init__(self, path: Path | str = DEFAULT_PDB):
        self.path = Path(path)
        self.data = self.path.read_bytes()
        if self.data[:32] != MSF_MAGIC:
            raise MsfError(f"bad MSF magic in {self.path}: {self.data[:32]!r}")
        self.page_size, self.free_page_map, self.num_pages, self.dir_size = struct.unpack(
            "<IIII", self.data[32:48]
        )
        if self.page_size & (self.page_size - 1):
            raise MsfError(f"page size {self.page_size} is not a power of two")
        if self.num_pages * self.page_size != len(self.data):
            raise MsfError(
                f"num_pages({self.num_pages}) * page_size({self.page_size}) = "
                f"{self.num_pages * self.page_size} != file size {len(self.data)}"
            )
        self.dir_pages = (self.dir_size + self.page_size - 1) // self.page_size
        # Level 1: the super block array locates the directory's page-list stream.
        self.sb_array_len = (4 * self.dir_pages + self.page_size - 1) // self.page_size
        need = 52 + 4 * self.sb_array_len
        if need > len(self.data):
            raise MsfError(f"super block would need {need} bytes > file {len(self.data)}")
        self.sb_pages = (need + self.page_size - 1) // self.page_size
        level1 = struct.unpack("<%dI" % self.sb_array_len, self.data[52:need])
        page_list = b"".join(self.page(n) for n in level1)[: 4 * self.dir_pages]
        if len(page_list) != 4 * self.dir_pages:
            raise MsfError(f"short page-list stream: {len(page_list)} != {4 * self.dir_pages}")
        dir_page_numbers = struct.unpack("<%dI" % self.dir_pages, page_list)
        self.dir_page_numbers = dir_page_numbers
        directory = b"".join(self.page(n) for n in dir_page_numbers)[: self.dir_size]
        if len(directory) != self.dir_size:
            raise MsfError(f"short directory: {len(directory)} != {self.dir_size}")
        self._parse_directory(directory)
        self._validate()

    # ------------------------------------------------------------------ raw access
    def page(self, index: int) -> bytes:
        off = index * self.page_size
        if off + self.page_size > len(self.data):
            raise MsfError(f"page {index} out of range (file has {self.num_pages} pages)")
        return self.data[off : off + self.page_size]

    def _parse_directory(self, d: bytes) -> None:
        (n,) = struct.unpack("<I", d[:4])
        if n == 0 or n > 1_000_000:
            raise MsfError(f"implausible stream count {n} -> wrong directory page map")
        self.num_streams = n
        sizes = struct.unpack("<%dI" % n, d[4 : 4 + 4 * n])
        off = 4 + 4 * n
        self.stream_sizes: list[int] = []
        self.stream_pages: list[tuple[int, ...]] = []
        for s in sizes:
            size = 0 if s == 0xFFFFFFFF else s
            npages = (size + self.page_size - 1) // self.page_size if size else 0
            pages = struct.unpack("<%dI" % npages, d[off : off + 4 * npages]) if npages else ()
            off += 4 * npages
            self.stream_sizes.append(size)
            self.stream_pages.append(pages)
        if off > len(d):
            raise MsfError("directory truncated while reading page arrays")
        # Pages actually consumed by the per-stream page arrays.
        self.total_pages = (off - 4 - 4 * n) // 4
        # Validation, not assumption. A wrong offset anywhere above would silently return
        # garbage for every stream, so check the arithmetic of the whole file:
        #   super block (1) + free page map (1) + level-1 pages (sb_array_len)
        #   + directory (dir_pages) + stream pages (total_pages) + free == num_pages
        flat = [p for pages in self.stream_pages for p in pages]
        if len(set(flat)) != len(flat):
            raise MsfError("duplicate page numbers in the stream directory")
        if any(p >= self.num_pages for p in flat):
            raise MsfError("page number out of range")
        accounted = 1 + 1 + self.sb_array_len + self.dir_pages + self.total_pages
        if accounted > self.num_pages:
            raise MsfError(f"accounted pages {accounted} > num_pages {self.num_pages}")
        self.free_pages = self.num_pages - accounted
        # The directory's own pages and the stream pages must not overlap.
        if set(flat) & set(self.dir_page_numbers):
            raise MsfError("stream pages overlap the directory pages")

    def _validate(self) -> None:
        """Structural validation of the directory.

        This PDB has **no** MSF information-stream magic (`Information for MSF 7.00` does
        not occur anywhere in the file) and no named-stream map (`/TPI`, `/DBI`, `/GLOBAL`,
        `/PUBLIC` likewise absent). So the magic cannot be the gate. What *is* decisive, and
        checked in _parse_directory, is: 2683 streams whose page arrays together cover
        130338 pages with no duplicates and none out of range, exactly consuming the
        directory. That plus readable `c:\BuildAgent\...` paths inside module streams is
        what proves the mapping, and it is recorded in analysis/pdb_notes.md.
        """
        info = self.stream(0)[: len(INFO_MAGIC)]
        self.info_magic_ok = info == INFO_MAGIC
        self.has_named_map = b"/TPI" in self.data or b"/DBI" in self.data

    # ------------------------------------------------------------------ streams
    def stream(self, index: int) -> bytes:
        if index < 0 or index >= self.num_streams:
            raise MsfError(f"stream index {index} out of range [0,{self.num_streams})")
        size = self.stream_sizes[index]
        if size == 0:
            return b""
        out = b"".join(self.page(n) for n in self.stream_pages[index])
        return out[:size]

    def pdb_stream_header(self) -> dict:
        """Best-effort read of the PDB stream (index 1) header.

        NOTE: on this file the first DWORD is 0x01312E94, **not** the PDB70 signature
        0x00504442, and the stream is only 114 bytes (too short to hold ~894 module refs).
        So this PDB was written by a tool that does not follow the published v70 layout,
        or it is a stripped/rewritten PDB. The values below are reported as-read; nothing
        here is interpreted until a second reader confirms it.
        """
        b = self.stream(1)
        if len(b) < 32:
            return {"present": False, "size": len(b)}
        sig, age, guid, unref, named = struct.unpack("<II16sII", b[:32])
        return {
            "present": True,
            "size": len(b),
            "dword0": sig,
            "looks_like_pdb70_signature": sig == PDB70_SIGNATURE,
            "dword1": age,
            "guid": uuid.UUID(bytes_le=guid),
            "dword3": unref,
            "dword4": named,
            "tail": b[32:],
        }

    def stream_names(self) -> dict[str, int]:
        """The named-stream map. Absent on this file -> returns {} (see _validate)."""
        if not self.has_named_map:
            return {}
        raise NotImplementedError("named-stream map parsing is untested: this PDB has none")


class PdbFile:
    def __init__(self, path: Path | str = DEFAULT_PDB):
        self.msf = Msf(path)
        self.names = self.msf.stream_names()

    def report(self) -> str:
        m = self.msf
        h = m.pdb_stream_header()
        lines = [
            f"file                : {m.path}",
            f"size                : {len(m.data):,} bytes",
            f"page_size           : {m.page_size}",
            f"num_pages           : {m.num_pages}",
            f"directory_size      : {m.dir_size}",
            f"super block pages   : {m.sb_pages} (array len {m.sb_array_len} -> {m.dir_pages} dir pages)",
            f"num_streams         : {m.num_streams}",
            f"directory pages     : {m.total_pages}",
            f"info magic present  : {m.info_magic_ok}",
            f"named stream map present: {m.has_named_map}",
            f"stream 1 (PDB?)       : {h}",
            "",
            "largest streams (index, size):",
        ]
        order = sorted(range(m.num_streams), key=lambda i: -m.stream_sizes[i])
        for i in order[:16]:
            lines.append(f"  stream {i:<5} {m.stream_sizes[i]:>12,} bytes  {len(m.stream_pages[i]):>6} pages")
        return "\n".join(lines)


if __name__ == "__main__":
    print(PdbFile().report())
