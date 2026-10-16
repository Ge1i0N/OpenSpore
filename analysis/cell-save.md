# Cell-stage save decode — creature data structure

> **Verdict 2026-10-16:** the cell-stage `.spo` (19,683 B, 12 records) decodes
> to full field-level values. Two multi-object class streams hold 1503 + 1648
> declared objects in 7 frame types each. Float stats (1.0, π), position data
> and a dev-marker class `0xABCDEF00` are present. The engine decoder
> `source/engine/cell_save.cpp` reproduces the decode from C++ and matches the
> Python reference on the real save (stat4=79, stat9=1.0, stat18=512).
> This is **structural decode** — field *semantics* (which float is speed vs.
> attack) need binary confirmation.

## Stream layout

The file is a standard DBPF: header/index little-endian with a kind-selected
index (this save: kind=4 — prefix 8, stride 28, per-record type/group words),
parsed by `engine/pkg.cpp`. Inside, both big streams use
`[count BE u32][crc][token][12-byte header][frames...]` and **field payload
bytes are big-endian** (verified: `3f800000`=1.0, `00000200`=512,
`0000004f`=79 — the descriptor-table default).
Frames are found by tag scan at offset 24; gaps between frames vary (0–346 B).
Stream types: `0x018EB4B7` (refpacked 6627→225450) and `0x018EB45E`
(refpacked 5670→164800), both records in the same save.

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
- `analysis/audit/cell-save-full.json` — full decode (all walked objects).
- `source/engine/cell_save.h/.cpp` — engine decoder (read-only; built on
  `engine/pkg.cpp` + RefPack). `decode_save(path)` / `decode_save_from_memory`.
- `tools/cell_save_verify` — 5 constructed-input tests, no game data needed.
- `tools/cell_save_dump <path.spo>` — CLI dump of a real save.

## Field descriptors (from binary 0xF194A0)

The cell-save field table at VA `0xF194A0` (file offset `0xF188B8`) initializes
descriptors in BSS via `rep movsd`. Each 14-dword entry is:

| dword | meaning |
|---|---|
| 0 | runtime offset in creature struct |
| 1 | field ID (wire) |
| 2–3 | default value (u32/float) |
| 4 | flags (0) |
| 5 | read callback VA |
| 6 | write callback VA |

### Creature stats (frame type 1, 5 fields)

| field ID | offset | default | read CB | write CB |
|---|---|---|---|---|
| `0x0240E421` | `0x00` | `0x00000000` | `0x00ACA060` | `0x00BCA960` |
| `0x0240E422` | `0x04` | `0x00` (u8) | `0x00AC8750` | `0x0057CE00` |
| `0x0240E423` | `0x08` | `0xFFFFFFFF` | `0x00692FB0` | `0x00693090` |
| `0x02E9EFD3` | `0x0C` | `0x00000000` | `0x00572810` | `0x00572840` |
| `0x03CDD54B` | `0x10` | `0x0000004F`=79 | `0x00AC8050` | `0x00AC8050` |

### Position (frame type 2, 4 fields)

| field ID | offset | default | read CB | write CB |
|---|---|---|---|---|
| `0x024162BF` | `0x14` | `0x00000000` | `0x00AC8750` | `0x0057CE00` |
| `0x024162C0` | `0x18` | float | `0x00AC8780` | `0x00AC8780` |
| `0x024162C1` | `0x1C` | float | `0x00693090` | `0x00693090` |
| `0x024162C2` | `0x20` | `0x00000000` | `0x00694FA0` | `0x00694FA0` |

### Detailed stats (frame type 3, 11 fields)

| field ID | offset | default | read CB |
|---|---|---|---|
| `0x02416C5E` | `0x24` | 12× zero (vec3-sized, 12 B on wire) | `0x00AC8750` |
| `0x02416C60` | `0x30` | 16 B on wire: 0,0,0,`3F800000`=1.0 (vec4/quat-sized) | `0x00AC8780` |
| `0x02416C61` | `0x40` | `0x00` | `0x00693090` |
| `0x02416C62` | `0x44` | `0x01` | `0x00693090` |
| `0x02416C63` | `0x48` | `0x01` | `0x00693090` |
| `0x02416C64` | `0x4C` | `3F800000`=1.0 | `0x00AC8050` |
| `0x02416C65`–`0x02416C6E` | `0x50`–`0x6C` | mixed | various |
| `0x0316F520` | `0x70` | `3F800000`=1.0 | — |
| `0x0445DC86` | `0x74` | `0x00` | — |
| `0x047DF70D` | `0x78` | 12× zero | — |
| `0x047DF723` | `0x84` | 24× mixed | — |

Callbacks `0xAC8750`/`0xAC8780`/`0x693090`/`0xAC8050`/`0x572810`/`0x572840`/`0x57CE00`
are shared with the StarRecord stat callbacks — confirming these are the same
stat fields, now confirmed on real creature data.

## Boundaries

- Field IDs are structural; **semantics not yet confirmed** (which float is
  speed, attack, social — needs binary cross-reference at the stat descriptor).
- Gap-scan walk is lossy: the C++ decoder walks 2000 objects total
  (Python reference: 1200 + 1000); declared 1503 + 1648. Tail objects after
  long gaps are still missed; frame counts match declared counts in the
  envelope headers.
- `0x02416C60` is 16 B on the wire (3×zero + 1.0), treated as 4 floats.
- No creature *name* strings visible — names may be in single-envelope streams
  or encoded differently from StarRecord names.
