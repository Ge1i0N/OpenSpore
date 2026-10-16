"""Read-only structural decode of one real stars.db (DBPF 3.0/index3).

Cross-checks the exact-key layout against the C++ readers without executing
them: extracts the DBPF records, RefPack-decodes each (tools/refpack.py, itself
derived from the original's instructions), parses the context index payload,
walks every class stream's object envelopes (length/CRC/token) and reads the
frame headers of the root and first StarRecord bodies.

It does NOT decode field semantics (that is starsdb/links-reader parity work),
resolve references, or publish catalogue entries. Payload bytes stay in memory;
only structure (sizes, counts, tags, tokens, CRC results) is reported.

Usage:
    python tools/starsdb_real_decode.py <stars.db> [--json-out <path>]

Exit 0 = decoded and reported (any findings). Exit 2 = could not read/parse.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.refpack import RefPackError, decode as refpack_decode
from tools.real_save_probe import HDR, MAGIC, kind_info

MAX_BYTES = 256 * 1024 * 1024
MAX_ENVELOPES = 1 << 20
MAX_FRAME_FIELDS = 128
CRC_CAP = 1000000
OBJECT_TAG = 0x01A80D26


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def u32(b: bytes, at: int) -> int:
    return struct.unpack("<I", b[at:at + 4])[0]


def be32(b: bytes, at: int) -> int:
    return struct.unpack(">I", b[at:at + 4])[0]


_CRC_TABLE = None


def crc_table():
    global _CRC_TABLE
    if _CRC_TABLE is None:
        t = []
        for i in range(256):
            c = i << 24
            for _ in range(8):
                c = (((c << 1) ^ 0x04C11DB7) & 0xFFFFFFFF) if c & 0x80000000 else ((c << 1) & 0xFFFFFFFF)
            t.append(c)
        _CRC_TABLE = t
    return _CRC_TABLE


def crc32_bzip2(data: bytes) -> int:
    # 0x932CD0 table (poly 0x04C11DB7, MSB-first) + 0x932D70 init/complement.
    crc, table = 0xFFFFFFFF, crc_table()
    for byte in data:
        crc = ((crc << 8) ^ table[(crc >> 24) ^ byte]) & 0xFFFFFFFF
    return crc ^ 0xFFFFFFFF


def read_index(data: bytes):
    # Mirror pkg.cpp: kind dword, shared prefix words, per-record words.
    version, minor = u32(data, 4), u32(data, 8)
    count, index_size = u32(data, 0x24), u32(data, 0x2C)
    field_3c = u32(data, 0x3C)
    offset = u32(data, 0x40) or u32(data, 0x28)
    if u32(data, 0) != MAGIC or version != 3 or minor != 0 or field_3c != 3:
        raise ValueError("not a DBPF 3.0/index3 file")
    kind = u32(data, offset)
    ki = kind_info(kind)
    if ki is None or index_size != ki["prefix"] + ki["stride"] * count:
        raise ValueError("index kind/stride mismatch")
    shared = [u32(data, offset + 4 + 4 * i) for i in range(bin(kind & 7).count("1"))]
    recs = []
    base = offset + ki["prefix"]
    for n in range(count):
        words = [u32(data, base + n * ki["stride"] + 4 * x) for x in range(ki["nwords"])]
        si, vals = 0, {}
        for name, i, has in (("type", ki["i_type"], ki["has_type"]),
                             ("group", ki["i_group"], ki["has_group"]),
                             ("unknown", ki["i_unknown"], ki["has_unknown"])):
            vals[name] = shared[si] if not has else words[i]
            si += 0 if has else 1
        vals["instance"] = words[ki["i_instance"]]
        vals["offset"] = words[ki["i_offset"]]
        vals["packed"] = words[ki["i_size"]] & 0x7FFFFFFF
        vals["mem"] = words[ki["i_usize"]]
        vals["comp"] = words[ki["i_flags"]] & 0xFFFF
        vals["flags"] = words[ki["i_flags"]] >> 16
        recs.append(vals)
    return recs


def decode_record(data: bytes, rec: dict):
    blob = data[rec["offset"]:rec["offset"] + rec["packed"]]
    if len(blob) != rec["packed"]:
        return None, "record bytes outside file"
    if rec["comp"] == 0xFFFF:
        try:
            got = refpack_decode(blob, rec["mem"])
        except RefPackError as e:
            return None, f"refpack: {e}"
        if len(got) != rec["mem"]:
            return None, "decoded length != memSize"
        return got, ""
    if rec["comp"] == 0:
        if rec["packed"] != rec["mem"]:
            return None, "raw record packed != mem"
        return blob, ""
    return None, f"unknown compression 0x{rec['comp']:04x}"


def parse_index_payload(payload: bytes):
    # starsdb-context.md: BE version(=1), class_count, total_objects, then pairs.
    if len(payload) < 12:
        return None, "index payload < 12 bytes"
    version, nclass, total = be32(payload, 0), be32(payload, 4), be32(payload, 8)
    if len(payload) != 12 + 8 * nclass:
        return None, "index payload is not exactly 12 + 8*class_count"
    classes = [(be32(payload, 12 + 8 * i), be32(payload, 16 + 8 * i)) for i in range(nclass)]
    return {"version": version, "class_count": nclass, "total": total, "classes": classes}, ""


def walk_envelopes(stream: bytes):
    # starsdb-context.md envelopes: BE total (incl 12B header), BE CRC, BE token, body.
    out_env, pos, n = [], 0, 0
    while pos < len(stream):
        if n >= MAX_ENVELOPES or len(stream) - pos < 12:
            return None, "truncated envelope header"
        total, crc, token = be32(stream, pos), be32(stream, pos + 4), be32(stream, pos + 8)
        if total < 12 or pos + total > len(stream):
            return None, f"envelope overruns stream at 0x{pos:x}"
        covered = stream[pos + 8:pos + 8 + min(total - 8, CRC_CAP)]
        out_env.append({"offset": pos, "total": total, "token": f"0x{token:08x}",
                        "crc_ok": crc32_bzip2(covered) == crc,
                        "crc_capped": total - 8 > CRC_CAP})
        pos += total
        n += 1
    return out_env, ""


def parse_frame_header(payload: bytes, at: int = 0):
    # serialized_frame: BE tag, BE L (bytes after first 8), BE N, N (id,size) pairs.
    if len(payload) - at < 12:
        return None, "frame header < 12 bytes"
    tag, length, count = be32(payload, at), be32(payload, at + 4), be32(payload, at + 8)
    if count > MAX_FRAME_FIELDS or len(payload) - at - 12 < 8 * count:
        return None, "frame directory overruns payload"
    if 8 + length > len(payload) - at:
        return None, "frame length overruns payload"
    fields = [(be32(payload, at + 12 + 8 * i), be32(payload, at + 16 + 8 * i)) for i in range(count)]
    return {"tag": f"0x{tag:08x}", "is_object_tag": tag == OBJECT_TAG,
            "length": length, "count": count,
            "field_ids": [f"0x{f:08x}" for f, _ in fields]}, ""


def main() -> int:
    ap = argparse.ArgumentParser(description="structural decode of one real stars.db")
    ap.add_argument("file", help="explicitly selected stars.db; nothing is scanned")
    ap.add_argument("--json-out", default="", help="write the machine-readable report here")
    args = ap.parse_args()
    path = Path(args.file)
    rep: dict = {"file": str(path)}
    try:
        size = path.stat().st_size
        data = path.read_bytes() if 0 < size <= MAX_BYTES else b""
        if not data:
            raise OSError("empty/unreadable/oversize file")
    except OSError as e:
        out(f"! {path}: {e}")
        return 2
    rep["size"] = size
    try:
        recs = read_index(data)
    except (ValueError, struct.error, IndexError) as e:
        out(f"! {path}: {e}")
        return 2
    rep["records"] = []
    blobs: dict = {}
    for r in recs:
        key = f"0x{r['instance']:08x},0x{r['type']:08x},0x{r['group']:08x}"
        blob, err = decode_record(data, r)
        blobs[key] = blob
        rep["records"].append({"key": key, "packed": r["packed"], "mem": r["mem"],
                               "comp": f"0x{r['comp']:04x}", "decoded": blob is not None,
                               "error": err})
        out(f"{key} packed={r['packed']} mem={r['mem']} comp=0x{r['comp']:04x} "
            f"-> {'decoded' if blob is not None else 'FAIL ' + err}")
    root_key = "0x01897c18,0x01897c18,0x01897c18"
    index_key = "0x0179d304,0x0179d310,0x00000000"
    root, index = blobs.get(root_key), blobs.get(index_key)
    if root is None or index is None:
        rep["verdict"] = "MISSING_ROOT_OR_INDEX"
        out("verdict=MISSING_ROOT_OR_INDEX")
        return finish(rep, args.json_out)
    parsed, err = parse_index_payload(index)
    if parsed is None:
        rep["verdict"] = "INDEX_UNPARSED"
        rep["reason"] = err
        out(f"verdict=INDEX_UNPARSED ({err})")
        return finish(rep, args.json_out)
    rep["context_index"] = {**parsed,
                            "classes": [[f"0x{c:08x}", n] for c, n in parsed["classes"]]}
    out(f"context index: version={parsed['version']} classes={parsed['class_count']} "
        f"total={parsed['total']} unique_ids={len({c for c, _ in parsed['classes']})}")
    by_type = {}
    for r in recs:
        by_type.setdefault(f"0x{r['type']:08x}", []).append(
            f"0x{r['instance']:08x},0x{r['type']:08x},0x{r['group']:08x}")
    rep["classes"] = []
    notes = []
    for class_id, want_n in parsed["classes"]:
        rec_key = f"0x0179d304,0x{class_id:08x},0x00000000"
        stream = blobs.get(rec_key)
        entry: dict = {"class_id": f"0x{class_id:08x}", "declared": want_n,
                       "record": "FOUND" if stream is not None else "MISSING"}
        if stream is None:
            notes.append(f"class 0x{class_id:08x} declared but no record")
            rep["classes"].append(entry)
            continue
        envs, err = walk_envelopes(stream)
        if envs is None:
            entry["envelopes"] = err
            notes.append(f"class 0x{class_id:08x}: {err}")
        else:
            entry["envelopes"] = len(envs)
            entry["crc_fail"] = sum(1 for e in envs if not e["crc_ok"])
            entry["max_envelope"] = max(e["total"] for e in envs)
            entry["tokens_head"] = [e["token"] for e in envs[:4]]
            if len(envs) != want_n:
                notes.append(f"class 0x{class_id:08x}: {len(envs)} envelopes vs {want_n} declared")
            if class_id == 0x03E4353C and envs:
                body_at = envs[0]["offset"] + 12
                frame, ferr = parse_frame_header(stream, body_at)
                entry["first_starrecord_frame"] = frame if frame else ferr
        rep["classes"].append(entry)
        out(f"class 0x{class_id:08x} x{want_n}: {entry.get('envelopes')} envelopes, "
            f"crc_fail={entry.get('crc_fail', '?')}")
    frame, ferr = parse_frame_header(root, 0)
    rep["root_frame"] = frame if frame else {"error": ferr}
    out(f"root first frame: {frame if frame else ferr}")
    rep["notes"] = notes
    rep["verdict"] = "DECODED_STRUCTURE" if not notes else "DECODED_WITH_NOTES"
    out(f"verdict={rep['verdict']}")
    return finish(rep, args.json_out)


def finish(rep: dict, json_out: str) -> int:
    if json_out:
        try:
            Path(json_out).write_text(json.dumps(rep, indent=2), encoding="utf-8")
            out(f"report -> {json_out}")
        except OSError as e:
            out(f"! cannot write report: {e}")
            return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
