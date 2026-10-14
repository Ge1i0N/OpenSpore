"""The designer schema's NAME -> proxy-id pairs, and whether any name carries STRUCTURE.

Correction to my first attempt: `proxy` holds the hex ID, and the human NAME is a separate `name=`
attribute on <Class>/<Property>/<Struct>/<Enum> elements. Grepping only `proxy=` therefore returned
nothing but ids - which looked like "the schema has no parent property" when in fact I had not read
the schema at all.

The question this answers: does the shipped designer define a property that links a widget to its
parent or its next sibling? If yes, its id is already present in the `spui` records we parse and the
real z-order tree is one lookup away, replacing the paint/hit heuristics. If no, the hierarchy lives in
the record stream itself and the search moves there - a negative result that is still worth recording.
"""
import glob
import io
import re

STRUCT = ("parent", "child", "children", "next", "prev", "sibling", "owner", "root", "attach",
          "tree", "node", "index", "order", "depth", "layer", "zorder", "z-order", "clip",
          "visible", "control", "group", "id")

TAG = re.compile(r"<(Class|Property|Struct|Enum|EnumVal)\b([^>]*)>", re.I | re.S)
ATTR = re.compile(r"(\w+)\s*=\s*\"([^\"]*)\"")

pairs = []
seen = set()
for f in sorted(glob.glob(r"analysis\scripts\xml\*.xml")):
    t = io.open(f, encoding="utf-8", errors="replace").read()
    for m in TAG.finditer(t):
        kind = m.group(1)
        attrs = dict(ATTR.findall(m.group(2)))
        name = attrs.get("name")
        if not name:
            continue
        proxy = attrs.get("proxy", "")
        key = (kind.lower(), name, proxy)
        if key in seen:
            continue
        seen.add(key)
        pairs.append((kind, name, proxy))

print("name->proxy pairs found: %d" % len(pairs))
kinds = {}
for k, _n, _p in pairs:
    kinds[k] = kinds.get(k, 0) + 1
print("by element: %s" % ", ".join("%s=%d" % kv for kv in sorted(kinds.items())))
print()

props = [(n, p) for k, n, p in pairs if k.lower() == "property" and p]
print("=== PROPERTIES with a proxy id: %d ===" % len(props))
struct_props = [(n, p) for n, p in props if any(s in n.lower() for s in STRUCT)]
print("--- of those, names that could carry structure: %d ---" % len(struct_props))
for n, p in struct_props:
    print("  %-32s %s" % (n, p))
print()
print("--- ALL property names (so a negative is readable, not asserted) ---")
for i in range(0, len(props), 3):
    print("  " + " | ".join("%-28s %s" % (n, p) for n, p in props[i:i + 3]))

print()
print("=== the STRUCTURAL groups, by id prefix (the schema's own names) ===")
for pref, label in (("0xeec1b", "EEC1B0xx"), ("0xeec1d", "EEC1D0xx"), ("0xeec1c", "EEC1C0xx")):
    rows = []
    for k, n, p in pairs:
        if p and p.lower().startswith(pref):
            rows.append((p, n))
    rows.sort(key=lambda t: int(t[0], 16))
    print("--- %s (%d) ---" % (label, len(rows)))
    for p, n in rows:
        print("  %-12s %s" % (p, n))
