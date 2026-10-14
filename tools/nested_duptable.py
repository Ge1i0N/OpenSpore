"""Does a single nested locale package contain the SAME table instance more than once?

The engine's precedence report printed one impossible-looking line:

    tbl 0xEA78CFB9 key 0x0773735A: Spore_EP1_Locale_01.package#nested WINS over
                                    Spore_EP1_Locale_01.package#nested

Same container on both sides, different text. That is not a patch-over-base duplicate; it means ONE
DBPF holds two `locale` resources with the same instance id, disagreeing. If that is real, then
"instance id identifies a table" is false inside a container, and any reader that keys a table map by
instance alone silently drops one of them - which is also a hint about how the ORIGINAL resolves it
(a resource cache keyed by (type,group,instance) can only keep one, so the game has an answer here
whether it wants one or not).
"""
import sys, struct, re
sys.path.insert(0, r"E:\_DPT_Projects_E\_Harness\OpenSpore")
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode
from pathlib import Path
from collections import defaultdict

PKG_TYPE, LOCALE_TYPE = 0x06EFC6AA, 0x02FAC0B6
EN, HOT = 0x7080DB55, 0xEA78CFB9
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
               shared.get("group", w[fld["group"]] if "group" in fld else 0),
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


def keys_of(b):
    if b[:3] == b"\xef\xbb\xbf":
        b = b[3:]
    out, cur = {}, None
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
            out[cur] = m.group(2).strip()
        elif cur is not None:
            out[cur] += "\n" + s.strip()
    return out


for pat in ("*.package", "*.pck"):
    for p in sorted(root.rglob(pat)):
        if "PatchData" not in p.name and "EP1_Locale" not in p.name:
            continue
        raw = p.read_bytes()
        if raw[:4] != b"DBPF":
            continue
        for t, g, i, meta in index(raw):
            if t != PKG_TYPE or i != EN:
                continue
            d = payload(meta)
            if len(d) < HDR or d[:4] != b"DBPF":
                continue
            seen = defaultdict(list)
            for t2, g2, i2, m2 in index(d):
                if t2 == LOCALE_TYPE:
                    seen[i2].append((g2, payload(m2)))
            dups = {k: v for k, v in seen.items() if len(v) > 1}
            print("%s / nested 0x%08X: %d locale resources, %d distinct instances, %d repeated"
                  % (p.name, i, sum(len(v) for v in seen.values()), len(seen), len(dups)))
            for k, v in dups.items():
                print("   instance 0x%08X appears %d times (groups %s)"
                      % (k, len(v), ", ".join("0x%08X" % gg for gg, _ in v)))
                parsed = [keys_of(b) for _gg, b in v]
                common = set(parsed[0]) & set(parsed[1])
                diff = [x for x in common if parsed[0][x] != parsed[1][x]]
                print("      keys: %d vs %d, %d common, %d disagree"
                      % (len(parsed[0]), len(parsed[1]), len(common), len(diff)))
                for x in diff[:6]:
                    print("      key 0x%08X" % x)
                    print("         1st: %r" % (parsed[0][x][:70],))
                    print("         2nd: %r" % (parsed[1][x][:70],))
                if k == HOT and HOT not in dups:
                    pass
            if HOT in seen:
                print("   >> the reported table 0x%08X is present %d time(s) here"
                      % (HOT, len(seen[HOT])))
                for gg, b in seen[HOT]:
                    kk = keys_of(b)
                    print("      group 0x%08X: %d keys, 0x0773735A = %r"
                          % (gg, len(kk), kk.get(0x0773735A, "<absent>")[:70]))
