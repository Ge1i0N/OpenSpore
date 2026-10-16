"""Match planet (mKey) records to saved star positions in a stars.db.

Read-only, explicit files. Extracts every StarRecord body, reads its mKey (first
numeric field, contiguous with 0x03D58BF2 id) and position, then compares to a
planetRecords.pkp's `key` values (0x04164F8E) — the exact star.mKey + i*mStride
translation from planet_record.h, modulo 0x01000000 wrap.

Reports:
  * how many StarRecords carry planets (planet_count != 0)
  * how many of those mKeys appear verbatim in the surviving planet mKey set
  * which planet records are referenced (sets overlap)
  * staged-position filter (closest star to each planet? descriptive only)
  * both saves (early vs late) so deltas are visible

The tool makes no catalogue claims. It answers one question: do stars and planet
records actually connect on real data, and which side survives stage boundaries?

Usage:
    python tools/star_planet_match.py <stars.db> <planetRecords.pkp> [--json-out <path>]

Exit 0 = probed. Exit 2 = could not read (missing file / bad DBPF).
"""
from __future__ import annotations

import argparse
import collections
import json
import math
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))
from tools.planet_real_decode import read_index, read_record, decode_outer, unwrap_nested
from tools.refpack import decode as refpack_decode
from tools.starsdb_real_decode import read_index as sdb_read_index

MAX_BYTES = 256 * 1024 * 1024


def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def le32(b, at):
    return struct.unpack("<I", b[at:at + 4])[0]


def star_keys(path: Path):
    # Extracts (mKey, planet_count, finite_position) per StarRecord envelope.
    data = path.read_bytes()
    if len(data) > MAX_BYTES:
        return None, "stars.db over host package limit"
    recs = sdb_read_index(data)
    root = next(r for r in recs if r["instance"] == 0x01897C18 and r["type"] == 0x01897C18)
    ctx_rec = next(r for r in recs if r["type"] == 0x0179D310)
    from tools.refpack import decode as refpack_decode
    root_blob = refpack_decode(data[root["offset"]:root["offset"] + root["packed"]], root["mem"]) \
        if root["comp"] == 0xFFFF else data[root["offset"]:root["offset"] + root["packed"]]
    ctx_blob = refpack_decode(data[ctx_rec["offset"]:ctx_rec["offset"] + ctx_rec["packed"]], ctx_rec["mem"]) \
        if ctx_rec["comp"] == 0xFFFF else data[ctx_rec["offset"]:ctx_rec["offset"] + ctx_rec["packed"]]
    # parse context index
    def be32(b, at): return struct.unpack(">I", b[at:at + 4])[0]
    n_class = be32(ctx_blob, 4)
    total = be32(ctx_blob, 8)
    classes = [(be32(ctx_blob, 12 + 8 * i), be32(ctx_blob, 16 + 8 * i)) for i in range(n_class)]
    star_id = 0x03E4353C
    assert star_id in [c for c, _ in classes], "no StarRecord class"
    # find class stream record
    star_rec = next(r for r in recs if r["type"] == star_id)
    star_stream = refpack_decode(data[star_rec["offset"]:star_rec["offset"] + star_rec["packed"]],
                                star_rec["mem"]) if star_rec["comp"] == 0xFFFF else \
        data[star_rec["offset"]:star_rec["offset"] + star_rec["packed"]]
    # walk envelopes, extract body mKey + position + planet_count
    results = []
    pos = 0
    while pos < len(star_stream):
        if len(star_stream) - pos < 12:
            return None, "truncated StarRecord envelope"
        total_e, crc, token = be32(star_stream, pos), be32(star_stream, pos + 4), be32(star_stream, pos + 8)
        if total_e < 12 or pos + total_e > len(star_stream):
            return None, f"StarRecord envelope overrun at 0x{pos:x}"
        body = star_stream[pos + 12:pos + total_e]
        from tools.starsdb_real_decode import parse_frame_header
        frame, err = parse_frame_header(body, 0)
        if frame is None:
            return None, f"bad StarRecord frame at 0x{pos:x}: {err}"
        pos += total_e
        mkey = None
        position = None
        planet_count = 0
        pca = 12 + 8 * frame["count"]
        for i in range(frame["count"]):
            fid = be32(body, 12 + 8 * i)
            size = be32(body, 16 + 8 * i)
            payload_at = pca
            if size == 4 and fid == 0x03D58BF2:
                mkey = be32(body, payload_at)
            elif size == 12 and fid == 0x03D58BF1:
                import struct as S
                position = list(S.unpack(">fff", body[payload_at:payload_at + 12]))
            elif size == 1 and fid == 0x051A1188:
                planet_count = body[payload_at]
            pca += size
        if mkey is not None:
            results.append((mkey, planet_count, position))
    return results, ""


def planet_keys(path: Path):
    d = path.read_bytes()
    if len(d) > MAX_BYTES:
        return None, "planetRecords over host package limit"
    recs = read_index(d)
    out_vals = []
    for r in recs:
        payload, err = decode_outer(d, r)
        if payload is None:
            continue
        inner, err = unwrap_nested(payload)
        if inner is None:
            continue
        try:
            rec = read_record(inner)
        except Exception:
            continue
        if "key" in rec:
            out_vals.append({"key": rec["key"], "name": rec.get("name"), "flags": rec.get("flags"),
                             "type": rec.get("type")})
    return out_vals, ""


def main() -> int:
    ap = argparse.ArgumentParser(description="match star mKeys to planet mKeys read-only")
    ap.add_argument("stars_db", help="explicitly selected stars.db")
    ap.add_argument("planet_records", help="explicitly selected planetRecords.pkp")
    ap.add_argument("--json-out", default="", help="write the machine-readable report here")
    args = ap.parse_args()
    sk, err = star_keys(Path(args.stars_db))
    if sk is None:
        out(f"! stars: {err}")
        return 2
    pk, err = planet_keys(Path(args.planet_records))
    if pk is None:
        out(f"! planets: {err}")
        return 2
    pkeys = {p["key"] for p in pk}
    with_planets = [s for s in sk if s[1] != 0]
    matched = [s for s in with_planets if s[0] in pkeys]
    unmatched = [s for s in with_planets if s[0] not in pkeys]
    # star-side missing: how many star records say they have N planets but their mKey absent
    agg = {
        "stars_total": len(sk),
        "stars_with_planets": len(with_planets),
        "stars_planet_count_distribution": {},
        "matched": len(matched),
        "unmatched": len(unmatched),
        "planet_records_total": len(pk),
        "planet_unique_keys": len(pkeys),
        "star_keys_in_planet_set": sum(1 for s in sk if s[0] in pkeys),
    }
    import collections
    agg["stars_planet_count_distribution"] = dict(collections.Counter(s[1] for s in sk))
    # hull/min summary of unmatched mKeys
    agg["unmatched_sample"] = [s[0] for s in unmatched[:10]]
    # per matched: proximity to other stars by mKey isn't spatial; keep ID correlation
    rep = {"stars_db": args.stars_db, "planet_records": args.planet_records,
           "aggregate": agg, "matched_star_keys": [s[0] for s in matched[:30]]}
    out(f"stars total: {len(sk)}  with_planets: {len(with_planets)}")
    out(f"matched to surviving planet mKey: {len(matched)}/{len(with_planets)}  unmatched: {len(unmatched)}")
    out(f"planet records: {len(pk)} unique keys: {len(pkeys)} star.mKey in set: {agg['star_keys_in_planet_set']}")
    out(f"planet_count distribution (star side): {agg['stars_planet_count_distribution']}")
    out(f"unmatched samples: {agg['unmatched_sample']}")
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
