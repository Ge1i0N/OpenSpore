"""Read one exact GA camera prop from an explicitly selected DBPF, without loading a huge package.

No package priority, inherited property lookup, runtime camera creation or save access.
"""
import hashlib
import math
from pathlib import Path
import struct

from prop_table import parse
from refpack import decode

KEY = (0x780786FD, 0x00B1B104, 0x40410100)  # instance,type,group; GA selection and shipped record


def read_camera(path):
    with Path(path).open('rb') as f:
        size = f.seek(0, 2)
        f.seek(0)
        h = f.read(96)
        if len(h) != 96 or h[:4] != b'DBPF':
            raise ValueError('short/not-DBPF camera package')
        word = lambda at: struct.unpack_from('<I', h, at)[0]
        if (word(4), word(8), word(0x3c)) != (3, 0, 3):
            raise ValueError('unsupported camera DBPF version/index')
        count, index_size = word(0x24), word(0x2c)
        index_at = word(0x40) or word(0x28)
        if count > 262144 or index_at < 96 or index_size < 4 or index_size > 16 * 1024 * 1024 or index_at + index_size > size:
            raise ValueError('camera DBPF index bounds/host limit')
        f.seek(index_at)
        index = f.read(index_size)
        if len(index) != index_size:
            raise ValueError('short camera DBPF index')
        mask, = struct.unpack_from('<I', index)
        if mask > 7:
            raise ValueError('unsupported camera DBPF shared mask')
        prefix, stride = 4 + 4 * mask.bit_count(), 32 - 4 * mask.bit_count()
        if index_size != prefix + count * stride:
            raise ValueError('camera DBPF index count/stride')
        shared, at = {}, 4
        for bit, name in ((1, 'type'), (2, 'group'), (4, 'high')):
            if mask & bit:
                shared[name], = struct.unpack_from('<I', index, at)
                at += 4
        matches = []
        for ordinal in range(count):
            at = prefix + ordinal * stride
            values = dict(shared)
            for name in ('type', 'group', 'high', 'instance', 'offset', 'packed', 'memory', 'codec_flags'):
                if name not in shared:
                    values[name], = struct.unpack_from('<I', index, at)
                    at += 4
            if (values['instance'], values['type'], values['group']) == KEY:
                matches.append((ordinal, values))
        if len(matches) != 1:
            raise ValueError(f'exact camera key requires one entry, found {len(matches)}')
        ordinal, r = matches[0]
        packed = r['packed'] & 0x7fffffff
        if r['high'] or r['codec_flags'] not in (0x10000, 0x1ffff) or not 0 < packed <= 1024 * 1024 or not 0 < r['memory'] <= 1024 * 1024:
            raise ValueError('camera resource flags/high-word/size unsupported')
        if r['offset'] < 96 or r['offset'] + packed > index_at:
            raise ValueError('camera resource outside bounded pre-index span')
        f.seek(r['offset'])
        payload = f.read(packed)
        if len(payload) != packed:
            raise ValueError('short camera resource')
        if r['codec_flags'] == 0x1ffff:
            # RefPack decoder uses its own declared length: validate before allowing allocation.
            if len(payload) < 5 or int.from_bytes(payload[2:5], 'big') != r['memory']:
                raise ValueError('camera RefPack declaration/index mismatch')
            payload = decode(payload, r['memory'])
        if len(payload) != r['memory']:
            raise ValueError('camera decoded length mismatch')
        return ordinal, payload


def report(path):
    ordinal, payload = read_camera(path)
    result = []
    for field in parse(payload):
        row = {**field, 'id': f'{field["id"]:08x}', 'type': f'{field["type"]:04x}'}
        raw = payload[field['data_offset']:field['data_offset'] + field['data_bytes']]
        row['raw_hex'] = raw.hex()
        if not field['array']:
            if field['type'] == 0x0d:
                value, = struct.unpack('>f', raw)
                if not math.isfinite(value):
                    raise ValueError('nonfinite camera property float')
                row['float'] = value
            elif field['type'] == 0x0a:
                row['uint32'], = struct.unpack('>I', raw)
            elif field['type'] == 0x13:
                row['string16'] = raw[4:].decode('utf-16-le', errors='strict')
        result.append(row)
    return {'package': str(path), 'record': ordinal, 'key_instance_type_group': [f'{v:08x}' for v in KEY],
            'bytes': len(payload), 'sha256': hashlib.sha256(payload).hexdigest(),
            'consumed': len(payload), 'fields': result,
            'limits': 'One explicit package, no priority/inheritance/native camera readiness'}
