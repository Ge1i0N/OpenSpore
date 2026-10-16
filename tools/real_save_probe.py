"""Read-only structural probe of one explicitly selected DBPF file.

Authority: the shipped file itself (#2) checked against the exact gates our C++
readers enforce - the pkg.cpp header validator (Package::parse) and the strict
starsdb_dbpf.cpp 3.0/index3 subset (ExplicitDbpfReader::open_from_memory).
It never writes to the probed file and never scans directories: pass the file.

Usage:
    python tools/real_save_probe.py <file> [--key 0xINST,0xTYPE,0xGROUP ...]
        [--list-keys N] [--json-out <path>]

Exit 0 = the file was probed and the report is complete (even when the verdict
is "not usable by the adapter": that is a finding, not a failure).
Exit 2 = the file could not be probed (missing/unreadable/truncated).
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

MAGIC = 0x46504244  # "DBPF", an inline immediate in the binary (pkg.h)
HDR = 0x60
MAX_BYTES = 256 * 1024 * 1024
LIST_CAP = 256


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def u32(b: bytes, at: int) -> int:
    return struct.unpack("<I", b[at:at + 4])[0]


def kind_info(k: int):
    # Mirrors pkg.cpp kind_info exactly: bits mask SHARED fields
    # (bit0 type, bit1 group, bit2 unknown); full record is 32 bytes.
    shared = bin(k & 7).count("1")
    prefix, stride = 4 + 4 * shared, 32 - 4 * shared
    if stride < 20 or stride > 32 or stride % 4:
        return None
    w = 0
    i_type, has_type = w, not (k & 1)
    w += has_type
    i_group, has_group = w, not (k & 2)
    w += has_group
    i_unknown, has_unknown = w, not (k & 4)
    w += has_unknown
    i_instance = w
    w += 1
    i_offset, i_size, i_usize, i_flags = w, w + 1, w + 2, w + 3
    w += 4
    if w != stride // 4:
        return None
    return {"prefix": prefix, "stride": stride, "nwords": stride // 4,
            "i_type": i_type, "has_type": has_type,
            "i_group": i_group, "has_group": has_group,
            "i_unknown": i_unknown, "has_unknown": has_unknown,
            "i_instance": i_instance, "i_offset": i_offset,
            "i_size": i_size, "i_usize": i_usize, "i_flags": i_flags}


def parse_key(s: str):
    parts = s.split(",")
    if len(parts) != 3:
        raise ValueError(f"key must be INST,TYPE,GROUP: {s!r}")
    return tuple(int(p.strip(), 0) & 0xFFFFFFFF for p in parts)


def raw_index(data: bytes, size: int, index_offset: int, index_size: int) -> dict:
    # Unparsed layout: the kind/stride formula was measured on 3.0 packages only,
    # so for anything else the index dwords are evidence, not record keys.
    if index_offset < HDR or index_offset >= size:
        return {"raw_dwords": [], "raw_note": "index offset outside file"}
    n = min(8, max(0, (min(index_size, size - index_offset)) // 4))
    return {"raw_dwords": [f"0x{u32(data, index_offset + 4 * i):08x}" for i in range(n)],
            "raw_note": "unparsed layout: dwords, not keys"}


def probe(path: Path, want_keys, list_n: int) -> dict:
    rep: dict = {"file": str(path)}
    try:
        size = path.stat().st_size
    except OSError as e:
        rep["error"] = f"cannot stat file: {e}"
        return rep
    rep["size"] = size
    if size > MAX_BYTES:
        rep["error"] = "file exceeds 256 MiB host package limit"
        return rep
    try:
        data = path.read_bytes()
    except OSError as e:
        rep["error"] = f"cannot read file: {e}"
        return rep
    if len(data) < HDR:
        rep["error"] = f"short file: {len(data)} bytes < 0x60-byte header"
        return rep
    hdr = {k: u32(data, o) for k, o in (
        ("magic", 0x00), ("version", 0x04), ("minor", 0x08), ("count", 0x24),
        ("alt_offset", 0x28), ("index_size", 0x2c), ("field_3c", 0x3c),
        ("index_offset_raw", 0x40))}
    rep["header"] = {k: (f"0x{v:08x}" if k in ("magic",) else v) for k, v in hdr.items()}
    index_offset = hdr["index_offset_raw"] or hdr["alt_offset"]
    rep["index_offset"] = index_offset
    # Validator replay, same clauses and order as pkg.cpp Package::parse.
    if hdr["magic"] != MAGIC:
        rep["verdict"] = "NOT_DBPF"
        return rep
    for why in (
            "version > 3" if hdr["version"] > 3 else None,
            "count >= 0x07FFFFFF" if hdr["count"] >= 0x07FFFFFF else None,
            "index_offset >= filesize" if index_offset >= size else None,
            "index_offset + index_size > filesize"
            if index_offset + hdr["index_size"] > size else None,
            "header +0x3c is zero" if hdr["field_3c"] == 0 else None):
        if why:
            rep["verdict"] = "HEADER_REJECTED"
            rep["reason"] = why
            return rep
    # Strict starsdb subset gate, same order as starsdb_dbpf.cpp.
    # A refused file still leaves its raw index dwords as evidence.
    if not (hdr["version"] == 3 and hdr["minor"] == 0 and hdr["field_3c"] == 3):
        rep["verdict"] = "DBPF_OK_NOT_STARSDB"
        rep["reason"] = (f"version={hdr['version']} minor={hdr['minor']} "
                         f"field_3c={hdr['field_3c']} (adapter requires 3.0/index3)")
        rep.update(raw_index(data, size, index_offset, hdr["index_size"]))
        return rep
    if hdr["count"] > 65536:
        rep["verdict"] = "DBPF_OK_NOT_STARSDB"
        rep["reason"] = "record count exceeds host limit 65536"
        return rep
    if index_offset < HDR or index_offset > size or hdr["index_size"] < 4 or \
            hdr["index_size"] > size - index_offset:
        rep["verdict"] = "DBPF_OK_NOT_STARSDB"
        rep["reason"] = "DBPF directory outside input"
        return rep
    rep["strict_3_0_index3"] = True
    kind = u32(data, index_offset)
    rep["kind"] = kind
    if kind > 7:
        rep["verdict"] = "DBPF_OK_NOT_STARSDB"
        rep["reason"] = "unsupported DBPF shared-field mask"
        return rep
    ki = kind_info(kind)
    expect = (ki["prefix"] + ki["stride"] * hdr["count"]) if ki else None
    rep["index_formula"] = {"prefix": ki["prefix"] if ki else None,
                            "stride": ki["stride"] if ki else None,
                            "expected_size": expect}
    if expect != hdr["index_size"]:
        rep["verdict"] = "DBPF_OK_NOT_STARSDB"
        rep["reason"] = "directory count/stride mismatch"
        return rep
    # Enumerate record keys (bounded): shared prefix words then per-record words.
    shared_words = [u32(data, index_offset + 4 + 4 * i)
                    for i in range(bin(kind & 7).count("1"))]
    keys = []
    base = index_offset + ki["prefix"]
    for n in range(hdr["count"]):
        words = [u32(data, base + n * ki["stride"] + 4 * x) for x in range(ki["nwords"])]
        si = 0
        vals = {}
        for name, i, has in (("type", ki["i_type"], ki["has_type"]),
                             ("group", ki["i_group"], ki["has_group"]),
                             ("unknown", ki["i_unknown"], ki["has_unknown"])):
            vals[name] = shared_words[si] if not has else words[i]
            si += 0 if has else 1
        vals["instance"] = words[ki["i_instance"]]
        keys.append(vals)
    rep["record_count"] = len(keys)
    rep["keys_shown"] = min(list_n, len(keys))
    rep["keys"] = [f"0x{k['instance']:08x},0x{k['type']:08x},0x{k['group']:08x}" for k in keys[:list_n]]
    hits = {}
    for wk in want_keys:
        inst, typ, grp = wk
        n = sum(1 for k in keys
                if k["instance"] == inst and k["type"] == typ and k["group"] == grp)
        hits[f"0x{inst:08x},0x{typ:08x},0x{grp:08x}"] = ("FOUND" if n == 1 else
                                                        "ABSENT" if n == 0 else f"DUPLICATE x{n}")
    rep["wanted"] = hits
    rep["verdict"] = "STARSDB_SHAPED"
    return rep


def main() -> int:
    ap = argparse.ArgumentParser(description="read-only structural probe of one DBPF file")
    ap.add_argument("file", help="explicitly selected file; nothing is scanned")
    ap.add_argument("--key", action="append", default=[],
                    help="INST,TYPE,GROUP hex key to look up (repeatable)")
    ap.add_argument("--list-keys", type=int, default=16, help="record keys to print (default 16)")
    ap.add_argument("--json-out", default="", help="write the machine-readable report here")
    args = ap.parse_args()
    try:
        want = [parse_key(k) for k in args.key]
    except ValueError as e:
        out(f"! {e}")
        return 2
    rep = probe(Path(args.file), want, max(0, args.list_keys))
    if "error" in rep:
        out(f"! {rep['file']}: {rep['error']}")
        return 2
    h = rep["header"]
    out(f"== {rep['file']} ({rep['size']} bytes)")
    out(f"magic={h['magic']} version={h['version']} minor={h['minor']} "
        f"count={h['count']} index_size={h['index_size']} field_3c={h['field_3c']} "
        f"index_offset=0x{rep['index_offset']:x}")
    out(f"strict_3_0_index3={rep.get('strict_3_0_index3', False)}")
    if "kind" in rep:
        out(f"kind={rep['kind']} prefix={rep['index_formula']['prefix']} "
            f"stride={rep['index_formula']['stride']}")
    if "raw_dwords" in rep:
        out(f"raw index dwords ({rep['raw_note']}):")
        for d in rep["raw_dwords"]:
            out(f"  {d}")
    if "record_count" in rep:
        out(f"records={rep['record_count']} (showing {rep['keys_shown']})")
        for k in rep["keys"]:
            out(f"  {k}")
    for k, v in rep.get("wanted", {}).items():
        out(f"key {k}: {v}")
    out(f"verdict={rep['verdict']}" + (f" ({rep['reason']})" if "reason" in rep else ""))
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
