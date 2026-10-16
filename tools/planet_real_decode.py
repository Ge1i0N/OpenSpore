"""Read-only value decode of one real planetRecords.pkp (DBPF 3.0/index3).

Mirrors source/engine/planet_record.cpp read_record exactly: BE civilization /
tribe / event / commodity-node counts, per-city 28 raw bytes, field frames
walked but not gameplay-decoded, then the PlanetRecord field frame (key, type,
tech_level, flags, name, terrain key). Aggregates only; the first few names are
kept as samples, everything else is counts and histograms.

Usage:
    python tools/planet_real_decode.py <planetRecords.pkp> [--json-out <path>]

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
MAX_NODES = 4096
MAX_NAME = 4096
MAX_FRAME_FIELDS = 128
OBJECT_TAG = 0x01A80D26

KEY_ID, TYPE_ID, TECH_ID, FLAGS_ID = 0x04164F8E, 0x03D58B9E, 0x03D58B9F, 0x03D58B99
NAME_ID, TERRAIN_ID = 0x03D58B9A, 0x03D58B9B


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def u32(b: bytes, at: int) -> int:
    return struct.unpack("<I", b[at:at + 4])[0]


def be32(b: bytes, at: int) -> int:
    return struct.unpack(">I", b[at:at + 4])[0]


def ascii_safe(units) -> str:
    return "".join(chr(u) if 32 <= u < 127 else "?" for u in units)


class WalkError(Exception):
    pass


class Walker:
    # Mirrors planet_record.cpp Walker: counts, frames, city prefix, aggregate cap.
    def __init__(self, data: bytes):
        self.d, self.at, self.nodes, self.frames = data, 0, 0, 0

    def count(self) -> int:
        if len(self.d) - self.at < 4:
            raise WalkError("truncated planet collection count")
        count = be32(self.d, self.at)
        self.at += 4
        if count > MAX_NODES - self.nodes:
            raise WalkError("planet collections exceed host aggregate node limit 4096")
        self.nodes += count
        return count

    def frame(self):
        tag, length, nfields, end, spans = parse_frame(self.d, self.at)
        self.at = end
        self.frames += 1
        return nfields

    def civilization(self):
        cities = self.count()
        for _ in range(cities):
            if len(self.d) - self.at < 28:
                raise WalkError("truncated city fourteen-word prefix")
            self.at += 28
            self.frame()
        vehicles = self.count()
        for _ in range(vehicles):
            self.frame()
        self.frame()


def parse_frame(data: bytes, at: int):
    # Serialized field frame: BE tag, BE L, BE N, N (id,size) pairs, payloads.
    if len(data) - at < 12:
        raise WalkError("frame header < 12 bytes")
    tag, length, count = be32(data, at), be32(data, at + 4), be32(data, at + 8)
    if tag != OBJECT_TAG:
        raise WalkError(f"frame tag 0x{tag:08x} is not the object tag")
    if count > MAX_FRAME_FIELDS or len(data) - at - 12 < 8 * count:
        raise WalkError("frame directory overruns payload")
    if 8 + length > len(data) - at:
        raise WalkError("frame length overruns payload")
    fields = [(be32(data, at + 12 + 8 * i), be32(data, at + 16 + 8 * i)) for i in range(count)]
    pos = at + 12 + 8 * count
    spans = []
    for fid, size in fields:
        if pos + size > len(data):
            raise WalkError("field payload overruns frame")
        spans.append((fid, pos, size))
        pos += size
    return tag, length, count, at + 8 + length, spans


def read_record(data: bytes) -> dict:
    w = Walker(data)
    rec: dict = {"civs": w.count(), "tribes": 0, "events": 0, "nodes": 0}
    for _ in range(rec["civs"]):
        w.civilization()
    for name in ("tribes", "events", "nodes"):
        n = w.count()
        rec[name] = n
        for _ in range(n):
            w.frame()
    _, _, _, end, spans = parse_frame(data, w.at)
    rec["frames_walked"] = w.frames + 1
    rec["tail"] = len(data) - end
    for fid, at, size in spans:
        if size == 0:
            continue
        if fid in (KEY_ID, TYPE_ID, TECH_ID, FLAGS_ID):
            if size != 4:
                raise WalkError("planet key/type/stage/flags must contain one BE word")
            rec[{KEY_ID: "key", TYPE_ID: "type", TECH_ID: "tech", FLAGS_ID: "flags"}[fid]] = be32(data, at)
        elif fid == NAME_ID:
            if size < 4:
                raise WalkError("truncated planet name length")
            count = be32(data, at)
            if count > MAX_NAME or count != (size - 4) // 2 or (size - 4) % 2:
                raise WalkError("planet name length/host limit mismatch")
            units = struct.unpack("<%dH" % count, data[at + 4:at + 4 + 2 * count]) if count else ()
            rec["name"] = ascii_safe(units)
        elif fid == TERRAIN_ID:
            if size != 12:
                raise WalkError("planet terrain key must contain three BE words")
            rec["terrain"] = [be32(data, at + 8), be32(data, at + 4), be32(data, at)]
    return rec


def read_index(data: bytes):
    if len(data) < HDR or u32(data, 0) != MAGIC:
        raise ValueError("not a DBPF file")
    version, minor = u32(data, 4), u32(data, 8)
    count, index_size = u32(data, 0x24), u32(data, 0x2C)
    field_3c = u32(data, 0x3C)
    offset = u32(data, 0x40) or u32(data, 0x28)
    if version != 3 or minor != 0 or field_3c != 3:
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
        recs.append(vals)
    return recs


WRAP_PREFIX = b"\x00\x25\x00\x03"  # constant 4-byte prefix on all observed records


def decode_outer(data: bytes, r: dict):
    blob = data[r["offset"]:r["offset"] + r["packed"]]
    if len(blob) != r["packed"]:
        return None, "record bytes outside file"
    if r["comp"] == 0xFFFF:
        try:
            payload = refpack_decode(blob, r["mem"])
        except RefPackError as e:
            return None, f"refpack: {e}"
        if len(payload) != r["mem"]:
            return None, "decoded length != memSize"
        return payload, ""
    if r["comp"] == 0 and r["packed"] == r["mem"]:
        return blob, ""
    return None, f"unknown compression 0x{r['comp']:04x}"


def unwrap_nested(payload: bytes):
    # Real pkp transport: 4-byte prefix (constant on every observed record;
    # meaning unestablished) + nested DBPF v3 holding exactly 1 record whose
    # bytes are the read_record input.
    if len(payload) < 4 + HDR or payload[:4] != WRAP_PREFIX or payload[4:8] != b"DBPF":
        return None, "not a prefixed nested planet DBPF"
    nested = payload[4:]
    try:
        inner_recs = read_index(nested)
    except (ValueError, struct.error, IndexError) as e:
        return None, f"nested index: {e}"
    if len(inner_recs) != 1:
        return None, "nested planet DBPF does not hold exactly 1 record"
    return decode_outer(nested, inner_recs[0])


def main() -> int:
    ap = argparse.ArgumentParser(description="value decode of one real planetRecords.pkp")
    ap.add_argument("file", help="explicitly selected file; nothing is scanned")
    ap.add_argument("--json-out", default="", help="write the machine-readable report here")
    args = ap.parse_args()
    path = Path(args.file)
    rep: dict = {"file": str(path)}
    try:
        size = path.stat().st_size
        data = path.read_bytes() if 0 < size <= MAX_BYTES else b""
        if not data:
            raise OSError("empty/unreadable/oversize file")
        recs = read_index(data)
    except (OSError, ValueError, struct.error, IndexError) as e:
        out(f"! {path}: {e}")
        return 2
    rep["size"], rep["records"] = size, len(recs)
    agg: dict = {"decoded_ok": 0, "decode_fail": 0, "unwrapped": 0, "unwrap_fail": 0,
                 "value_ok": 0, "value_fail": 0,
                 "civs": 0, "tribes": 0, "events": 0, "nodes": 0, "tails": 0,
                 "names_present": 0, "name_samples": [], "terrains": 0,
                 "distinct": {}}
    for r in recs:
        key = f"0x{r['instance']:08x},0x{r['type']:08x},0x{r['group']:08x}"
        payload, err = decode_outer(data, r)
        if payload is None:
            agg["decode_fail"] += 1
            continue
        agg["decoded_ok"] += 1
        inner, err = unwrap_nested(payload)
        if inner is None:
            agg["unwrap_fail"] += 1
            continue
        agg["unwrapped"] += 1
        try:
            rec = read_record(inner)
        except WalkError:
            agg["value_fail"] += 1
            continue
        agg["value_ok"] += 1
        for coll in ("civs", "tribes", "events", "nodes"):
            agg[coll] += rec[coll]
        if rec["tail"]:
            agg["tails"] += 1
        if "name" in rec:
            agg["names_present"] += 1
            if len(agg["name_samples"]) < 5:
                agg["name_samples"].append(rec["name"][:48])
        if "terrain" in rec:
            agg["terrains"] += 1
        for name in ("key", "type", "tech", "flags"):
            if name in rec:
                hist = agg["distinct"].setdefault(name, {})
                hist[rec[name]] = hist.get(rec[name], 0) + 1
    agg["distinct"] = {k: sorted(v.items(), key=lambda kv: -kv[1])[:8] for k, v in agg["distinct"].items()}
    rep["values"] = agg
    out(f"{path}: {len(recs)} records, decoded {agg['decoded_ok']}, "
        f"unwrapped {agg['unwrapped']}/{agg['unwrap_fail']} unwrap-fail, "
        f"values ok {agg['value_ok']}/{agg['value_fail']} fail, "
        f"civs={agg['civs']} tribes={agg['tribes']} events={agg['events']} nodes={agg['nodes']} "
        f"tails={agg['tails']} names={agg['names_present']}")
    rep["verdict"] = "DECODED_VALUES" if not agg["decode_fail"] and not agg["unwrap_fail"] \
        and not agg["value_fail"] and not agg["tails"] else "DECODED_WITH_NOTES"
    out(f"verdict={rep['verdict']}")
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
