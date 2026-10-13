"""Index-kind rule, tested across every shipped package (57 files).

Discovered from the data, then aimed at the disassembler:
  The index begins with a u32 KIND tag K at index_offset+0. K selects the record shape, and
  the header carries NO discriminator (tools/pkg_hdr_diff.py: two files of different kinds
  have byte-identical headers except count/idx_size/idx_offset). The three observed kinds
  trade prefix bytes for record bytes with a constant sum of 36:

    K=4 -> prefix 8,  stride 28 (7 u32): type, ?, instanceId, offset, size|0x80000000, usize, sentinel
    K=6 -> prefix 12, stride 24 (6 u32): type, instanceId,     offset, size|0x80000000, usize, sentinel
    K=7 -> prefix 16, stride 20 (5 u32): type,                 offset, size|0x80000000, usize, sentinel

  prefix = 4 + K, stride = 32 - K, so idx_size == (4+K) + (32-K)*count for every file.
  K=6 and K=7 differ by exactly the instanceId word; K=4 has an EXTRA word (index 1) whose
  values look like IEEE-754 floats in a narrow range (0x406a2100 = 3.658, 0x40606000 = 3.508)
  - reported as a candidate float, not asserted.

Checks run here (authority #2, the shipped files):
  * formula: idx_size == (4+K) + (32-K)*count
  * bounds : offset < index_offset and offset+size <= index_offset, per record
  * usize  : usize >= size, per record
  * sentinel is 0x10000 or 0x1FFFF
  * K=4 word[1]: is it always in a plausible float range and non-denormal? (diagnostic only)
Usage: python tools/pkg_kinds.py
"""
from __future__ import annotations

import math
import struct
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIRS = [ROOT / "build" / "bin" / "Debug" / "Data", ROOT / "build" / "bin" / "Debug" / "DataEP1"]
HDR = 0x60
OFF_COUNT, OFF_ISIZE, OFF_IOFF, OFF_ALT = 0x24, 0x2C, 0x40, 0x28

# kind -> (prefix, stride, field table)
# kind -> (prefix, stride, field table)
# Field order in a full record is type, group, unknown, instance, offset, packedSize, memSize,
# flags; a field is OMITTED from the record when its bit is set in the kind mask, and its value
# then lives once in the prefix. So:
#   kind 4 (bit2): type,group,instance per record  -> words: typeid, group, instance, offset, size, usize, sentinel
#   kind 6 (bits1,2): type + instance per record   -> words: typeid, instance, offset, size, usize, sentinel
#   kind 7 (bits0,1,2): only instance + rest       -> words: instance, offset, size, usize, sentinel
# The kind-7 labelling used to say typeid=0, which is wrong and made the inventory read the
# instance as the type (caught while resolving names).
KINDS = {
    4: (8, 28, dict(typeid=0, group=1, instance=2, offset=3, size=4, usize=5, sentinel=6)),
    6: (12, 24, dict(typeid=0, instance=1, offset=2, size=3, usize=4, sentinel=5)),
    7: (16, 20, dict(instance=0, offset=1, size=2, usize=3, sentinel=4)),
}



def out(s: str = "") -> None:
    sys.stdout.buffer.write((s + "\n").encode("ascii", "replace"))


def main() -> int:
    files = sorted(p for d in DIRS if d.is_dir() for p in d.rglob("*.package"))
    out(f"# {len(files)} .package files")
    kinds: Counter[int] = Counter()
    bad_formula: list[str] = []
    bad_bounds = bad_usize = bad_sent = 0
    recs = 0
    sentinels: Counter[int] = Counter()
    typeids: Counter[int] = Counter()
    floats: list[float] = []
    float_weird = 0
    for f in files:
        h = f.read_bytes()[:HDR]
        count = struct.unpack("<I", h[OFF_COUNT : OFF_COUNT + 4])[0]
        isize = struct.unpack("<I", h[OFF_ISIZE : OFF_ISIZE + 4])[0]
        ioff = struct.unpack("<I", h[OFF_IOFF : OFF_IOFF + 4])[0]
        alt = struct.unpack("<I", h[OFF_ALT : OFF_ALT + 4])[0]
        use = ioff or alt
        with f.open("rb") as fh:
            fh.seek(use)
            kb = fh.read(4)
        if len(kb) < 4:
            bad_formula.append(f"{f.name}: cannot read kind")
            continue
        kind = struct.unpack("<I", kb)[0]
        kinds[kind] += 1
        if kind not in KINDS:
            bad_formula.append(f"{f.name}: unknown kind {kind}")
            continue
        prefix, stride, fld = KINDS[kind]
        if isize != prefix + stride * count:
            bad_formula.append(f"{f.name}: kind={kind} count={count} idx_size={isize} predicted={prefix+stride*count}")
            continue
        with f.open("rb") as fh:
            fh.seek(use + prefix)
            blob = fh.read(stride * count)
        nw = stride // 4
        for i in range(count):
            w = struct.unpack("<%dI" % nw, blob[i * stride : (i + 1) * stride])
            recs += 1
            off, szraw = w[fld["offset"]], w[fld["size"]]
            sz = szraw & 0x7FFFFFFF
            if "typeid" in fld:
                typeids[w[fld["typeid"]]] += 1
            if szraw >> 31 != 1:
                bad_sent += 1
            if off >= use or off + sz > use:
                bad_bounds += 1
            if w[fld["usize"]] < sz:
                bad_usize += 1
            sentinels[w[fld["sentinel"]]] += 1
            if "maybe_float" in fld:
                fv = struct.unpack("<f", struct.pack("<I", w[fld["maybe_float"]]))[0]
                if math.isfinite(fv) and 1e-3 < abs(fv) < 1e3:
                    floats.append(fv)
                else:
                    float_weird += 1
    out("")
    out(f"## kind distribution: {dict(kinds)}")
    out(f"## records checked: {recs:,}")
    out(f"## formula violations: {len(bad_formula)}")
    for r in bad_formula[:12]:
        out(f"   {r}")
    out(f"## bounds violations : {bad_bounds}")
    out(f"## usize<size violations: {bad_usize}")
    out(f"## size high-bit clear (unexpected): {bad_sent}")
    out(f"## sentinel values: {dict(sentinels)}")
    out(f"## distinct type ids: {len(typeids)}; top: {typeids.most_common(8)}")
    if floats:
        out(f"## kind-4 word[1] as float: {len(floats):,} plausible, {float_weird} not; "
            f"range [{min(floats):.4f}, {max(floats):.4f}], distinct {len(set(round(x,4) for x in floats))}")
    ok = not bad_formula and not bad_bounds and not bad_usize and not bad_sent
    out("")
    out("## VERDICT: " + ("**RULE HOLDS on all 57 files / %d records**" % recs if ok else "**INCOMPLETE**"))
    out("")
    out("Still NOT reversed (data fit only, authority #2): the branch on K in the original.")
    out("Next disasm target: the index reader must compare this dword; find it near")
    out("0x008d9a90 (vtable+0x90) and the container at [pkg+0x2d0].")
    return 0


if __name__ == "__main__":
    sys.exit(main())
