"""Check local inline Markdown links in maintained docs; no network, no archive rewriting.

Original upstream references and historical snapshots are deliberately excluded. Code-fenced
examples, external URLs and fragment-only links are not local file references. Anchor spelling
is not validated: GitHub/Unicode heading slug rules are a separate concern.
"""
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
LINK = re.compile(r"!?\[[^\]\n]*\]\((<[^>]+>|[^)\n]+)\)")


def missing_links(path: Path):
    display = path.relative_to(ROOT) if path.is_relative_to(ROOT) else path
    fenced = False
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if line.lstrip().startswith(("```", "~~~")):
            fenced = not fenced
            continue
        if fenced:
            continue
        for match in LINK.finditer(line):
            target = match.group(1).strip()
            if target.startswith("<"):
                target = target[1:-1]
            else:
                target = re.split(r'\s+["\']', target, maxsplit=1)[0]
            parsed = urlsplit(target)
            if parsed.scheme or not parsed.path or target.startswith("//"):
                continue
            candidate = (ROOT if parsed.path.startswith("/") else path.parent) / unquote(parsed.path).lstrip("/")
            if not candidate.exists():
                yield f"{display}:{number}: missing {target}"


def main():
    paths = sorted(set(ROOT.glob("*.md")) | set((ROOT/"analysis").rglob("*.md")))
    failures = [failure for path in paths for failure in missing_links(path)]
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"{len(paths)} maintained Markdown files: local inline file links OK (anchors/network/archives excluded)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
