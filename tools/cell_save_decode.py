"""Full decode of the cell-stage save's multi-object class streams.

Structure discovered:
  Single-envelope streams (0x03a3ac9f, 0xabcdef00):
    [total(4) BE][crc(4) BE][token(4) BE][obj_token(4) BE][frame...]
  Multi-object streams (0x018eb4b7, 0x018eb45e):
    [count(4) BE][crc(4) BE][token(4) BE][12-byte header][frame][token(4)][frame][token(4)]...

Each frame is a serialized field frame (tag 0x01A80D26). The 12-byte header
after count/crc/token is not yet interpreted.

Usage:
    python tools/cell_save_decode.py <cell-save.spo> [--json-out <path>]
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.starsdb_real_decode import read_index, parse_frame_header
from tools.refpack import decode as rd

MAX_BYTES = 256 * 1024 * 1024
OBJECT_TAG = 0x01A80D26


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def be32(b: bytes, at: int) -> int:
    return struct.unpack(">I", b[at:at + 4])[0]


def decode_multi_object(blob: bytes):
    """Parse [count][crc][token][header][frame][token]... structure."""
    if len(blob) < 24:
        return None, "stream too short"
    count = be32(blob, 0)
    crc = be32(blob, 4)
    token = be32(blob, 8)
    # 12-byte header at offset 12
    header = blob[12:24]
    pos = 24
    objects = []
    for n in range(count):
        if pos + 12 > len(blob):
            return None, f"truncated at object {n}"
        frame, err = parse_frame_header(blob, pos)
        if frame is None and n == 0:
            # direct debug: what does parse_frame_header see?
            tag, length, count = be32(blob, pos), be32(blob, pos+4), be32(blob, pos+8)
            return None, (f"first frame parse failed: {err}; blob_len={len(blob)}; "
                           f"at={pos}; tag=0x{tag:08x} length={length} count={count}; "
                           f"len-at-12={len(blob)-pos-12}; 8*count={8*count}")
        if frame is None:
            return None, f"object {n}: {err}"
        frame_end = pos + 8 + frame["length"]
        # token after frame
        if frame_end + 4 > len(blob):
            return None, f"object {n}: no token after frame"
        obj_token = be32(blob, frame_end)
        body = blob[pos + 12:frame_end]
        objects.append({
            "index": n,
            "token": f"0x{obj_token:08x}",
            "frame_tag": frame["tag"],
            "frame_length": frame["length"],
            "field_count": frame["count"],
            "fields": frame["field_ids"],
        })
        pos = frame_end + 4
    return {"count": count, "crc": f"0x{crc:08x}", "token": f"0x{token:08x}",
            "header": header.hex(), "objects": objects, "consumed": pos,
            "total": len(blob)}, ""


def decode_single_envelope(blob: bytes):
    """Parse [total][crc][token][obj_token][frame...] structure."""
    if len(blob) < 16:
        return None, "stream too short"
    total = be32(blob, 0)
    crc = be32(blob, 4)
    token = be32(blob, 8)
    obj_token = be32(blob, 12)
    frame, err = parse_frame_header(blob, 16)
    if frame is None:
        return None, err
    return {"total": total, "crc": f"0x{crc:08x}", "token": f"0x{token:08x}",
            "obj_token": f"0x{obj_token:08x}", "frame": frame}, ""


def main() -> int:
    ap = argparse.ArgumentParser(description="decode cell-stage save class streams")
    ap.add_argument("file", help="explicitly selected .spo file (or size in bytes)")
    ap.add_argument("--json-out", default="", help="write the machine-readable report here")
    args = ap.parse_args()
    # Cyrillic filenames get mangled through PowerShell; allow size-based lookup
    if args.file.isdigit():
        size = int(args.file)
        d = Path('C:/Users/gelion/AppData/Roaming/SPORE/Games/Game0')
        matches = [x for x in d.glob('*.spo') if x.stat().st_size == size]
        if not matches:
            out(f"! no .spo with size {size}")
            return 2
        path = matches[0]
    else:
        path = Path(args.file)
    data = path.read_bytes()
    if len(data) > MAX_BYTES:
        out(f"! file exceeds host limit")
        return 2
    recs = read_index(data)
    rep: dict = {"file": str(path), "size": len(data), "records": []}
    for r in recs:
        key = f"0x{r['instance']:08x},0x{r['type']:08x},0x{r['group']:08x}"
        blob = data[r['offset']:r['offset'] + r['packed']]
        if r['comp'] == 0xFFFF:
            try:
                blob = rd(blob, r['mem'])
            except Exception as e:
                out(f"{key} REFPACK FAIL: {e}")
                continue
        elif r['comp'] != 0:
            out(f"{key} unknown comp 0x{r['comp']:04x}")
            continue
        entry: dict = {"key": key, "packed": r['packed'], "mem": r['mem']}
        # try multi-object first
        result, err = decode_multi_object(blob)
        if result is None:
            result, err = decode_single_envelope(blob)
            if result is None:
                entry["error"] = err
            else:
                entry["format"] = "single_envelope"
                entry.update(result)
        else:
            entry["format"] = "multi_object"
            entry.update(result)
        rep["records"].append(entry)
        # summary line
        if "format" in entry:
            if entry["format"] == "multi_object":
                objs = entry["objects"]
                out(f"{key}: multi_object count={entry['count']} objects={len(objs)} "
                    f"consumed={entry['consumed']}/{entry['total']}")
                if objs:
                    f0 = objs[0]
                    out(f"  obj0: token={f0['token']} fields={f0['field_count']} "
                        f"ids={f0['fields'][:8]}")
            else:
                out(f"{key}: single_envelope total={entry['total']} "
                    f"frame_fields={entry['frame']['count']}")
        else:
            out(f"{key}: ERROR {entry.get('error')}")
    if args.json_out:
        try:
            Path(args.json_out).write_text(json.dumps(rep, indent=2), encoding="utf-8")
            out(f"report -> {args.json_out}")
        except OSError as e:
            out(f"! cannot write report: {e}")
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
