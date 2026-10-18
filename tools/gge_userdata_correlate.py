"""Correlate explicit GGEUserData tables with explicit DBPF directories, read-only.

Tests the observed triple order (group,type,instance); no native schema, resource
priority, payload readability or catalogue readiness is inferred from a match.
"""
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import subprocess

from gge_userdata_compare import TABLES, inspect

RECORD = re.compile(r'^\[(\d+)\] instance=0x([0-9a-f]+) type=0x([0-9a-f]+) '
                    r'group=0x([0-9a-f]+) packed=(\d+) memory=(\d+) compression=0x([0-9a-f]+)$')


def parse_inventory(text):
    lines = text.splitlines()
    headers = [line for line in lines if line.startswith('EXPLICIT DBPF INVENTORY ONLY ')]
    if len(headers) != 1:
        raise ValueError('inventory header unavailable')
    declared = re.search(r' records=(\d+) kind=\d+ groups=\d+$', headers[0])
    if not declared:
        raise ValueError('inventory count unavailable')
    records = []
    for line in lines:
        if not line.startswith('['):
            continue
        m = RECORD.fullmatch(line)
        if not m:
            raise ValueError('unrecognized inventory record')
        index, instance, kind, group, packed, memory, compression = m.groups()
        if int(index) != len(records):
            raise ValueError('non-contiguous inventory ordinals')
        records.append({'index': int(index), 'key': (int(group, 16), int(kind, 16), int(instance, 16)),
                        'packed': int(packed), 'memory': int(memory), 'compression': int(compression, 16)})
    if len(records) != int(declared[1]):
        raise ValueError('incomplete directory output')
    return records


def inventory(exe, path):
    proc = subprocess.run([str(exe), str(path), '--full'], capture_output=True, timeout=30)
    if proc.returncode:
        raise ValueError(proc.stderr.decode('utf-8', errors='replace').strip())
    return parse_inventory(proc.stdout.decode('utf-8', errors='replace'))


def correlate(rows, records):
    directory = defaultdict(list)
    related = defaultdict(list)
    for r in records:
        directory[r['key']].append(r)
        group, kind, instance = r['key']
        related[(group, instance)].append(r)
    report = {}
    for field in TABLES:
        sides = {}
        for side, start in (('left', 0), ('right', 3)):
            counts = Counter()
            other_type_rows = 0
            other_types = Counter()
            preview = []
            for ordinal, row in enumerate(rows[field]):
                key = row[start:start + 3]
                matches = directory.get(key, [])
                different_types = [r for r in related.get((key[0], key[2]), []) if r['key'][1] != key[1]]
                if different_types:
                    other_type_rows += 1
                    other_types.update(r['key'][1] for r in different_types)
                if not matches:
                    status = 'absent_in_selected_directory'
                elif len(matches) > 1:
                    status = 'ambiguous'
                elif matches[0]['packed'] == 0:
                    status = 'unique_directory_entry_unreadable_or_zero_size'
                else:
                    status = 'unique_directory_entry_payload_not_checked'
                counts[status] += 1
                if len(preview) < 8:
                    preview.append({'row': ordinal, 'group_type_instance': [f'{v:08x}' for v in key],
                                    'status': status, 'record_indices': [r['index'] for r in matches],
                                    'same_group_instance_other_types': [f'{r["key"][1]:08x}' for r in different_types[:8]]})
            sides[side] = {'rows': len(rows[field]), 'counts': dict(counts), 'preview': preview,
                           'rows_with_same_group_instance_other_type': other_type_rows,
                           'other_type_entry_counts': {f'{k:08x}': v for k, v in other_types.items()}}
        report[f'{field:08x}'] = sides
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frame-inspector', type=Path, required=True)
    parser.add_argument('--inventory-inspector', type=Path, required=True)
    parser.add_argument('--package', type=Path, action='append', required=True,
                        help='One explicit DBPF, repeat for each desired comparison; no search or merge')
    parser.add_argument('userdata', type=Path)
    args = parser.parse_args()
    report = {'authority': 'local save bytes; group/type/instance wire-order hypothesis',
              'userdata': str(args.userdata), 'packages': [],
              'limitations': ['Exact directory matches only; no payload decode or priority merge',
                              'Absence is limited to each selected directory',
                              'Package reader may mark out-of-layout resource spans with packed=0',
                              'No candidate/refill/terrain readiness claim']}
    failed = False
    try:
        _, rows = inspect(args.frame_inspector.resolve(), args.userdata.resolve())
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        report['error'] = str(exc)
        print(json.dumps(report, indent=2))
        return 1
    for path in args.package:
        try:
            records = inventory(args.inventory_inspector.resolve(), path.resolve())
            report['packages'].append({'path': str(path), 'status': 'inspected',
                                       'records': len(records), 'tables': correlate(rows, records)})
        except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
            report['packages'].append({'path': str(path), 'status': 'blocked', 'error': str(exc)})
            failed = True
    print(json.dumps(report, indent=2, ensure_ascii=True))
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
