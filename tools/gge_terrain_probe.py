"""Correlate explicit StarRecord starter-world IDs with explicit terrain property packages.

This is a bounded diagnostic only. It performs no directory/profile search, package priority
merge, writes, native PropManager call, or catalogue readiness publication. Duplicate matches
remain visible and therefore do not become ``terrain_loaded`` automatically.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from collections import defaultdict
from pathlib import Path

import prop_table

RECORD = re.compile(
    r"^\[(\d+)\] instance=0x([0-9a-f]+) type=0x([0-9a-f]+) "
    r"group=0x([0-9a-f]+) packed=(\d+) memory=(\d+) compression=0x([0-9a-f]+)$"
)
TERRAIN_GROUP = 0xE38475D9
PROPERTY_TYPE = 0x00B1B104
TERRAIN_PROPERTY = 0x2BD4F5AC


def inventory(exe: Path, package: Path) -> list[dict]:
    result = subprocess.run([str(exe), str(package), "--full"], capture_output=True,
                            timeout=30, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode("utf-8", errors="replace").strip() or
                         "explicit package inventory failed")
    records = []
    for line in result.stdout.decode("utf-8", errors="replace").splitlines():
        match = RECORD.fullmatch(line)
        if not match:
            continue
        ordinal, instance, kind, group, packed, memory, codec = match.groups()
        records.append({"ordinal": int(ordinal), "instance": int(instance, 16),
                        "type": int(kind, 16), "group": int(group, 16),
                        "packed": int(packed), "memory": int(memory),
                        "compression": int(codec, 16)})
    return records


def resource(exe: Path, package: Path, ordinal: int) -> bytes:
    result = subprocess.run([str(exe), str(package), str(ordinal), "--dump"],
                            capture_output=True, timeout=30, check=False)
    if result.returncode:
        raise ValueError(result.stderr.decode("utf-8", errors="replace").strip() or
                         "explicit resource read failed")
    match = re.search(rb"^resource_raw_hex=([0-9a-f]+)\r?$", result.stdout, re.MULTILINE)
    if not match:
        raise ValueError("resource raw hex missing")
    return bytes.fromhex(match.group(1).decode("ascii"))


def terrain_value(payload: bytes) -> dict:
    parsed = prop_table.parse(payload)
    matches = [f for f in parsed if f["id"] == TERRAIN_PROPERTY]
    if len(matches) != 1:
        return {"status": "missing_or_ambiguous", "matches": len(matches),
                "fields": len(parsed), "bytes": len(payload)}
    field = matches[0]
    raw = payload[field["data_offset"]:field["data_offset"] + field["data_bytes"]]
    if field["array"] or field["type"] != 0x0A or field["data_bytes"] != 4:
        return {"status": "wrong_type_or_shape", "type": field["type"],
                "array": field["array"], "bytes": len(payload)}
    return {"status": "type10", "value": int.from_bytes(raw, "big"),
            "type": field["type"], "array": False, "fields": len(parsed),
            "bytes": len(payload), "payload_sha256": hashlib.sha256(payload).hexdigest()}


def rows(capture: dict) -> list[dict]:
    result = []
    for source in ("persisted_saved", "persisted_starters"):
        section = capture.get(source)
        if not isinstance(section, dict):
            continue
        for row in section.get("rows", []):
            star = row.get("star") or {}
            value = star.get("starter_world_id")
            if isinstance(value, str) and value.lower().startswith("0x"):
                result.append({"source": source, "source_index": row.get("source_index"),
                               "star_key": star.get("key"),
                               "starter_world_id": int(value, 16)})
            else:
                result.append({"source": source, "source_index": row.get("source_index"),
                               "star_key": star.get("key"),
                               "starter_world_id": None})
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="one explicit GGE_SAVE_OBSERVATIONS_V1 JSON")
    parser.add_argument("--inventory-inspector", type=Path, required=True)
    parser.add_argument("--resource-inspector", type=Path, required=True)
    parser.add_argument("--package", type=Path, action="append", required=True,
                        help="one explicit DBPF package; repeat, no search or merge")
    args = parser.parse_args()
    capture = json.loads(args.capture.read_text(encoding="utf-8"))
    if capture.get("format") != "GGE_SAVE_OBSERVATIONS_V1":
        raise SystemExit("capture must be GGE_SAVE_OBSERVATIONS_V1")
    source_rows = rows(capture)
    ids = {r["starter_world_id"] for r in source_rows if r["starter_world_id"] is not None}
    matches = defaultdict(list)
    package_reports = []
    for package in args.package:
        records = inventory(args.inventory_inspector, package)
        package_matches = []
        for record in records:
            if record["instance"] not in ids or record["type"] != PROPERTY_TYPE or record["group"] != TERRAIN_GROUP:
                continue
            try:
                detail = terrain_value(resource(args.resource_inspector, package, record["ordinal"]))
            except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                detail = {"status": "blocked", "error": str(exc)}
            item = {"package": str(package), **record, "property": detail}
            package_matches.append(item)
            matches[record["instance"]].append(item)
        package_reports.append({"package": str(package), "status": "inspected",
                                "matching_records": package_matches})
    output_rows = []
    for row in source_rows:
        found = matches.get(row["starter_world_id"], []) if row["starter_world_id"] is not None else []
        values = sorted({item["property"].get("value") for item in found
                         if item["property"].get("status") == "type10"})
        output_rows.append({**row, "match_count": len(found),
                            "status": ("UnknownStarterWorld" if row["starter_world_id"] is None else
                                       "AbsentInExplicitPackages" if not found else
                                       "AmbiguousExplicitPackages" if len(found) != 1 else
                                       "ObservedType10Property"),
                            "type10_values": [f"0x{value:08x}" for value in values]})
    print(json.dumps({"format": "GGE_TERRAIN_CORRELATION_V1",
                      "capture": str(args.capture),
                      "property_group": f"0x{TERRAIN_GROUP:08x}",
                      "property_type": f"0x{PROPERTY_TYPE:08x}",
                      "property_id": f"0x{TERRAIN_PROPERTY:08x}",
                      "rows": output_rows, "packages": package_reports,
                      "native_prop_manager_query": "Unknown",
                      "production_catalogue": False,
                      "limitations": ["explicit packages only", "no package priority merge",
                                      "duplicate matches remain ambiguous", "no native PropManager call",
                                      "no terrain/refill/readiness publication"]}, indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
