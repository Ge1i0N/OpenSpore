"""Compare a bounded MiniWiki command lead list with GA plaintext string presence.

String presence is only a lead. This tool deliberately reports no command as implemented from a
matching string alone; native registration/callback evidence remains separate.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
SOURCES = [
    ROOT / "refs" / "MiniWiki" / "ConsoleCommands.md",
    ROOT / "refs" / "MiniWiki" / "OldBetaResearch" / "Cheats_2.txt",
]
OUT = ROOT / "analysis" / "audit" / "miniwiki-command-compare.json"


def main() -> int:
    from tools.pe import PE
    from tools.strings_scan import all_strings

    text = "\n".join(p.read_text(encoding="utf-8", errors="replace") for p in SOURCES)
    # Only take command-shaped tokens: Markdown bold command names and command/list entries.
    candidates = set(re.findall(r"\*\*([A-Za-z][A-Za-z0-9_]*)\*\*", text))
    candidates.update(re.findall(r"(?m)^\s*-([A-Za-z][A-Za-z0-9_]*)\b", text))
    candidates.update(re.findall(r"(?m)^\s*([A-Za-z][A-Za-z0-9_]*)\s+-", text))
    candidates = sorted(candidates, key=str.casefold)

    pe = PE()
    native: set[str] = set()
    locations: dict[str, list[str]] = {}
    for section in (".rdata", ".data", "CONST"):
        if section not in pe.sections:
            continue
        rows = all_strings(pe.section_bytes(section), pe.sections[section]["va"], 4)
        for va, _kind, value in rows:
            folded = value.casefold()
            native.add(folded)
            if folded in {c.casefold() for c in candidates}:
                locations.setdefault(folded, []).append(hex(va))

    confirmed_registration = {"help", "history", "clear", "levels", "unlockadventures"}
    report = {
        "authority": "historical MiniWiki leads compared with GA plaintext string pool",
        "sources": [str(p.relative_to(ROOT)) for p in SOURCES],
        "candidate_count": len(candidates),
        "string_present": [c for c in candidates if c.casefold() in native],
        "string_absent": [c for c in candidates if c.casefold() not in native],
        "observed_native_registration": sorted(confirmed_registration),
        "locations": locations,
        "limitations": [
            "A GA string match is not command registration or runtime execution evidence.",
            "MiniWiki mixes retail/wiki and old Beta/debug material.",
            "The candidate extractor is bounded to command-shaped tokens and may omit aliases.",
        ],
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps({k: report[k] for k in ("candidate_count", "string_present", "string_absent", "observed_native_registration")}, indent=2, ensure_ascii=False))
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
