"""Extract the build topology (module / object / source paths) out of the beta PDB.

Why: the retail GA .text is encrypted (see analysis/recon.md), so the beta PDB is the only
level-1 source of *names*. Its module streams contain S_OBJNAME records whose payloads are
plain ANSI paths, which is enough to recover the game's project/subsystem topology without
decoding a single CodeView type record.

The MSF 7.00 reader here is the validated one from tools/pdb.py: the stream directory
resolves to 2683 streams / 130338 pages, all distinct and fully consumed (verified), and
stream 9 is the section-header array of the beta image.

Usage: python tools/pdb_topology.py
Writes: analysis/pdb_topology.md
"""
from __future__ import annotations

import re
import sys
from collections import Counter
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

from tools.pdb import Msf

OUT = _ROOT / "analysis" / "pdb_topology.md"

_PATH = re.compile(rb"[A-Za-z]:\\[\x20-\x7e]{4,}")
# CodeView record header as it actually appears in this file: {u16 length; u16 type}
# (verified by walking stream 14: 0x62/0x1101 S_OBJNAME, then 0x3a/0x1116 S_COMPILE3,
#  each landing exactly on the next header).
S_OBJNAME = 0x1101


def paths_from(data: bytes) -> list[str]:
    out = []
    for m in _PATH.finditer(data):
        s = m.group().decode("latin-1")
        # a path ends at the first NUL; the regex already stops at non-printable
        out.append(s)
    return out


def main() -> int:
    msf = Msf()
    lines = ["# Build topology from refs/bins/Beta190208/SporeApp.pdb", ""]
    lines.append(f"- streams: {msf.num_streams}, directory pages: {msf.total_pages}")
    lines.append(f"- page_size: {msf.page_size}, num_pages: {msf.num_pages}, dir_size: {msf.dir_size}")
    lines.append(f"- info magic present: {msf.info_magic_ok}; named-stream map present: {msf.has_named_map}")
    lines.append("")

    # 1. section header array (stream 9 on this file; find it by shape, not by index)
    sha = None
    for i in range(msf.num_streams):
        b = msf.stream(i)
        if len(b) >= 40 and len(b) % 40 == 0 and b[:8].rstrip(b"\x00") == b".text":
            sha = b
            lines.append(f"## Section header array (stream {i}, {len(b)} bytes = {len(b)//40} sections)")
            lines.append("")
            lines.append("| name | VirtualSize | VirtualAddress | SizeOfRawData | PointerToRawData | Characteristics |")
            lines.append("|---|---|---|---|---|---|")
            import struct

            for k in range(len(b) // 40):
                h = b[k * 40 : (k + 1) * 40]
                name = h[:8].rstrip(b"\x00").decode("latin-1")
                vsz, va, rsz, rptr, _ts, _nr, _nl, ch = struct.unpack("<IIIIHHII", h[8:36])
                lines.append(f"| `{name}` | 0x{vsz:08x} | 0x{va:08x} | 0x{rsz:08x} | 0x{rptr:08x} | 0x{ch:08x} |")
            lines.append("")
            break
    if sha is None:
        lines.append("## Section header array: not found (no stream starts with `.text` + 40-byte records)")
        lines.append("")

    # 2. every path in the file, bucketed by the directory component
    all_paths: list[str] = []
    for i in range(msf.num_streams):
        b = msf.stream(i)
        if not b:
            continue
        for p in paths_from(b):
            all_paths.append(p)
    uniq = sorted(set(all_paths))
    lines.append(f"## Paths in the PDB: {len(all_paths)} occurrences, {len(uniq)} distinct")
    lines.append("")
    dirs = Counter(str(Path(p).parent).lower() for p in uniq)
    bases = Counter(Path(p).name.lower() for p in uniq)
    lines.append("### Directories (top 60)")
    lines.append("")
    for k, v in dirs.most_common(60):
        lines.append(f"- {v:>4}  `{k}`")
    lines.append("")
    lines.append("### Object/base names (top 120)")
    lines.append("")
    for k, v in bases.most_common(120):
        lines.append(f"- {v:>4}  `{k}`")
    lines.append("")
    # 3. project names: the directory just under obj\Win32\<config>\
    proj = Counter()
    for p in uniq:
        parts = Path(p).parts
        for j, part in enumerate(parts):
            if part.lower() in ("win32", "x64") and j + 2 < len(parts):
                proj[parts[j + 2]] += 1
                break
    lines.append("### Project names (component directories under obj\\Win32\\<config>)")
    lines.append("")
    lines.append(f"distinct: {len(proj)}")
    lines.append("")
    for k, v in sorted(proj.items()):
        lines.append(f"- `{k}` ({v})")
    lines.append("")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT.relative_to(_ROOT)} ({len(lines)} lines), {len(uniq)} distinct paths")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
