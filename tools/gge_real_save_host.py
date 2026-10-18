"""Build a host-only GGERealSavePreview input from explicit save observations.

The output is deliberately *not* GGE_CATALOG_V1. It contains saved rows only, an explicitly empty
candidate list, and Unknown producer/refill statuses. Terrain image instances are accepted only
from a caller-selected GGE_TERRAIN_CORRELATION_V1 report whose rows are EquivalentExplicitPackages.
No profile search, save write, native service or readiness promotion occurs.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def hex_utf8(value: str) -> str:
    return value.encode("utf-8", errors="strict").hex()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("observations", type=Path)
    ap.add_argument("correlation", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    capture = json.loads(args.observations.read_text(encoding="utf-8"))
    correlation = json.loads(args.correlation.read_text(encoding="utf-8"))
    if capture.get("format") != "GGE_SAVE_OBSERVATIONS_V1":
        raise SystemExit("observations must be GGE_SAVE_OBSERVATIONS_V1")
    if correlation.get("format") != "GGE_TERRAIN_CORRELATION_V1":
        raise SystemExit("correlation must be GGE_TERRAIN_CORRELATION_V1")
    terrain = {}
    for row in correlation.get("rows", []):
        if row.get("source") != "persisted_saved":
            continue
        if row.get("status") != "EquivalentExplicitPackages" or len(row.get("type10_values", [])) != 1:
            raise SystemExit("saved terrain row is not an explicit equivalent-package observation")
        terrain[row.get("source_index")] = int(row["type10_values"][0], 16)
    saved = capture.get("persisted_saved", {}).get("rows", [])
    if len(terrain) != len(saved):
        raise SystemExit("terrain correlation does not cover every saved row")
    out = ["GGE_HOST_REAL_SAVE_V1", "saved_status\tAvailable", "candidate_status\tAvailable",
           "manager_status\tUnknown", "starsdb_status\tUnknown", "planet_status\tUnknown",
           "refill_status\tUnknown", "resource_root_status\tUnknown", "resource_index_status\tUnknown",
           "resource_context_status\tUnknown", "resource_class_status\tUnknown",
           "refill_append\tUnknown", f"saved_count\t{len(saved)}", "candidate_count\t0"]
    for index, row in enumerate(saved):
        star = row.get("star") or {}
        position = star.get("position")
        header = row.get("header") or {}
        planet = row.get("planet_selection") or {}
        stem = planet.get("save_stem")
        name = star.get("name")
        if not isinstance(position, list) or len(position) != 3 or not isinstance(stem, str) or not isinstance(name, str):
            raise SystemExit(f"saved row {index} lacks resolved position/name/save stem")
        if header.get("status") != "decoded_header_subset":
            raise SystemExit(f"saved row {index} lacks explicit decoded header")
        flags = int(star.get("flags", "0"), 16) if isinstance(star.get("flags"), str) else int(star.get("flags", 0))
        stage = int(star.get("tech_level", 0) or 0)
        fields = [str(int(star["key"], 16)), *[str(float(v)) for v in position], str(stage), str(flags),
                  hex_utf8(name), hex_utf8(stem), "1" if header.get("unsigned_major_ge_33") else "0",
                  str(int(header["major"])), str(int(header["minor"])), "1", str(terrain[index])]
        out.append("saved\t" + "\t".join(fields))
    out.append("END")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if args.output.exists():
        raise SystemExit("output must be new")
    args.output.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
    print(f"GGE_HOST_REAL_SAVE_V1 written; saved={len(saved)} candidates=0 native_readiness=Unknown")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
