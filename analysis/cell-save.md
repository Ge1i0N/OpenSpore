# Cell-stage save decode — creature data structure

> **Verdict 2026-10-16:** the cell-stage `.spo` (19,683 B, 12 records) decodes
> to full field-level values. Two multi-object class streams hold **150 + 100
> creature objects**; each object is a GROUP of 8 serialized field frames
> with field counts `[5][4][25][11][4][2][35][8|13]`, closed by a 24-byte
> object trailer. The engine decoder `source/engine/cell_save.cpp` walks all
> 250 groups and matches the Python reference byte-for-byte on the stats.
> This is **structural decode** — field *semantics* (which word is speed vs.
> attack) need binary confirmation.

## Stream layout

The file is a standard DBPF: header/index little-endian with a kind-selected
index (this save: kind=4 — prefix 8, stride 28, per-record type/group words),
parsed by `engine/pkg.cpp`. Inside, both big streams use
`[count BE u32][crc][token][12-byte header][creature groups...]` and
**field payload bytes are big-endian** (verified: `3f800000`=1.0,
`00000200`=512, `0000004f`=79 — the descriptor-table word).
Stream types: `0x018EB4B7` (refpacked 6627→225450) and `0x018EB45E`
(refpacked 5670→164800), both records in the same save.

### One creature = one group of frames

Each creature is a GROUP of 8 serialized field frames (tag `0x01A80D26`),
separated by 0–8 B and closed by a **24-byte object trailer** (first dword =
the envelope count word). Observed exactly 150 groups in `0x018EB4B7` and
100 in `0x018EB45E`; the envelope `count` words (1503 / 1648) are NOT the
group count. Frame field-count pattern and ids (group 0 of stream A):

| # | cnt | content |
|---|---|---|
| 0 | 5 | stat ids `0240e421 0240e422 0240e423 02e9efd3 03cdd54b` |
| 1 | 4 | packed coordinates `024162bf..c2` (**not floats**) |
| 2 | 25 | detailed stats `02416c5e..02416c6e` + 11 larger blocks |
| 3 | 11 | `02415689..02415693` + `037d3df5 039a844f 049489a3 04c919ab` |
| 4 | 4 | `02415eb8..bb` |
| 5 | 2 | `043c4ab2 04bfed68` |
| 6 | 35 | big mixed block incl. the 92-byte `076c8f46/4d` blobs |
| 7 | 8 (A) / 13 (B) | `02e6e1ab..b2 05f0d6f3 06a814a9` (stream B adds 5) |

**The two streams are two CLASSES, not two views of one.** Frames 0-6 are
byte-identical in both; only frame 7 differs, and the group block sizes differ
by exactly that frame (1503 B per group in A, 1648 B in B). A's frame-7 ids are
one consecutive family (`02e6e1ab 02e6e1ad 02e6e1af 02e6e1b0 02e6e1b1
02e6e1b2`) plus two more; B's are 13 scattered ids. `tools/check_save_tables.py`
re-derives the writer's tables from `analysis/audit/cell-save-full.json` and
checks them against the shipped `0x018eb4b7` class: all 8 frames match in ids,
payload sizes **and** the frame length word
(`length = 4 + 8*count + sum(sizes)` -> 61, 52, 330, 149, 52, 28, 622, 97).
`World::write_save` emits that class. The `0x018eb45e` class is **not**
emitted - the engine writes one group (the player), not 250.

`0x03cdd54b` is the object **sequence id** (79, 80, 81 … in stream A;
stream B continues 0xac=172…). Type-3-style float samples in the detailed
block (`3f800000`=1.0, `40490fdb`≈π, `3f000000`=0.5, `42c80000`=100.0,
`beaa7efa`≈−0.333) are real creature-stat values.

### `0x024162BF..C2` are packed ints, not floats

Wire values like `04f30503` / `ee0f01f4` decode as int16 pairs
(1267/1283, −4337/500) — consistent with the cell stage's 2D integer world.
Their callback chains are the u32 families (see the descriptor section),
not the float one. Kept as raw u32 until the packing is fully decoded.

**Which field holds what.** Measured over all 150 shipped groups of the
`0x018eb4b7` class: `0x024162BF` and `0x024162C2` are **0 in every group**;
`0x024162C0` holds a packed int16 pair and `0x024162C1` a second one (both
spanning the whole int16 range, -32713..32767). So the position block is
`0, (x, y), (vx, vy), 0` - and the engine's writer now emits exactly that
instead of putting the coordinates in the first field. The **wire scale is a
SEED** (1/100 of a world unit): the shipped values cover the whole int16
range and the game's world size was not recovered.

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
| `0x018EB4B7` | 1503* | multi-object stats — 150 creature groups |
| `0x018EB45E` | 1648* | multi-object secondary — 100 creature groups |
| `0x0355C93A` | 1 | frame: 1 field |
| `0x04238ECC` | 1 | frame: 1 field |

\* envelope `count` word — NOT the object count (it equals the byte size of
one group block: stream A blob = 150 blocks × 1503 B, B = 100 × 1648 B).
The true counts are 150 + 100 creature groups (8 frames each).

## Tools

- `tools/cell_save_decode.py <size-in-bytes>` — full field-value dump (size
  lookup avoids Cyrillic path issues). JSON output.
- `analysis/audit/cell-save-full.json` — full decode (all walked objects).
- `source/engine/cell_save.h/.cpp` — engine decoder (read-only; built on
  `engine/pkg.cpp` + RefPack). `decode_save(path)` / `decode_save_from_memory`.
- `tools/cell_save_verify` — 5 constructed-input tests, no game data needed.
- `tools/cell_save_dump <path.spo>` — CLI dump of a real save.
- `tools/check_save_tables.py` — re-derives the writer's per-frame wire tables
  from the shipped audit and checks them against it (ids, payload sizes and
  the frame length word).
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

## Cell-stage tuning in the oracle binary

The game binary carries the **cell-stage gameplay vocabulary and balance
tables** as plaintext data, even though the save-field ids are separate.
`tools/cell_field_descriptors.py`-adjacent scans (scratch dumps) recovered:

**1. The Spore name hash is FNV-1 (not FNV-1a)** — verified on known ids:
`fnv1("creature") = 0x9EA3031A` (`kCreature`) and `fnv1("cell") = 0xDFAD9F51`
(`kCell`). This is the hash SporeModder FX uses for resource/instance names.

**2. Cell-tuning property vocabulary** (`.rdata` key clusters, read by the
tuning system): `hp`, `hpValue`, `foodValue`, `chaseSpeed`, `fleeSpeed`,
`wanderSpeed`, `digestionOutput`, `digestionCount`, `digestionTime`,
`growAmount`, `growCount`, `spawnOutput`, `spawnTime`, `awarenessRadius`,
`awarenessRadiusPredator`, `awarenessRadiusFood`, `protectTime`, `protectRadius`,
`fearsDamage`, `fearsWeapons`, `fearsMouths`, `chasesDamage`, `chaseTime`,
`fleeTime`, `hatchDuration`, `sleepTime`, `awakeTime`, `electricDischarge`,
`electricRecharge`, `poisonRecharge`, `poisonNova`, `ignoresFood`, `turnFactor`,
`axisMovement`→`axialMovement`, `flocking`, `movementStyle`, and the part list
`cell_flagella`/`cell_cilia`/`cell_jet`/`cell_spike`/`cell_jaw`/`cell_filter`,
`cellType`, `cell_carnivore`/`cell_herbivore`/`cell_omnivore`.

**3. Creature size→HP progression table** (file `0x010830E0`, stride `0x1C`):
key `22,25,28,…` (step 3), `hp` geometric ≈×1.73 (`30,52,100,173,300,520,
1000,1730,3000,5200,10000,17300,30000,52000`), a lagging second HP, a size
fraction `0.25/0.5/0.75/1.0`, and a species-class tag `0x1D2EC0A5..A0`. This is
the cell/creature size-level balance (HP tiers per growth stage).

**4. Cell tuning lives at `Simulator/Cell/Data`** (string in `.rdata`); the
tuning `.prop` files that bind these names to values are **not in the exe** —
they ship in the game's `Data` packages.

## Boundaries

- Field IDs are structural; **per-id semantics not yet confirmed** (which
  float is speed, attack, social). Callback chains now type each field by
  size class (1/4/12/16/24/92 B).
- The **runtime creature fields, however, are now named**: the registration
  functions `FUN_013326c0` (`mHealthPoints`=0x38, `mMaxHealthPoints`=0x2c,
  `mLastAttacker`=0x3c, `mArmorProbability`=0x40), `FUN_01335000`
  (`mFoodValue`, `mSpeciesKey`, `mAge`, `mArchetype`, `mSpeedState`,
  `mStrengthRating`, `mHunger*`, `mCreatureName`, ...), `FUN_01353d90`
  (`mPosition`=0x04, `mOrientation`=0x10, `mScale`=0x64, `mBoundingRadius`=0x5c,
  `mFootprintRadius`=0x60, `mIsTangible`, `mbPickable`, ...) and `FUN_01335fe0`
  (`mSelectionGroup`, `mSpecializedTool`, `mpOwnerCity`, `mpOwnerTribe`, ...)
  each write a `{name, id, offset, ...}` descriptor entry, so the **object**
  side of the save is named even though the **wire ids** are not. The
  `cell` archetype's own fields (HP, speed, awareness, food, growth) are
  named and offset-mapped too - see
  [cell-game](cell-game.md#the-archetype-record-layout-recovered-from-the-ga-binary).
- **Semantic naming via property-strings is closed as a dead end.** The 107
  registration blocks contain **no string pointers** (scan of `mov[esp]/push/lea`
  immediates → zero name candidates). Each wire id appears **exactly once** in
  `.text` (its own registration), so consumers read fields by object offset,
  not by id — there is no lookup-by-id table to cross-reference. `0x5242896A`
  (4 B, present on all 250) is the only other varying field besides the
  packed-int coordinates and the sequence id; it is not a gameplay stat (it
  tracks the coordinate blocks). Naming needs consumer-side offset analysis,
  not registration-side strings.
- **No name in the binary hashes (FNV-1) to any of the 107 save ids** — brute
  force over every string in `.text/.rdata/.data`, the 33k-line string dumps,
  and a gameplay-vocabulary combo generator (prefix×word, camelCase) all return
  zero hits. The save-field names therefore live **only in external `.prop`
  data**, not in the executable. Known tuning keys like `creatureScale`
  (`0x036AD587`) are not FNV-1 of their names either, confirming a separate
  (data-side) hash namespace.
- Gap-scan lossiness is resolved: the walk is now group-based (8 frames +
  24-byte trailer per creature) and recovers all 250 objects (150 + 100);
  the exact-frame validation rejects in-payload tag collisions instead of
  stopping the walk.
- `0x02416C60` is 16 B on the wire (3×zero + 1.0), treated as 4 floats.
- No creature *name* strings visible — names may be in single-envelope streams
  or encoded differently from StarRecord names.
- The coordinate packing (`0x024162BF..C2`) is raw u32; int16-pair decoding
  is plausible but not confirmed.
- **Variation census over all 250 objects** (both streams): aside from the
  sequence id (`0x03CDD54B`) and the packed-int coordinates
  (`0x024162C0/C1`), the only other field varying per object is
  `0x5242896A` (4 B), and it moves with the coordinate blocks — a movement /
  animation vector, not a discrete gameplay stat. Every other one of the ~107
  wire fields is constant across this save (e.g. `0x05501F64`=100.0,
  `0x05501F65`=−0.333, `0x021D1BA5`=50.0, `0x02416C64`=1.0). So this save has
  **no per-object stat spread** to correlate against gameplay; the
  speed/attack/social values are constants (creature defaults), and their
  meaning cannot be recovered from value correlation.

## The engine's own save round-trip

`World::write_save` now emits the **same frame shape the decoder reads**: a stream
header `[count][crc][token][12 bytes]`, then the shipped 8-frame field-count
pattern `[5][4][25][11][4][2][35][8]`, each frame a `0x01A80D26` record
(`[tag][length][count]` + per-field `[id][size]` + payloads), closed by the 24-byte
object trailer, inside a well-formed DBPF (kind-4 index, count 1). Previously the
writer emitted a bare run of field ids with no frame header, so the decoder read
**zero** creatures and `World::load_save` silently restored nothing.
`cell_world_verify` now has a round-trip case: write -> read back -> the player's
position is restored.
