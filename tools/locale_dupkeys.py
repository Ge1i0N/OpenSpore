"""Keys defined TWICE inside a single `locale` resource.

The engine's precedence report claimed table 0xEA78CFB9 key 0x0773735A was overridden by the SAME
container that defined it. tools/nested_duptable.py ruled out a repeated instance inside a package
(65 locale resources, 65 distinct instances). The remaining shape is the one a dict-based reader
cannot see: the same `0x........` key line appearing twice in ONE locale text, with different values.

Our parser pushes one Entry per key line, so it sees both; a reader that keys a table by
{key: text} collapses them and never knows. That difference is exactly what decides which string a
menu shows, so measure it: how many resources, how many keys, and what the two texts say.
"""
import sys, struct, re
from collections import defaultdict, Counter
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path

PKG_TYPE, LOCALE_TYPE = 0x06EFC6AA, 0x02FAC0B6
EN = 0x7080DB55
root = Path(r"E:\_DPT_Projects_E\_Harness\OpenSpore\build\bin\Debug")


def index(raw):
    h = raw[:HDR]
    count = struct.unpack("<I", h[OFF_COUNT:OFF_COUNT + 4])[0]
    use = struct.unpack("<I", h[OFF_IOFF:OFF_IOFF + 4])[0] or struct.unpack(
        "<I", h[OFF_ALT:OFF_ALT + 4])[0]
    kind = struct.unpack("<I", raw[use:use + 4])[0]
    if kind not in KINDS:
        return
    prefix, stride, fld = KINDS[kind]
    pw = struct.unpack("<%dI" % (prefix // 4), raw[use:use + prefix])
    shared, q = {}, 1
    for bit, nm in ((1, "type"), (2, "group"), (4, "unknown")):
        if kind & bit:
            shared[nm] = pw[q]
            q += 1
    for r in range(count):
        w = struct.unpack("<%dI" % (stride // 4), raw[use + prefix + r * stride:][:stride])
        yield (shared.get("type", w[fld["typeid"]] if "typeid" in fld else 0),
               w[fld["instance"]] if "instance" in fld else 0, (raw, w, fld))


def payload(meta):
    raw, w, fld = meta
    off = w[fld["offset"]]
    d = raw[off:off + (w[fld["size"]] & 0x7FFFFFFF)]
    if (w[fld["sentinel"]] & 0xFFFF) == 0xFFFF:
        try:
            d = decode(d, w[fld["usize"]])
        except Exception:
            pass
    return d


def key_lines(b):
    """[(key, text)] in FILE ORDER, repeats included - the point of this tool."""
    if b[:3] == b"\xef\xbb\xbf":
        b = b[3:]
    out, cur = [], None
    for line in b.decode("utf-8", "replace").replace("\r\n", "\n").split("\n"):
        s = line.lstrip(" \t")
        if not s:
            cur = None
            continue
        if s.startswith("#"):
            continue
        m = re.match(r"^0x([0-9A-Fa-f]{8})[\t ]*(.*)$", s)
        if m:
            cur = int(m.group(1), 16)
            out.append([cur, m.group(2).strip()])
        elif cur is not None and out:
            out[-1][1] += "\n" + s.strip()
    return out


stats = Counter()
examples = []
for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        raw = p.read_bytes()
        if raw[:4] != b"DBPF":
            continue
        todo = [(p.name, raw)]
        while todo:
            label, img = todo.pop()
            for t, i, meta in index(img):
                # Check the TYPE before decoding. Decoding every resource of all 57 containers is
                # what made the first version of this tool time out at 15 minutes.
                if t == LOCALE_TYPE:
                    d = payload(meta)
                    stats["resources"] += 1
                    kl = key_lines(d)
                    stats["key lines"] += len(kl)
                    seen, dup = {}, 0
                    for k, txt in kl:
                        if k in seen:
                            dup += 1
                            if seen[k] != txt and len(examples) < 12:
                                examples.append((label, i, k, seen[k], txt))
                        else:
                            seen[k] = txt
                    if dup:
                        stats["resources with a repeated key"] += 1
                        stats["repeated key occurrences"] += dup
                        stats["repeats with DIFFERENT text"] += sum(
                            1 for k, txt in kl if seen.get(k) != txt)
                elif t == PKG_TYPE and len(d) >= HDR and d[:4] == b"DBPF":
                    if i == EN or "Locale" in label:
                        todo.append(("%s#nested(0x%08X)" % (label, i), d))

print("scanned: %d locale resources, %d key lines" % (stats["resources"], stats["key lines"]))
print("resources containing a repeated key : %d" % stats["resources with a repeated key"])
print("repeated key occurrences            : %d" % stats["repeated key occurrences"])
print("of those, DIFFERENT text on the repeat: %d" % stats["repeats with DIFFERENT text"])
print()
print("=== the repeats (this is what a dict-keyed reader never sees) ===")
for label, inst, k, first, second in examples:
    print("  %s / table 0x%08X key 0x%08X" % (label, inst, k))
    print("     1st: %r" % (first[:74],))
    print("     2nd: %r" % (second[:74],))
