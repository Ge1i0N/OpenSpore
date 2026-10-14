"""Dump SPUI external table entries for a resource, preserving raw fields.

GA 0x82ce10 reads three external groups before class ids and object blocks. E3LaunchScreen has
counts 0/40/0; this tool prints those 40 twelve-byte entries and compares their words with the
object reference indices used by ImageDrawable. It intentionally does not label fields until the
binary reader's consumers confirm the semantics.
"""
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.spui_objects import layouts, BinaryLayout

for package, record, instance, data in layouts(
        sorted((Path(__file__).resolve().parents[1] / 'build/bin/Debug').rglob('*.package')),
        0xE932BD44):
    print('%s rec[%d] 0x%08X: %d bytes' % (package.name, record, instance, len(data)))
    if len(data) < 28:
        continue
    magic, ver, c0, c1, c2, objects = struct.unpack_from('<IHHHHH', data, 0)
    print('header magic=0x%08X version=%d external_counts=%d,%d,%d object_count=%d' %
          (magic, ver, c0, c1, c2, objects))
    pos = 4 + 2 + 2*4
    for group, count in enumerate((c0, c1, c2)):
        print('group %d count=%d' % (group, count))
        for n in range(count):
            raw = data[pos:pos+12]
            words = struct.unpack_from('<3I', raw)
            print('  [%02d] off=%04X raw=%s words=%s' %
                  (n, pos, raw.hex(' '), ' '.join('0x%08X' % x for x in words)))
            pos += 12
    print('class ids start offset=0x%X' % pos)
    q = BinaryLayout(data).parse()
    print('parsed objects=%d windows=%d' % (len(q.objects), len(q.windows)))
    print('drawable external-reference properties:')
    for oi, obj in sorted(q.objects.items()):
        for p in obj['properties']:
            if p['code'] == 19 and p.get('refs') and oi in (43, 46, 49, 52, 55, 58, 64):
                print('  object %d property 0x%08X refs=%s' % (oi, p['id'], p['refs']))
    print('external refs used by drawable image objects:')
    external = []
    for n in range(c0 + c1 + c2):
        external.append(struct.unpack_from('<3I', data, 10 + n * 12))
    used = sorted({ref for oi, obj in q.objects.items() for p in obj['properties']
                   if p['code'] == 19 and p.get('refs') and oi in (43, 46, 49, 52, 55, 58, 64)
                   for ref in p['refs']})
    for ref in used:
        if ref < len(external):
            inst, typ, group = external[ref]
            print('  ref[%d]: instance=0x%08X type=0x%08X group=0x%08X' % (ref, inst, typ, group))
    if package.name == 'PatchData.package':
        break
