#!/usr/bin/env python3
"""Extract cell-save field descriptors from the GA binary.

The field registration is inline code, not a static table: each field is
written into a stack descriptor by `mov dword [esp+off], imm32` stores.
Two observed shapes (file offsets; VA = file + 0xC00 for .text):

  shape A (esp+0x14 block):
    C7 44 24 10 imm32(registry_fn)
    C7 44 24 14 imm32(field_id)
    89 4x 24 28/2C  ; 89 5x 24 30
    C7 44 24 34..48 imm32(cb...)   (4-6 callback VAs)
    C7 44 24 18 imm32(default)

  shape B (esp+0x0C block):
    C7 44 24 0C imm32(field_id)
    89 4x 24 20/24  ; 89 5x 24 28
    C7 44 24 2C..40 imm32(cb...)   (5-6 callback VAs)
    C7 44 24 10 imm32(default)

Field IDs are NOT unique across classes: the same wire ID can appear in
several registration blocks. Every hit is reported with its file offset.

Usage: python tools/cell_field_descriptors.py
Reads analysis/audit/cell-save-full.json for the ID list (the decoded real
save) and refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe for the code.
Writes analysis/audit/cell-field-descriptors.json.
"""
from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe.unpacked.exe"
SAVE_JSON = ROOT / "analysis" / "audit" / "cell-save-full.json"
OUT_JSON = ROOT / "analysis" / "audit" / "cell-field-descriptors.json"
TEXT_VA_DELTA = 0xC00  # .text: raw 0x400, VA 0x1000


def u32(b: bytes, at: int) -> int:
    return struct.unpack_from("<I", b, at)[0]


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def parse_shape_a(d: bytes, at: int) -> dict | None:
    """at = offset of the field-id dword (preceded by C7 44 24 14)."""
    q = at + 4
    cbs: list[int] = []
    default = None
    for _ in range(12):
        if q + 8 > len(d):
            return None
        op = d[q : q + 4]
        if op[:3] == b"\xc7\x44\x24":
            slot, imm = op[3], u32(d, q + 4)
            if 0x34 <= slot <= 0x48:
                cbs.append(imm)
            elif slot == 0x18:
                default = imm
            q += 8
        elif op[0] == 0x89 and op[2] == 0x24 and (op[1] & 0x07) == 0x04:
            q += 4  # mov [esp+disp8], r32
        else:
            break
    if not cbs:
        return None
    return {"shape": "A", "cbs": cbs, "default": default}


def parse_shape_b(d: bytes, at: int) -> dict | None:
    """at = offset of the field-id dword (preceded by C7 44 24 0C)."""
    q = at + 4
    cbs: list[int] = []
    default = None
    for _ in range(12):
        if q + 8 > len(d):
            return None
        op = d[q : q + 4]
        if op[:3] == b"\xc7\x44\x24":
            slot, imm = op[3], u32(d, q + 4)
            if 0x2C <= slot <= 0x40:
                cbs.append(imm)
            elif slot == 0x10:
                default = imm
                break
            q += 8
        elif op[0] == 0x89 and op[2] == 0x24 and (op[1] & 0x07) == 0x04:
            q += 4  # mov [esp+disp8], r32
        else:
            break
    if not cbs:
        return None
    return {"shape": "B", "cbs": cbs, "default": default}


def main() -> int:
    if not BIN.is_file():
        out(f"oracle binary missing: {BIN}")
        return 2
    if not SAVE_JSON.is_file():
        out(f"save decode missing: {SAVE_JSON}")
        return 2
    data = BIN.read_bytes()
    save = json.loads(SAVE_JSON.read_text(encoding="utf-8"))

    ids: set[int] = set()
    for stream in save.values():
        for obj in stream:
            for k in obj.get("fields", {}):
                ids.add(int(k, 16))
    ids.discard(0)

    table: list[dict] = []
    for fid in sorted(ids):
        pat = struct.pack("<I", fid)
        hits: list[int] = []
        start = 0
        while True:
            p = data.find(pat, start)
            if p < 0:
                break
            hits.append(p)
            start = p + 1
        for h in hits:
            entry: dict = {"fid": f"0x{fid:08X}", "file": f"0x{h:X}", "va": f"0x{h + TEXT_VA_DELTA:X}"}
            if h >= 4 and u32(data, h - 4) == 0x142444C7:
                # shape A: field id stored at [esp+0x14]
                parsed = parse_shape_a(data, h)
                if parsed:
                    entry.update(parsed)
                    if h >= 8:
                        entry["registry_fn"] = f"0x{u32(data, h - 8):08X}"
            elif h >= 4 and u32(data, h - 4) == 0x0C2444C7:
                # shape B: field id stored at [esp+0x0C]
                parsed = parse_shape_b(data, h)
                if parsed:
                    entry.update(parsed)
            else:
                entry["note"] = "raw id hit, no descriptor shape"
            if "cbs" in entry:
                entry["cbs"] = [f"0x{c:08X}" for c in entry["cbs"]]
                entry["default"] = f"0x{entry['default']:08X}" if entry.get("default") is not None else None
            table.append(entry)

    OUT_JSON.parent.mkdir(parents=True, exist_ok=True)
    OUT_JSON.write_text(
        json.dumps(
            {
                "binary": str(BIN.relative_to(ROOT)),
                "note": "field ids from the decoded real cell save; inline code descriptors",
                "entries": table,
            },
            indent=1,
        )
        + "\n",
        encoding="utf-8",
    )
    shaped = sum(1 for e in table if "cbs" in e)
    out(f"{len(ids)} field ids, {len(table)} hits, {shaped} parsed descriptors")
    by_fid: dict[str, list[dict]] = {}
    for e in table:
        by_fid.setdefault(e["fid"], []).append(e)
    for fid in sorted(by_fid):
        for e in by_fid[fid]:
            if "cbs" in e:
                out(f"{fid} va={e['va']} shape={e['shape']} default={e['default']} cbs={' '.join(e['cbs'])}")
            else:
                out(f"{fid} va={e['va']} {e.get('note','')}")
    out(f"\nwrote {OUT_JSON.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
