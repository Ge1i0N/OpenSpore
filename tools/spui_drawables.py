"""Inspect drawable/WinProc references in one sequentially parsed SPUI layout.

The GA reader proves code 19 is an object-index array. This tool answers the next concrete question:
which WindowBase properties point at drawable objects, what classes do those objects have, and whether
their properties contain external resource ids we can bind to PNG/raster assets.
"""
import io
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.spui_objects import BinaryLayout, layouts

INSTANCE = 0xE932BD44
CONTROL = 0xEEC1B001
COMMAND = 0xEEC1B002
FLAGS = 0xEEC1B000
AREA = 0xEEC1B005
DRAWABLE = 0xEEC1B007
WINPROCS = 0xEEC1B00C
CHILDREN = 0xEEC1B00B


def names():
    out = {}
    for path in sorted((ROOT / "analysis/scripts/xml").glob("*.xml")):
        text = io.open(path, encoding="utf-8", errors="replace").read()
        for m in re.finditer(r"<(Class|Property)\b([^>]*)>", text):
            kind, attrs = m.group(1), m.group(2)
            pm = re.search(r'proxy="(?:[^:]*:)?(0x[0-9A-Fa-f]+)"', attrs)
            nm = re.search(r'name="([^"]*)"', attrs)
            if pm and nm:
                out[int(pm.group(1), 16)] = (kind, nm.group(1))
    return out


def prop_map(obj):
    return {p["id"]: p for p in obj["properties"]}


def main():
    path = sorted((ROOT / "build/bin/Debug").rglob("*.package"))
    vocab = names()
    rows = []
    for package, record, instance, data in layouts(path, INSTANCE):
        parser = BinaryLayout(data).parse()
        for index, obj in sorted(parser.objects.items()):
            props = prop_map(obj)
            if not any(p in props for p in (CONTROL, AREA, CHILDREN)):
                continue
            refs = []
            for pid in (DRAWABLE, WINPROCS, CHILDREN):
                p = props.get(pid)
                if p:
                    refs.extend((pid, ref) for ref in p.get("refs", []))
            rows.append((index, obj, refs))
    print("layout 0x%08X: %d window-like objects, %d objects total" %
          (INSTANCE, len(rows), len(parser.objects)))
    for index, obj, refs in rows:
        props = prop_map(obj)
        cls = vocab.get(obj["class"], ("?", "?"))[1]
        control = props.get(CONTROL, {}).get("raw", "-")
        flags = props.get(FLAGS, {}).get("raw", "-")
        area = props.get(AREA, {}).get("raw", "-")
        print("\nobject %-3d class=%-24s control=%-8s flags=%-8s area=%s" %
              (index, cls, control, flags, area))
        for pid, ref in refs:
            print("  %-12s -> object %-3d class=%s" %
                  (vocab.get(pid, ("Property", "0x%08X" % pid))[1], ref,
                   vocab.get(parser.objects.get(ref, {}).get("class", 0), ("?", "?"))[1]))
    print("\nclasses present:")
    counts = {}
    for obj in parser.objects.values():
        nm = vocab.get(obj["class"], ("?", "0x%08X" % obj["class"]))[1]
        counts[nm] = counts.get(nm, 0) + 1
    for nm, count in sorted(counts.items(), key=lambda x: (-x[1], x[0])):
        print("  %-32s %d" % (nm, count))


if __name__ == "__main__":
    main()
