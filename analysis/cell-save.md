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
- `tools/cell_field_descriptors.py` — recovers all wire-id registration
  descriptors from the binary; writes `audit/cell-field-descriptors.json`.

## Field descriptors (inline code, 107/107 ids found)

Registration is **inline code, not a static table**: each field is written
into a stack descriptor by `mov dword [esp+off], imm32` stores
(`tools/cell_field_descriptors.py` recovers all 107 wire ids from the real
save; audit in `analysis/audit/cell-field-descriptors.json`). Two shapes:

- shape A: id at `[esp+0x14]`, callbacks at `[esp+0x34..0x48]`, value word
  at `[esp+0x18]`, registry fn at `[esp+0x10]` (VA `0x1469990` / `0x146AEAC`
  / `0x146A930` / `0x1473900`… — one per block)
- shape B: id at `[esp+0x0C]`, callbacks at `[esp+0x2C..0x40]`, value word
  at `[esp+0x10]` (frame-type-1 block starts at file `0xF188B4`, VA `0xF194B4`)

The word each callback chain ends in is size-correlated, not a default: the
12-byte fields (`0x02416C5E`, `0x02415692`, `0x049489A3`) share one chain,
the two 24-byte fields (`0x047DF723`, `0x05E8FAA8`) share another, the two
92-byte blobs (`0x076C8F46/4D`) share a third, the 16-byte `0x02416C60` has
a unique chain. 23 one-byte fields share the `AC8750/AC8780/693090/694FA0`
chain. A callback chain is therefore a **type signature**:

| chain (first 2 cbs) | count | wire size | example ids |
|---|---|---|---|
| `AC8750 AC8780 …` | 23 | 1 B | `0x0240E422`, `0x02416C61..63`, `0x02415EBC..` |
| `572810 AC88F0 …` | 13+6+1 | 4 B | `0x02415EB8/9/BB`, `0x024162C1`, `0x02416C64` |
| `572810 572840 …` | 12+10+2+1 | 4 B | `0x0240E423`, `0x02E9EFD3`, `0x03CDD54B`, `0x024162C0` |
| `ACA060 BCA960 …` | 2 | 4 B | `0x0240E421`, `0x024162BF` |
| `B6E3A0 AC88C0 …` | 3 | 12 B | `0x02416C5E`, `0x02415692`, `0x049489A3` |
| `AE33D0 AE3400 …` | 1 | 16 B | `0x02416C60` (0,0,0,1.0 on wire) |
| `AE3430 AE3450 …` | 3 | 12/24 B | `0x03F64A78`, `0x047DF70D`, `0x03F6565D` |
| `C88B60 C88B80 …` | 2 | 24 B | `0x047DF723`, `0x05E8FAA8` |
| `C143C0 C143E0 …` | 2 | 92 B | `0x076C8F46`, `0x076C8F4D` |

Field ids are **not unique across classes**: `0x024162BF` registers with the
type-1 stat chain at VA `0xF60778` (another class) while the cell-save
position block uses other chains. Earlier per-frame-type tables in this file
that assigned single offsets/defaults to ids were an over-read of this code
and are superseded by the JSON audit.

## Boundaries

- Field IDs are structural; **per-id semantics not yet confirmed** (which
  float is speed, attack, social). Callback chains now type each field by
  size class (1/4/12/16/24/92 B); naming individual stats needs the
  registration functions (`0x1469990` etc.) or gameplay cross-references.
- Gap-scan walk is lossy: the C++ decoder walks 2000 objects total
  (Python reference: 1200 + 1000); declared 1503 + 1648. Tail objects after
  long gaps are still missed; frame counts match declared counts in the
  envelope headers.
- `0x02416C60` is 16 B on the wire (3×zero + 1.0), treated as 4 floats.
- No creature *name* strings visible — names may be in single-envelope streams
  or encoded differently from StarRecord names.
