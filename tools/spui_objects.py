"""Probe SPUI object framing and code-19 references; no inferred tree is installed here."""
import argparse
from collections import Counter
import json
import hashlib
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from tools.pkg_kinds import HDR, KINDS, OFF_ALT, OFF_COUNT, OFF_IOFF
from tools.refpack import decode


def layouts(paths, instance=None):
    """Read indexes first, then decode only requested SPUI resources. Fail on decode errors."""
    seen = set()
    for path in paths:
        path = Path(path).resolve()
        if path in seen:
            continue
        seen.add(path)
        with path.open('rb') as f:
            hdr = f.read(HDR)
            if hdr[:4] != b'DBPF':
                continue
            count = struct.unpack_from('<I', hdr, OFF_COUNT)[0]
            off = (struct.unpack_from('<I', hdr, OFF_IOFF)[0]
                   or struct.unpack_from('<I', hdr, OFF_ALT)[0])
            f.seek(off)
            kind = struct.unpack('<I', f.read(4))[0]
            prefix, stride, fields = KINDS[kind]
            f.seek(off)
            index = f.read(prefix + count * stride)
            shared, q = {}, 1
            for bit, key in ((1, 'type'), (2, 'group'), (4, 'unknown')):
                if kind & bit:
                    shared[key] = struct.unpack_from('<I', index, q * 4)[0]
                    q += 1
            for n in range(count):
                w = struct.unpack_from('<%dI' % (stride // 4), index, prefix + n * stride)
                typ = shared['type'] if 'type' in shared else w[fields['typeid']]
                inst = w[fields['instance']]
                if typ != 0x250FE9A2 or (instance is not None and inst != instance):
                    continue
                f.seek(w[fields['offset']])
                data = f.read(w[fields['size']] & 0x7fffffff)
                if w[fields['sentinel']] & 0xffff == 0xffff:
                    data = decode(data, w[fields['usize']])
                yield path, n, inst, data


def probe(data):
    print('header:', data[:64].hex(' '))
    print('class/object markers (first 16):')
    pos, markers = 0, []
    while True:
        pos = data.find(b'\xf5\x5f', pos)
        if pos < 0:
            break
        markers.append(pos)
        pos += 2
    for off in markers[:16]:
        print('  @%5d before=%s after=%s' %
              (off, data[max(0, off-8):off].hex(' '), data[off:off+16].hex(' ')))
    print('markers total:', len(markers))
    for pid in (0xeec1b00b, 0xeec1b007):
        pos, rows = 0, []
        while True:
            pos = data.find(struct.pack('<I', pid), pos)
            if pos < 0 or pos + 8 > len(data):
                break
            code, count = struct.unpack_from('<HH', data, pos+4)
            if code == 19 and pos+8+2*count <= len(data):
                values = struct.unpack_from('<%dH' % count, data, pos+8)
                rows.append((pos, count, values))
            pos += 4
        print('  property 0x%08X: %d records; first 16 (offset, header count, u16 values)' % (pid, len(rows)))
        for row in rows[:16]:
            print('   ', row)


class FormatError(ValueError):
    pass


class BinaryLayout:
    """Bounded sequential reader, from GA 0x82ce10 / 0x82b3f0 / 0x82b240.

    Names are optional metadata. Unknown property ids are preserved and do not change framing.
    """
    WIDTHS = {2: 1, 3: 1, 4: 2, 5: 4, 6: 8, 7: 1, 8: 2, 9: 4,
              10: 8, 11: 4, 12: 8, 14: 16, 15: 8, 16: 16, 17: 8}

    def __init__(self, data):
        self.data, self.pos = data, 0
        self.objects = {}
        self.references = []
        self.codes = Counter()

    def take(self, count):
        if count < 0 or count > len(self.data)-self.pos:
            raise FormatError('read %d bytes at %d exceeds %d' % (count, self.pos, len(self.data)))
        pos = self.pos
        self.pos += count
        return self.data[pos:self.pos]

    def integer(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))[0]

    def property(self, depth=0):
        if depth > 64:
            raise FormatError('nested property depth exceeds 64')
        off = self.pos
        pid = self.integer('<I')
        code, count = self.integer('<H'), self.integer('<H')
        p = {'offset': off, 'id': pid, 'code': code, 'count': count}
        self.codes[code] += 1
        if code in self.WIDTHS:
            p['raw'] = self.take(self.WIDTHS[code]*count).hex()
        elif code == 19:
            refs = [self.integer('<H') for _ in range(count)]
            self.references.extend((off, x) for x in refs)
            p['refs'] = refs
        elif code == 18:
            values = []
            for _ in range(count):
                chars = self.integer('<H')
                if chars == 0xffff:
                    values.append({'table': self.integer('<I'), 'key': self.integer('<I')})
                else:
                    values.append(self.take(2*chars).decode('utf-16-le', 'strict'))
            p['strings'] = values
        elif code == 20:
            p['struct_tag'] = self.integer('<I')
            values = []
            for _ in range(count):
                nprops = self.integer('<H')
                if nprops > 200:
                    raise FormatError('too many struct properties at %d' % self.pos)
                values.append([self.property(depth+1) for _ in range(nprops)])
            p['structs'] = values
        else:
            raise FormatError('unsupported code %d at %d (id=0x%08X)' % (code, off, pid))
        p['end'] = self.pos
        return p

    def parse(self):
        if self.integer('<I') != 0xe3fe3fb8:
            raise FormatError('wrong magic')
        self.version = self.integer('<H')
        if self.version not in (2, 3):
            raise FormatError('unsupported version %d' % self.version)
        self.counts = [self.integer('<H') for _ in range(4)]
        self.total = sum(self.counts)
        if self.total > 6000:
            raise FormatError('more than 6000 resources/objects')
        self.external = [tuple(self.integer('<I') for _ in range(3))
                         for _ in range(self.counts[0] + self.counts[1])]
        for _ in range(self.counts[2]):
            kind = self.integer('<B') if self.version >= 3 else 0
            if kind == 0:
                self.take(12)
            elif kind == 1:
                self.take(8)
                runs = self.integer('<I')
                self.take(2*runs)
            else:
                raise FormatError('unsupported hitmask kind %d' % kind)
        self.first_object = self.total-self.counts[3]
        self.classes = [self.integer('<I') for _ in range(self.counts[3])]
        while True:
            off = self.pos
            if self.integer('<H') != 0x5ff5:
                raise FormatError('expected object marker at %d' % off)
            index = self.integer('<H')
            if index == 0xffff:
                if self.integer('<I') != 0x1c01c047:
                    raise FormatError('wrong end magic')
                if self.pos != len(self.data):
                    raise FormatError('trailing bytes at %d' % self.pos)
                break
            if index < self.first_object or index >= self.total or index in self.objects:
                raise FormatError('invalid/repeated object %d at %d' % (index, off))
            flags = self.integer('<H')
            count = flags & 0x7fff
            if count > 200:
                raise FormatError('too many object properties at %d' % off)
            props = [self.property() for _ in range(count)]
            self.objects[index] = {'index': index, 'offset': off, 'end': self.pos,
                                   'class': self.classes[index-self.first_object],
                                   'root': bool(flags & 0x8000), 'properties': props}
        for off, ref in self.references:
            if ref != 0xffff and ref >= self.total:
                raise FormatError('reference %d out of range at %d' % (ref, off))
        self.children = {}
        for index, obj in self.objects.items():
            for p in obj['properties']:
                if p['id'] == 0xeec1b00b:
                    if p['code'] != 19:
                        raise FormatError('Children has non-reference type')
                    refs = [x for x in p['refs'] if x != 0xffff]
                    if any(x not in self.objects for x in refs):
                        raise FormatError('Children references non-object')
                    self.children[index] = refs
        self.windows = {i: o for i, o in self.objects.items()
                        if any(p['id'] in (0xeec1b000, 0xeec1b001, 0xeec1b005, 0xeec1b00b)
                               for p in o['properties'])}
        self.parents = {}
        for parent, children in self.children.items():
            for child in children:
                if child not in self.windows or child in self.parents:
                    raise FormatError('non-window or repeated parent for child %d' % child)
                self.parents[child] = parent
        for child in self.parents:
            seen = set()
            cur = child
            while cur in self.parents:
                if cur in seen:
                    raise FormatError('cyclic child graph')
                seen.add(cur)
                cur = self.parents[cur]
        return self

    def summary(self):
        return {'version': self.version, 'counts': self.counts, 'bytes': len(self.data),
                'sha256': hashlib.sha256(self.data).hexdigest(),
                'external': self.external,
                'consumed': self.pos, 'objects': len(self.objects),
                'roots': [i for i, o in self.objects.items() if o['root']],
                'properties': sum(self.codes.values()), 'codes': dict(self.codes),
                'child_links': sum(map(len, self.children.values())),
                'windows': len(self.windows),
                'window_roots': [i for i in self.windows if i not in self.parents]}


def main():
    ap = argparse.ArgumentParser(__doc__)
    ap.add_argument('--instance', type=lambda s: int(s, 0), default=0xe932bd44)
    ap.add_argument('--all', action='store_true')
    ap.add_argument('--parse', action='store_true')
    ap.add_argument('--output', type=Path)
    ap.add_argument('--extract', type=Path, help='decoded fixtures plus manifest for the C++ verifier')
    args = ap.parse_args()
    paths = sorted((ROOT/'build/bin/Debug').rglob('*.package'))
    results = []
    manifest = []
    if args.extract:
        args.extract.mkdir(parents=True, exist_ok=True)
    for path, rec, inst, data in layouts(paths, None if args.all else args.instance):
        row = {'package': path.name, 'record': rec, 'instance': inst}
        if args.parse:
            reader = BinaryLayout(data)
            try:
                reader.parse()
                row.update(reader.summary(), ok=True)
                if args.extract:
                    name = '%03d_%08x.spui' % (len(results), inst)
                    (args.extract/name).write_bytes(data)
                    manifest.append('%s %d %d %d %d %d %d' %
                                    (name, len(data), len(reader.objects), len(reader.windows),
                                     sum(reader.codes.values()), row['child_links'],
                                     len(row['window_roots'])))
                if not args.all:
                    print(json.dumps(row, indent=2))
                    print('Children object links:', reader.children)
                    for i, o in reader.windows.items():
                        props = {p['id']: p for p in o['properties']}
                        area = props.get(0xeec1b005, {}).get('raw')
                        control = props.get(0xeec1b001, {}).get('raw')
                        flags = props.get(0xeec1b000, {}).get('raw', '')
                        print('  window %d parent=%s control=%s flags=%s area=%s' %
                              (i, reader.parents.get(i), control, flags,
                               struct.unpack('<4f', bytes.fromhex(area)) if area else None))
            except (FormatError, UnicodeError) as exc:
                row.update(ok=False, error=str(exc), offset=reader.pos)
                print(json.dumps(row))
            results.append(row)
        else:
            print('%s rec[%d] instance=0x%08X %d bytes' % (path.name, rec, inst, len(data)))
            probe(data)
    if args.parse:
        print('Sequential parse: %d/%d PASS' % (sum(x['ok'] for x in results), len(results)))
        if args.output:
            args.output.write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
        if args.extract:
            (args.extract/'manifest.txt').write_text('\n'.join(manifest)+'\n', encoding='utf-8')
        if any(not x['ok'] for x in results):
            return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
