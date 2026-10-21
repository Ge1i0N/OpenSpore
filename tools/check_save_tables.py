"""Check the cell-save writer's per-frame wire tables against the shipped save audit.

`source/engine/cell_world.cpp` emits one creature group as a sequence of
`0x01A80D26` field frames. Each frame is

    [tag 0x01A80D26 BE][length BE][count BE][fid BE][size BE]*[payloads]

where `length` counts the bytes AFTER the length word itself, i.e.

    length = 4 + 8 * count + sum(sizes)

The writer's id/size tables must be the SHIPPED ones, so this tool re-derives
them from the shipped save audit (`analysis/audit/cell-save-full.json`, the
real cell-stage `.spo`) and compares them with the tables in the source.

The shipped cell-stage save holds TWO class streams:

  * `0x018EB4B7` - 150 creature groups, frame 7 carries 8 fields. This is the
    class the engine's writer emits.
  * `0x018EB45E` - 100 creature groups, the same frames 0..6 but a 13-field
    frame 7. A different class; the writer does not emit it.

Usage:  python tools/check_save_tables.py
Exit code 0 = the writer's tables match the shipped class exactly.
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = os.path.join(REPO, 'source', 'engine', 'cell_world.cpp')
AUDIT = os.path.join(REPO, 'analysis', 'audit', 'cell-save-full.json')
WRITER_CLASS = '0x018eb4b7'


def read_tables() -> dict:
    """Pull the kFrame<N>Ids[] / kFrame<N>Sizes[] tables out of the writer source."""
    src = io.open(SOURCE, encoding='utf-8').read()
    tables = {}
    pattern = r'static const u32 kFrame(\d+)(Ids|Sizes)\[\]\s*=\s*\{(.*?)\};'
    for m in re.finditer(pattern, src, re.S):
        idx, kind, body = int(m.group(1)), m.group(2), m.group(3)
        values = []
        for tok in re.findall(r'0x[0-9A-Fa-f]+|\d+', body):
            values.append(int(tok, 16) if tok.lower().startswith('0x') else int(tok))
        tables[(idx, kind)] = values
    return tables


def frame_length(count: int, sizes: list) -> int:
    return 4 + 8 * count + sum(sizes)


def main() -> int:
    tables = read_tables()
    audit = json.load(io.open(AUDIT, encoding='utf-8'))
    stream = audit[WRITER_CLASS]
    creatures = len(stream) // 8

    # Every creature in the stream must use the same 8-frame shape.
    for c in range(1, creatures):
        for f in range(8):
            if list(stream[c * 8 + f]['fields'].keys()) != list(stream[f]['fields'].keys()):
                print('FAIL: creature %d frame %d has a different id set than creature 0' % (c, f))
                return 1

    fail = 0
    print('%s: %d creatures x 8 frames' % (WRITER_CLASS, creatures))
    for f in range(8):
        frame = stream[f]
        shipped_ids = [int(k, 16) for k in frame['fields'].keys()]
        shipped_sizes = [len(v) // 2 for v in frame['fields'].values()]
        ids = tables.get((f, 'Ids'))
        sizes = tables.get((f, 'Sizes'))
        if ids is None or sizes is None:
            print('  frame %d: MISSING table in the writer' % f)
            fail += 1
            continue
        if ids != shipped_ids:
            print('  frame %d: IDS differ\n    writer : %s\n    shipped: %s' % (
                f, ['0x%08X' % i for i in ids], ['0x%08X' % i for i in shipped_ids]))
            fail += 1
        if sizes != shipped_sizes:
            print('  frame %d: SIZES differ\n    writer : %s\n    shipped: %s' % (f, sizes, shipped_sizes))
            fail += 1
        expect = frame_length(len(shipped_ids), shipped_sizes)
        if expect != frame['len']:
            print('  frame %d: computed length %d != shipped %d' % (f, expect, frame['len']))
            fail += 1
        else:
            print('  frame %d: %2d fields, %3d payload bytes, length %3d  OK' % (
                f, len(shipped_ids), sum(shipped_sizes), expect))

    # The other shipped class is reported, not enforced: it is a different class.
    other = [k for k in audit if k != WRITER_CLASS]
    for k in other:
        other_stream = audit[k]
        f7 = other_stream[7]
        print('\nother shipped class %s: %d groups, frame 7 has %d fields (the writer emits the '
              '%s class)' % (k, len(other_stream) // 8, len(f7['fields']), WRITER_CLASS))

    print('\n%s' % ('FAIL: %d mismatch(es)' % fail if fail
                    else 'PASS: the writer tables match the shipped save exactly'))
    return 1 if fail else 0


if __name__ == '__main__':
    sys.exit(main())
