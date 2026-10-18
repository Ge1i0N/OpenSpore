"""Read-only comparison of explicit GGEUserData files through the C++ DBPF inspector.

Observed wire-shape hypothesis only: three BE counted six-word tables and one
BE counted word list. Does not assign native member names or readiness.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess

TAG = 0x01A80D26
TABLES = (0x07DB67DB, 0x07DB67DC, 0x07DB67DD)
LIST = 0x07DB67DE


def decode(payload):
    if len(payload) < 12:
        raise ValueError("short nested frame")
    tag, length, count = struct.unpack_from('>III', payload)
    if tag != TAG or length + 8 != len(payload) or count != 4:
        raise ValueError("unexpected nested tag/length/field count")
    at = 12 + count * 8
    if at > len(payload):
        raise ValueError("nested directory exceeds payload")
    fields = {}
    for i in range(count):
        field, size = struct.unpack_from('>II', payload, 12 + i * 8)
        if field in fields or size > len(payload) - at:
            raise ValueError("duplicate field or field outside nested payload")
        fields[field] = payload[at:at + size]
        at += size
    if at != len(payload) or set(fields) != set(TABLES + (LIST,)):
        raise ValueError("unsupported nested field set or tail")
    rows = {}
    for field, body in fields.items():
        if len(body) < 4:
            raise ValueError("short collection count")
        n, = struct.unpack_from('>I', body)
        width = 1 if field == LIST else 6
        if n > 4096 or len(body) != 4 + n * width * 4:
            raise ValueError("collection count/stride/host bound mismatch")
        words = struct.unpack_from('>' + 'I' * (n * width), body, 4)
        rows[field] = [tuple(words[i:i + width]) for i in range(0, len(words), width)]
    return rows


def inspect(inspector, path):
    proc = subprocess.run([str(inspector), str(path), '0', '0x07ca2689', '--dump'],
                          capture_output=True, timeout=30, check=False)
    if proc.returncode:
        raise ValueError(proc.stderr.decode('utf-8', errors='replace').strip())
    lines = proc.stdout.decode('utf-8', errors='strict').splitlines()
    dumps = [line.split('=', 1)[1] for line in lines if line.startswith('nested_raw_hex=')]
    if len(dumps) != 1:
        raise ValueError("expected one exact decoded nested payload")
    payload = bytes.fromhex(dumps[0])
    return payload, decode(payload)


def summary(payload, rows):
    forward, alternate, reverse = (rows[f] for f in TABLES)
    # Compare exact six-word rows with multiplicity; never collapse duplicate keys.
    inverse = Counter(row[3:] + row[:3] for row in forward)
    first_words = Counter((row[0],) for row in reverse)
    return {
        'nested_bytes': len(payload), 'nested_sha256': hashlib.sha256(payload).hexdigest(),
        'counts': {f'{f:08x}': len(rows[f]) for f in TABLES + (LIST,)},
        'forward_reverse_multiset_equal': inverse == Counter(reverse),
        'word_list_matches_reverse_third_word_multiset': Counter(rows[LIST]) ==
            Counter((row[2],) for row in reverse),
        'reverse_first_word_distribution': {f'{k[0]:08x}': v for k, v in first_words.items()},
        'forward_alternate_left_triples_equal_in_order':
            [r[:3] for r in forward] == [r[:3] for r in alternate],
        'preview_words_hex': {f'{f:08x}': [[f'{v:08x}' for v in r] for r in rows[f][:8]]
                              for f in TABLES + (LIST,)},
    }


def compare(a, b):
    result = {}
    for f in TABLES + (LIST,):
        x, y = Counter(a[f]), Counter(b[f])
        result[f'{f:08x}'] = {
            'unchanged_in_order': a[f] == b[f],
            'added_rows': sum((y - x).values()), 'removed_rows': sum((x - y).values()),
        }
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspector', type=Path, required=True)
    parser.add_argument('files', type=Path, nargs='+', help='Explicit GGEUserData.dat paths')
    args = parser.parse_args()
    report = {'authority': 'local save bytes; observed wire-shape hypothesis, not native schema',
              'limits': '4096 rows/field, preview 8, inspector timeout 30s; no writes/readiness',
              'files': [], 'comparisons_to_first': []}
    baseline = None
    baseline_path = None
    failed = False
    for path in args.files:
        try:
            payload, rows = inspect(args.inspector.resolve(), path.resolve())
            report['files'].append({'path': str(path), 'status': 'decoded', **summary(payload, rows)})
            if baseline is None:
                baseline = rows
                baseline_path = str(path)
            else:
                report['comparisons_to_first'].append({'baseline': baseline_path, 'path': str(path),
                                                       'fields': compare(baseline, rows)})
        except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
            report['files'].append({'path': str(path), 'status': 'blocked', 'error': str(exc)})
            failed = True
    print(json.dumps(report, indent=2, ensure_ascii=True))
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
