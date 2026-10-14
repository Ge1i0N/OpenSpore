"""Resolve E3LaunchScreen drawable external entries against all DBPF index records."""
import struct
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.spui_objects import layouts, BinaryLayout

ROOT = Path(__file__).resolve().parents[1]
TYPE_PNG = 0x2F7D0004
GROUP = 0x31A44893

targets = set()
for _p, _r, _i, data in layouts(sorted((ROOT/'build/bin/Debug').rglob('*.package')), 0xE932BD44):
    q = BinaryLayout(data).parse()
    h = struct.unpack_from('<I', data, 0)[0]
    counts = q.counts
    pos = 14 + 12 * counts[0]
    # E3 external group-1 entries are instance,type,group in this binary.
    for _n in range(counts[1]):
        inst, typ, group = struct.unpack_from('<3I', data, pos)
        if typ == TYPE_PNG and group == GROUP:
            targets.add(inst)
        pos += 12
    break

found = {x: [] for x in targets}
seen = set()
for p in sorted((ROOT/'build/bin/Debug').rglob('*.package')):
    p = p.resolve()
    if p in seen: continue
    seen.add(p)
    raw = p.read_bytes()
    if raw[:4] != b'DBPF': continue
    count = struct.unpack_from('<I', raw, OFF_COUNT)[0]
    off = struct.unpack_from('<I', raw, OFF_IOFF)[0] or struct.unpack_from('<I', raw, OFF_ALT)[0]
    kind = struct.unpack_from('<I', raw, off)[0]
    prefix, stride, fld = KINDS[kind]
    shared, q = {}, 1
    for bit, nm in ((1,'type'),(2,'group'),(4,'unknown')):
        if kind & bit:
            shared[nm] = struct.unpack_from('<I', raw, off + q*4)[0]; q += 1
    for n in range(count):
        w = struct.unpack_from('<%dI' % (stride//4), raw, off+prefix+n*stride)
        typ = shared.get('type', w[fld['typeid']] if 'typeid' in fld else 0)
        group = shared.get('group', w[fld['group']] if 'group' in fld else 0)
        inst = w[fld['instance']]
        if typ == TYPE_PNG and group == GROUP and inst in found:
            size = w[fld['size']] & 0x7fffffff
            found[inst].append((str(p.relative_to(ROOT)), n, size))

print('drawable external targets: %d' % len(targets))
for inst in sorted(targets):
    print('  instance=0x%08X matches=%d %s' %
          (inst, len(found[inst]), found[inst][:4]))
