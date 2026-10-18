"""Strict bounded property-table subset for caller-supplied decoded bytes.

Historical guide: upstream/SporeMaster3/docs/PROP_FORMAT.md. Corroboration requires
exact consumption of actual bytes; this is not REA native-reader evidence.
Unsupported types/flags/strides fail closed, never resynchronize by magic scanning.
"""
import struct

# Known scalar widths; composite scalars smaller than 16 occupy padded slots.
WIDTH = {0x00: 16, 0x01: 1, 0x02: 1, 0x03: 2, 0x05: 1, 0x06: 1,
         0x07: 2, 0x08: 2, 0x09: 4, 0x0a: 4, 0x0b: 8, 0x0c: 8,
         0x0d: 4, 0x0e: 8, 0x20: 12, 0x30: 8, 0x31: 12, 0x32: 12,
         0x33: 16, 0x34: 16, 0x38: 56, 0x39: 24}


def parse(payload):
    if len(payload) < 4 or len(payload) > 1024 * 1024:
        raise ValueError('property payload size/host bound')
    declared, = struct.unpack_from('>I', payload)
    if declared > 4096:
        raise ValueError('property field count exceeds host bound')
    at = 4
    fields = []
    seen = set()
    aggregate = 0
    for _ in range(declared):
        if len(payload) - at < 8:
            raise ValueError('truncated property header')
        header = at
        key, kind, flags = struct.unpack_from('>IHH', payload, at)
        if key in seen:
            raise ValueError('duplicate property id')
        if flags & 0x40:
            raise ValueError('unsupported property flag 0x40')
        seen.add(key)
        at += 8
        array = bool(flags & 0x30)
        count = 1
        if array:
            if len(payload) - at < 8:
                raise ValueError('truncated property array header')
            count, stride = struct.unpack_from('>II', payload, at)
            at += 8
            if kind not in WIDTH or stride != WIDTH[kind]:
                raise ValueError(f'unsupported property array type/stride {kind:04x}/{stride}')
            size = count * stride
        elif kind in (0x12, 0x13):
            if len(payload) - at < 4:
                raise ValueError('truncated property string count')
            count, = struct.unpack_from('>I', payload, at)
            if count > 4096:
                raise ValueError('property string exceeds host bound')
            stride = 1 if kind == 0x12 else 2
            size = 4 + count * stride
        elif kind in WIDTH:
            stride = WIDTH[kind]
            size = max(16, stride) if 0x20 <= kind <= 0x34 else stride
        else:
            raise ValueError(f'unsupported property scalar type {kind:04x}')
        aggregate += count
        if aggregate > 262144 or size > len(payload) - at:
            raise ValueError('property data count/bounds mismatch')
        fields.append({'id': key, 'type': kind, 'flags': flags, 'array': array,
                       'count': count, 'stride': stride, 'header_offset': header,
                       'data_offset': at, 'data_bytes': size})
        at += size
    if at != len(payload):
        raise ValueError('property table has unsupported trailing bytes')
    return fields


def summary(payload):
    try:
        fields = parse(payload)
    except ValueError as exc:
        return {'status': 'blocked', 'error': str(exc)}
    previews = []
    for field in fields:
        at, kind = field['data_offset'], field['type']
        entry = {**field, 'id': f'{field["id"]:08x}', 'type': f'{kind:04x}',
                 'flags': f'{field["flags"]:04x}'}
        if not field['array'] and kind == 0x13:
            try:
                entry['string16'] = payload[at + 4:at + field['data_bytes']].decode('utf-16-le', errors='strict')
            except UnicodeError:
                entry['string16_status'] = 'invalid_utf16'
        if kind == 0x20:
            keys = []
            for i in range(min(field['count'], 8)):
                instance, resource_type, group = struct.unpack_from('<III', payload, at + i * field['stride'])
                keys.append([f'{v:08x}' for v in (group, resource_type, instance)])
            entry['keys_group_type_instance_preview'] = keys
            if not field['array']:
                entry['slot_tail_hex'] = payload[at + 12:at + 16].hex()
        previews.append(entry)
    return {'status': 'exact_subset', 'fields': len(fields), 'consumed': len(payload),
            'field_previews': previews[:64], 'authority': 'historical format guide + exact local-byte consumption'}
