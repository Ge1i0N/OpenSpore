"""Run the pinned FLARE-FLOSS standalone extractor against the GA authority.

The FLOSS source checkout lives in refs/flare-floss and the optional verified Windows
release lives in refs/flare-floss-release. This wrapper keeps analysis reproducible:
cache is disabled and the JSON report is written to a caller-selected path.
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SAMPLE = ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe.unpacked.exe"
RELEASE_ROOT = ROOT / "refs" / "flare-floss-release"
DEFAULT_OUTPUT = ROOT / "analysis" / "strings" / "floss-ga-v3.2.0.json"


def find_floss() -> Path | None:
    candidates = sorted(RELEASE_ROOT.rglob("floss.exe")) if RELEASE_ROOT.exists() else []
    if candidates:
        return candidates[0]
    for name in ("floss", "floss.exe"):
        found = __import__("shutil").which(name)
        if found:
            return Path(found)
    return None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("sample", nargs="?", type=Path, default=DEFAULT_SAMPLE)
    ap.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT)
    args = ap.parse_args()
    floss = find_floss()
    if floss is None:
        print("FLOSS executable not found; download the verified v3.2.0 release into refs/flare-floss-release", file=sys.stderr)
        return 2
    sample = args.sample.resolve()
    output = args.output.resolve()
    if not sample.is_file():
        print(f"sample not found: {sample}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["FLOSS_CACHE_ENABLE"] = "0"
    # The GA unpacked PE is larger than FLOSS's safe default 0x1000000-byte limit.
    # --large-file is an explicit caller choice here; the wrapper still disables cache and
    # refuses to keep partial output.
    cmd = [str(floss), "--large-file", "--json", str(sample)]
    print("running:", " ".join(cmd))
    with output.open("w", encoding="utf-8", newline="\n") as report:
        result = subprocess.run(cmd, stdout=report, stderr=subprocess.PIPE, text=True, env=env)
    if result.stderr:
        sys.stderr.write(result.stderr)
    if result.returncode:
        print(f"FLOSS failed with exit code {result.returncode}; partial output is not treated as evidence", file=sys.stderr)
        output.unlink(missing_ok=True)
        return result.returncode
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
