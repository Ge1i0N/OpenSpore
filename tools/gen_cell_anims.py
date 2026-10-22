#!/usr/bin/env python3
"""Generate cell_anims.inc from the xls animation table in CellGame.package.

The xls resource (type 0x1F639D98) is an OLE2/Excel workbook with one sheet
mapping ANIM_EVENT names to clip names, movement styles and speed flags.
This tool extracts that table and emits a C++ include file with the data.
"""
import argparse
import struct
import sys
from pathlib import Path

XLS_TYPE = 0x1F639D98

# The repo root, so `from tools.refpack import decode` resolves when the tool is run as
# `python tools/gen_cell_anims.py` (sys.path[0] is tools/ then, and the import fails silently
# inside the reader, which makes the compressed xls look absent).
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))


def find_packages():
    roots = []
    import os
    if "SPORE_DATA" in os.environ:
        roots.append(Path(os.environ["SPORE_DATA"]))
    roots.append(Path("build/bin/Debug/Data"))
    roots.append(Path("build/bin/Debug/DataEP1"))
    for r in roots:
        if r.exists():
            yield r


def iter_resources(pkg_path):
    """Minimal DBPF reader to find the xls resource."""
    data = pkg_path.read_bytes()
    if len(data) < 96:
        return
    # DBPF header. Spore's DBPF (major 2/3) keeps the count at 0x24, the index SIZE at 0x2C and
    # the index OFFSET at 0x40 (0x28 is the loader's fallback). Reading the index at 0x0C/0x10 finds
    # nothing in this layout.
    magic = data[:4]
    if magic not in (b"DBPF", b"DBP2"):
        return
    index_size = struct.unpack_from("<I", data, 0x2C)[0]
    index_offset = struct.unpack_from("<I", data, 0x40)[0]
    if not index_offset:
        index_offset = struct.unpack_from("<I", data, 0x28)[0]
    if not index_offset or index_offset + index_size > len(data):
        return
    idx = data[index_offset:index_offset + index_size]
    # The index layout is chosen by the KIND dword: prefix = 4 + 4*popcount(kind),
    # stride = 32 - 4*popcount(kind), and the per-record fields are
    # type, group, instance, offset, size, usize, sentinel (in that order) minus the
    # fields the kind shares in the prefix.
    kind = struct.unpack_from("<I", idx, 0)[0]
    shared = bin(kind).count("1")
    prefix = 4 + 4 * shared
    stride = 32 - 4 * shared
    if stride < 20 or prefix + stride > len(idx):
        return
    shared_vals = {}
    for bit, name in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared_vals[name] = struct.unpack_from("<I", idx, 4 * (1 + len(shared_vals)))[0]
    fields = []
    for bit, name in ((1, "type"), (2, "group"), (4, "unknown"), (8, "instance"),
                      (16, "offset"), (32, "size"), (64, "usize"), (128, "sentinel")):
        if not (kind & bit):
            fields.append(name)
    n = (len(idx) - prefix) // stride
    for i in range(n):
        base = prefix + i * stride
        rec = {}
        for wi, name in enumerate(fields):
            rec[name] = struct.unpack_from("<I", idx, base + wi * 4)[0]
        rec.update(shared_vals)
        if rec.get("type") != XLS_TYPE:
            continue
        blob = data[rec["offset"]:rec["offset"] + (rec["size"] & 0x7FFFFFFF)]
        if (rec["sentinel"] & 0xFFFF) == 0xFFFF:
            try:
                from tools.refpack import decode as refpack_decode
                blob = refpack_decode(blob, rec["usize"])
            except Exception:
                continue
        yield rec["type"], rec.get("group", 0), rec.get("instance", 0), blob


def parse_xls_sheet(data):
    """Parse OLE2 compound document to extract the first sheet's cell values."""
    try:
        import xlrd
        import io
        book = xlrd.open_workbook(file_contents=data)
        sheet = book.sheet_by_index(0)
        rows = []
        for r in range(sheet.nrows):
            row = []
            for c in range(sheet.ncols):
                v = sheet.cell_value(r, c)
                row.append(v)
            rows.append(row)
        return rows
    except ImportError:
        return None


def parse_anim_rows(rows):
    """Parse animation rows into structured data.

    A blank EVENT cell is part of the shipped table (12 of the 73 rows), not a row to drop: the
    clip name is the only key for those. Only a row with no clip at all is skipped.
    """
    anims = []
    for row in rows:
        if len(row) < 2:
            continue
        event = str(row[0]).strip() if row[0] else ""
        clip = str(row[1]).strip() if row[1] else ""
        if not clip:
            continue
        style = str(row[2]).strip() if len(row) > 2 and row[2] else ""
        speed = 0.0
        if len(row) > 3 and row[3]:
            try:
                speed = float(row[3])
            except (ValueError, TypeError):
                speed = 0.0
        anims.append({
            "event": event,
            "clip": clip,
            "style": style,
            "speed": speed,
        })
    return anims


def generate_inc(anims, output_path):
    """Generate C++ include file with animation data."""
    lines = [
        "// Auto-generated from CellGame.package xls resource (type 0x1F639D98).",
        "// Maps ANIM_EVENT names to animation clip names, movement styles and speed flags.",
        "// Do not edit manually - regenerate from the game data.",
        "#pragma once",
        "#include <cstring>",
        "namespace os::cell {",
        "",
        "struct AnimationDef {",
        "    const char* event;",
        "    const char* clip;",
        "    const char* style;",
        "    float speed;",
        "};",
        "",
        f"inline constexpr AnimationDef kCellAnims[] = {{",
    ]
    for a in anims:
        event = a["event"].replace('"', '\\"')
        clip = a["clip"].replace('"', '\\"')
        style = a["style"].replace('"', '\\"')
        lines.append(f'    {{"{event}", "{clip}", "{style}", {a["speed"]}f}},')
    lines.append("};")
    lines.append("")
    lines.append("inline constexpr size_t kCellAnimCount = sizeof(kCellAnims) / sizeof(kCellAnims[0]);")
    lines.append("")
    lines.append("inline const AnimationDef* find_anim(const char* event) {")
    lines.append("    if (!event) return nullptr;")
    lines.append("    for (const auto& a : kCellAnims) {")
    lines.append('        if (a.event[0] && strcmp(a.event, event) == 0) return &a;')
    lines.append("    }")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")
    lines.append("}  // namespace os::cell")
    lines.append("")
    output_path.write_text("\n".join(lines), encoding="utf-8")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--output", type=Path, default=Path("source/engine/cell_anims.inc"))
    args = ap.parse_args()

    for pkg_root in find_packages():
        for pkg in sorted(pkg_root.glob("*.package")):
            for type_id, group_id, instance_id, data in iter_resources(pkg):
                if data[:8] == b"\xd0\xcf\x11\xe0\xa1\xb1\x1a\xe1":
                    rows = parse_xls_sheet(data)
                    if rows:
                        anims = parse_anim_rows(rows)
                        if anims:
                            generate_inc(anims, args.output)
                            print(f"Generated {args.output} with {len(anims)} animations from {pkg.name}")
                            return 0
    print("No xls animation table found", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
