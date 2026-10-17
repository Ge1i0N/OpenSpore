"""Find the bounded GGE message/control IDs in shipped SPUI windows.

The probe reads only the caller-selected shipped package root (default build/bin/Debug), parses
known SPUI layouts and reports exact ControlID/CommandID matches. It performs no profile search,
priority merge, writes or native message execution. Package copies remain separate observations.
"""
from __future__ import annotations

import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.spui_objects import BinaryLayout, layouts

DEFAULT_IDS = (0x062D88E9, 0x061DD69B, 0x0615B966, 0x070D74E7, 0x070D7509,
               0x0615B616, 0x07BFB76E)


def probe(root: Path, wanted: set[int]) -> dict:
    matches = {value: [] for value in sorted(wanted)}
    parsed = failed = skipped = 0
    packages = sorted(root.rglob("*.package"))
    for package in packages:
        try:
            records = layouts([package], None)
            for _, ordinal, instance, data in records:
                try:
                    reader = BinaryLayout(data).parse()
                except Exception:
                    failed += 1
                    continue
                parsed += 1
                for object_index, window in reader.windows.items():
                    properties = {p["id"]: p for p in window["properties"]}
                    for property_id, label in ((0xEEC1B001, "ControlID"),
                                               (0xEEC1B002, "CommandID")):
                        prop = properties.get(property_id)
                        raw = prop.get("raw", "") if prop else ""
                        if len(raw) < 8:
                            continue
                        value = struct.unpack("<I", bytes.fromhex(raw[:8]))[0]
                        if value in matches:
                            command_prop = properties.get(0xEEC1B002)
                            command_raw = command_prop.get("raw", "") if command_prop else ""
                            command = (struct.unpack("<I", bytes.fromhex(command_raw[:8]))[0]
                                       if len(command_raw) >= 8 else None)
                            caption = properties.get(0xEEC1B00A)
                            matches[value].append({"package": package.name,
                                                   "record": ordinal,
                                                   "instance": f"0x{instance:08x}",
                                                   "object": object_index,
                                                   "property": label,
                                                   "command_id": None if command is None else f"0x{command:08x}",
                                                   "parent": reader.parents.get(object_index),
                                                   "children": reader.children.get(object_index, []),
                                                   "caption": (caption.get("strings") if caption else None)})
        except Exception:
            skipped += 1
    return {"format": "GGE_CONTROL_ID_CORRELATION_V1",
            "root": str(root), "packages": len(packages), "parsed_layouts": parsed,
            "failed_layouts": failed, "skipped_packages": skipped,
            "ids": {f"0x{value:08x}": rows for value, rows in matches.items()},
            "limitations": ["shipped SPUI data only", "no runtime message execution",
                            "no package priority merge", "ControlID match is not proof of producer"]}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path("build/bin/Debug"))
    parser.add_argument("--id", dest="ids", action="append", type=lambda value: int(value, 0))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = probe(args.root.resolve(), set(args.ids or DEFAULT_IDS))
    text = json.dumps(result, indent=2, ensure_ascii=False) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8", newline="\n")
    else:
        print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
