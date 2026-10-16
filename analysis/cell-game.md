# Cell-stage game data (`CellGame.package`)

> **Verdict, 2026-10-17:** the cell mini-game (`Game_Cell`, state `CellGame`) is defined
> **entirely by data packages** — its archetypes, globals, spawn/biome/loot tables and
> animation list all ship in `Data/CellGame.package` (442 resources) and
> `Data/CellGame_Layers.package` (33). Resource types and instance names are 32-bit
> **FNV-1** hashes of lower-cased names, so they are fully recoverable. This note is the
> data model for the cell stage; the runtime skeleton is in `analysis/game-stages.md`.

Reproduce: `python tools/cell_game.py` (add `--globals` for the tuning curves, `--anims`
for the animation table, `--json out.json` for the audit).

## Hashing (tools/spore_hash.py)

Spore names resources and properties with FNV-1 over the **lower-cased** name:
`h = (h * 0x01000193) ^ c`, seed `0x811C9DC5`. Confirmed:

| name | hash | where |
|---|---|---|
| `cell` | `0xDFAD9F51` | 189 archetypes in `CellGame.package` |
| `structure` | `0x4B9EF6DC` | 144 |
| `world` | `0x9B8E862F` | 13 biomes |
| `globals` | `0x2A3CE5B7` | cell-globals block |
| `look_table` | `0x8C042499` | 9 steering tables |
| `loottable` | `0xD92AF091` | 48 loot tables |
| `random_creature` | `0xF9C3D770` | 10 spawn tables |
| `populate` | `0xDA141C1B` | 23 spawn populations |
| `cell_dragon` | `0x5905CC05` | archetype instance |
| `cell_squid` | `0x4BC16582` | archetype instance |
| `time_dilation` | `0x18B75C47` | cell-globals property key |
| `cilia_count` | `0x2EF68471` | cell-globals property key |

Names come from `upstream/SporeMaster3/data/reg_*.txt` (127 k names) plus a small
embedded cell dictionary; `reg_*.txt` also carries explicit aliases (`name<TAB>0xID`) for
ids that are not FNV of the name.

## Resource types in `CellGame.package`

| type | name | count | meaning |
|---|---|---:|---|
| `0xDFAD9F51` | `cell` | 189 | creature/prop archetypes (see below) |
| `0x4B9EF6DC` | `structure` | 144 | cell-part structure layout, 80 B record |
| `0xD92AF091` | `loottable` | 48 | part-unlock / drop tables (below) |
| `0x04805684` | `advect` | 28 | current/flow maps |
| `0xDA141C1B` | `populate` | 23 | spawn populations |
| `0x9B8E862F` | `world` | 13 | tide-pool biome definitions |
| `0xF9C3D770` | `random_creature` | 10 | random spawn tables |
| `0x8C042499` | `look_table` | 9 | steering weights |
| `0x5C74D18B` | `density` | 5 | density layer |
| `0xDBA35AE2` | (unnamed) | 1 | id→value transition table (232 B) |
| `0x433FB70C` | `effectMap` | 1 | effect map |
| `0x00B1B104` | `prop` | 1 | **cell globals** table |
| `0x612B3191` | `backgroundMap` | 1 | background map |
| `0x1F639D98` | `xls` | 1 | **animation table** (OLE2/Excel) |
| `0x8C042499`… | — | — | — |

## Cell globals — `prop`/`cell_properties`

A property table (`tools/prop.py`; type `0x0030009C` = float2 array) whose keys are the
game's tuning names. Each value is a **curve** `(x, y)` pairs:

| key | curve (x, y) | reading |
|---|---|---|
| `evo_points` | (1,20)(2,40)(3,60)(4,80)(5,100) | evo points granted per level |
| `cilia_count` | (0,0)(2,0.2)(4,0.25)…(10,0.31) | speed vs cilia count |
| `flagella_count` | (0,0)(2,5)(4,7)…(10,8) | speed vs flagella count |
| `jet_count` | (0,0)(2,0.05)…(10,0.1) | speed vs jet count |
| `eye_count` | (0,0)(2,2)…(10,10) | awareness vs eye count |
| `poison_count` | (0,0)(2,1.5)…(10,1.5) | poison damage vs count |
| `electric_jumps_count` | (0,0)(2,1)…(10,3) | chain-targets vs count |
| `electric_range_count` | (0,0)(2,1) | electric range factor |
| `electric_recharge_count` | (0,0)(2,10)(4,8)…(10,2) | recharge time |
| `time_dilation` | (0.5,4)(1,2)…(16,1) | size→time scale |
| `anim_dilation` | (0.5,0.5)(1,0.75)(2,1)…(8,4) | animation speed |
| `awareness_dilation` | (0.5,0.5)(1,1)…(16,16) | perception radius |
| `npc_turn_factor` | (0.5,1)(1,1)(2,0.5)…(16,0.05) | NPC turn rate |
| `cilia_graph` | (0,1)(0.1,1)(0.2,1)(0.3,1) | cilia visual graph |
| `mouse_zoom` | (0,1)(480,0.7)(960,0.5) | camera zoom vs distance |
| `camera_lookahead` | (0,3)…(32,32) | camera look-ahead distance |
| `camera_lerpfactor` | (0,2.5)(2,1.75)…(32,1) | camera smoothing |

Plus `globals`/`cellgame` (196 B raw struct) holding more scalar tuning (floats
`5.0, 0.5, 0.2, 1.0, 3.0, 250.0, 50.0, 35.0, 0.1, 40.0, 20.0, 100.0` from offset `0x40`).

## Cell archetypes — `cell` (189)

Each archetype is a 400-byte record: a 4-byte header then a **UTF-16LE display name**
(e.g. `cell_squid` → `"Snorf"`, `Cell_ChomperMed` → `"Chomper"`, `cell_booster` →
`"Booster"`). Named archetypes include the player parts (`cell_minno1/2/3`, `cell_leggy`,
`cell_nosey`, `cell_ocatmouth`), enemies (`cell_squid`, `Cell_ChomperMed`,
`cell_megamouth`, `Cell_StabberAlphaMed`, `Cell_GoldySpecial`), food/plants
(`cell_flora_clover_L0..2`, `cell_plant`), eggs (`egg_eggkid`, `cell_eggmom`,
`cell_eggdad`) and unlock pickups (`cell_unlock_proboscis`, `cell_unlock_jet`, …).
109/189 resolve to names from the registry; the rest are internal variants.

## Loot tables — `loottable` (48)

Defined by `docs/LOOTTABLE_FORMAT.md`. They are the **part-unlock / drop tables**: each
entry is `{type, cellID, tableID, weight(float), count, countDelta, levelOffset}`.

* `loot_booster` → `cell_unlock_mandible` (w 90), `cell_unlock_jet` (w 10)
* `loot_bonesb` → mandible/spike (5), jet/chemical (1), cilia/proboscis (0.2), electric (0.1)
* `loot_grubby`, `loot_squid`, `loot_chomper`, `loot_jawhead`, … one per archetype
* biome/rock tables: `loot_water`, `loot_beach`, `loot_ice`, `loot_rock`, `loot_plant`,
  `loot_random`, `loot_ice_chunk`, `loot_rock_chunk`, `solid_pieces`
* `loot_unlocks` (all part unlocks), `loot_placeholder`

## Look tables — `look_table` (9)

Steering weights (`docs/LOOKTABLE_FORMAT.md`), one per behaviour:
`NPC_idle`, `npc_moving`, `npc_mad`, `npc_scared`, `npc_move_scared`, `player_idle`,
`player_moving`, `look_up`, `default_look_table`. Entry types:
`up/forward/left/right/pursue_target/flee_target/object/food/predator/destination/cursor/player`.

## Spawn model — `populate` + `random_creature`

These define **what cells appear in a level** — the direct input a playable stage needs.

`populate` (23 resources, one per level: `level_water`, `level_plant`, `level_ice`,
`level_rock`, `level_beach`, `level_random` and `_2`/`_bg` variants) is a 16-byte header
`{type, ?, entryCount, 16}` followed by `entryCount` **64-byte entries**; each entry names a
cell archetype id at `+0x18` and a spawn **count** (f32) at `+0x2C`:

```
level_water  entries: cell_food_L0 x5, 0x5a5c0f03 x8, 0x5a5c0f01 x20, 0x5a5c0f00 x30,
                      0x5a5c0f03 x40, ...
level_plant  entries: cell_food_L0 x5, 0x6316609e x8, 0x63166099 x16, 0x63166098 x24, ...
level_water_bg entries: cell_jawhead x50, cell_puffish x200, cell_megamouth x300
```

`random_creature` (10) is an id/weight list; `neutralCells` names
`NeutralCell_01/02/03` at weight 1.0.

The unnamed ids (`0x5a5c0f00..03`, `0x63166098..9e`, `0x0e09bb72..75`, `0x0feb4d91..96`,
`0x981a536c`) are real `cell` archetypes with **no display name and no registry name**.
They come in **consecutive runs** (`…0F00/01/02/03`), so — like the cell-save field ids — they
are **assigned ids, not FNV hashes** (FNV is not linear, so a suffix change cannot produce
consecutive hashes). They are the food/plant/bubble prop tiers; their names are not recoverable
from the shipped data.

## World biomes — `world` (13)

`world_water`, `world_beach`, `world_ice`, `world_rock`, `world_plant`, `world_random`
plus `_bg` background variants and `cell_empty`. Each holds the tide-pool layer setup
(64 B record: layer counts + `3.5` depth scalar).

## Animation table — `xls` (OLE2/Excel)

A real Excel workbook (`D0CF11E0`) with one populated sheet (74 rows). Columns:
`ANIM_EVENT | clip name | movement style | speed/flag`. Rows map cell animation events to
clips, e.g. `ANIM_MOVE → Cell_mov_flg (1.0)`, `ANIM_EAT_JAW → Cell_eat_mand`, `ANIM_ZAP →
Cell_elec_release`, `ANIM_GROW → Cell_grow`. This is the cell animation state machine
(`Cell_mov_cil`/`Cell_mov_flg`/`Cell_move_jet` for cilia/flagella/jet movement).

## Data model

```
CellGame.package
├── cell            189  archetypes  (player parts, enemies, food, eggs, unlocks)
├── loottable        48  unlock/drop tables  → cell_unlock_* parts
├── look_table        9  steering weights per behaviour
├── world            13  biomes (water/beach/ice/rock/plant/random)
├── populate         23  per-level spawn lists (cell archetype id + count)
├── random_creature  10  random spawn tables (NeutralCell_01..03)
├── structure       144  part layout records
├── advect           28  flow maps
├── prop              1  cell_properties   (17 named tuning curves)
├── globals           1  cellgame          (raw scalar tuning)
└── xls               1  animation table    (74 rows)
```

## Boundaries

* Per-archetype **stats** (HP, damage, social) are not in these records as named fields;
  the 400-byte archetype payload and the 80-byte `structure` records are positional
  (offset-based), not keyed. See `analysis/cell-save.md` for the save-side field ids.
* The save-field ids in `analysis/audit/cell-field-descriptors.json` remain structural;
  the FNV registry does **not** contain them (they look like per-class sequential
  property indices, not name hashes).
* `xls` decoding needs `xlrd` (analysis-time only); the tool degrades gracefully.

## Reproduce

```powershell
python tools/spore_hash.py cell structure look_table loottable
python tools/cell_game.py                 # type/resource summary
python tools/cell_game.py --globals        # named cell-globals tuning curves
python tools/cell_game.py --json analysis/audit/cell-game.json
```
