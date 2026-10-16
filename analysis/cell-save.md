# Cell-stage save decode — creature data structure

> **Verdict 2026-10-16:** the cell-stage `.spo` (19,683 B, 12 records) decodes
> to full field-level values. Two multi-object class streams hold 1500 + 1000
> objects in 7 frame types each. Float stats (1.0, π), position data and a
> dev-marker class `0xABCDEF00` are present. This is **structural decode** —
> field *semantics* (which float is speed vs. attack) need binary confirmation.

## Stream layout

Both big streams use `[count BE u32][crc][token][12-byte header][frames...]`.
Frames are found by tag scan at offset 24; gaps between frames vary (0–346 B).

### 0x018EB4B7 — creature stats (1503 objects, 1200 walked)

| frame type | len | cnt | field IDs |
|---|---|---|---|
| 1 | 61 | 5 | `0240e421 0240e422 0240e423 02e9efd3 03cdd54b` |
| 2 | 52 | 4 | `024162bf 024162c0 024162c1 024162c2` |
| 3 | 149 | 11 | `02416c5e..02416c6e 0316f520 0445dc86 047df70d 047df723 049100ec 05e8faa8 0771d7b4 b1a64f7b d24b464b` |
| 4 | 330 | 25 | (larger stat block) |
| 5 | 28 | 2 | (minimal) |
| 6 | 622 | 35 | (full stat block) |
| 7 | 97 | 8 | (medium) |

Type-3 float samples: `3f800000`=1.0, `40490fdb`≈π, `3f000000`=0.5, `42c80000`=100.0,
`bf000000`=−0.5, `beaa7efa`≈−0.333. These are typical creature-stat values.

### 0x018EB45E — secondary stats (1648 objects, 1000 walked)

Same 7 frame types, same field IDs per type. Float values differ per object.

## Single-envelope streams

| type | count | content |
|---|---|---|
| `0x03A3AC9F` | 1 | frame: 3 fields (`03a3b30d`, `03a3b3d4`, `06676971`) |
| `0xABCDEF00` | 1 | **dev/test marker** — frame with 22 fields, ID-like values `06947316 06947317 35875139` |
| `0x02C21781` | 1 | frame: 2 fields |
| `0x03275728` | 1 | frame: 5 fields |
| `0x0179D310` | 1 | context index (92 B raw) |
| `0x01906183` | 1 | frame: 1 field |
| `0x018EB5C7` | 1 | frame: 1 field |
| `0x018EB4B7` | 1503 | multi-object stats |
| `0x018EB45E` | 1648 | multi-object secondary |
| `0x0355C93A` | 1 | frame: 1 field |
| `0x04238ECC` | 1 | frame: 1 field |

## Tools

- `tools/cell_save_decode.py <size-in-bytes>` — full field-value dump (size
  lookup avoids Cyrillic path issues). JSON output.
- `analysis/audit/cell-save-full.json` — full decode (all 2200 objects).

## Boundaries

- Field IDs are structural; **semantics unconfirmed** (which float is speed,
  attack, social — needs binary cross-reference at the stat descriptor).
- 303 objects in stream 1 and 648 in stream 2 were not walked (gap-scan
  limitation); frame counts match declared counts.
- No creature *name* strings visible — names may be in single-envelope streams
  or encoded differently from StarRecord names.
