"""Probe source-key byte sequences in explicit related DBPF resources, not native references.

Selects DB-right group/instance with an explicitly supplied different type. No
fallback lookup, directory search, priority merging, property semantics or readiness.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from collections import defaultdict

from gge_userdata_compare import TABLES, inspect
from gge_userdata_correlate import inventory


def patterns(triple):
    group, kind, instance = triple
    return {
        'be_group_type_instance': struct.pack('>III', group, kind, instance),
        'le_group_type_instance': struct.pack('<III', group, kind, instance),
        'be_instance_type_group': struct.pack('>III', instance, kind, group),
        'le_instance_type_group': struct.pack('<III', instance, kind, group),
    }


def occurrences(payload, needle):
    offsets = []
    total = 0
    at = payload.find(needle)
    while at != -1:
        total += 1
        if len(offsets) < 8:
            offsets.append(at)
        at = payload.find(needle, at + 1)
    return {'count': total, 'preview_offsets': offsets}


def read_payload(exe, path, record):
    proc = subprocess.run([str(exe), str(path), str(record['index']), '--dump'],
                          capture_output=True, timeout=30)
    if proc.returncode:
        raise ValueError(proc.stderr.decode('utf-8', errors='replace').strip())
    dumps = [line.split('=', 1)[1] for line in proc.stdout.decode('utf-8', errors='replace').splitlines()
             if line.startswith('resource_raw_hex=')]
    if len(dumps) != 1:
        raise ValueError('missing exact resource bytes')
    payload = bytes.fromhex(dumps[0])
    if len(payload) != record['memory'] or len(payload) > 1024 * 1024:
        raise ValueError('resource size mismatch/host cap')
    return payload


def probe(rows, records, selected_type, read):
    directory = defaultdict(list)
    for record in records:
        directory[record['key']].append(record)
    results = []
    if len(rows) > 64:
        raise ValueError('probe limited to 64 table rows; no partial report')
    for i, row in enumerate(rows):
        source, destination = row[:3], row[3:]
        selected = (destination[0], selected_type, destination[2])
        entry = {'row': i, 'source_group_type_instance': [f'{v:08x}' for v in source],
                 'table_destination': [f'{v:08x}' for v in destination],
                 'explicit_probe_key': [f'{v:08x}' for v in selected]}
        matches = directory.get(selected, [])
        if len(matches) != 1:
            entry['status'] = 'absent_in_selected_directory' if not matches else 'ambiguous'
        elif matches[0]['packed'] == 0:
            entry['status'] = 'unreadable_or_zero_size'
        else:
            try:
                payload = read(matches[0])
                entry.update(status='bytes_inspected', record=matches[0]['index'], bytes=len(payload),
                             sha256=hashlib.sha256(payload).hexdigest(),
                             source_key_patterns={k: occurrences(payload, v) for k, v in patterns(source).items()})
            except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
                entry.update(status='blocked', error=str(exc))
        results.append(entry)
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--frame-inspector', type=Path, required=True)
    parser.add_argument('--inventory-inspector', type=Path, required=True)
    parser.add_argument('--resource-inspector', type=Path, required=True)
    parser.add_argument('--package', type=Path, required=True)
    parser.add_argument('--payload-type', type=lambda s: int(s, 0), required=True)
    parser.add_argument('userdata', type=Path)
    args = parser.parse_args()
    report = {'userdata': str(args.userdata), 'package': str(args.package),
              'authority': 'local resource bytes; sequence occurrence only, not typed reference',
              'limits': '64 rows, 1MiB/payload, 30s/subprocess, 8 offsets preview; no writes/readiness'}
    try:
        if not 0 <= args.payload_type <= 0xffffffff:
            raise ValueError('payload type outside uint32')
        _, rows = inspect(args.frame_inspector.resolve(), args.userdata.resolve())
        records = inventory(args.inventory_inspector.resolve(), args.package.resolve())
        report['rows'] = probe(rows[TABLES[0]], records, args.payload_type,
                               lambda r: read_payload(args.resource_inspector.resolve(), args.package.resolve(), r))
        failed = any(r['status'] == 'blocked' for r in report['rows'])
    except (ValueError, OSError, subprocess.TimeoutExpired) as exc:
        report['error'] = str(exc)
        failed = True
    print(json.dumps(report, indent=2))
    return int(failed)


if __name__ == '__main__':
    raise SystemExit(main())
