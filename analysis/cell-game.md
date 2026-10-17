# Cell-stage game data (`CellGame.package`)

> **Verdict, 2026-10-17:** the cell mini-game (`Game_Cell`, state `CellGame`) is defined
> **entirely by data packages** — its archetypes, globals, spawn/biome/loot tables,
> body models, background fields and animation list all ship in `Data/CellGame.package`
> (442 resources) and `Data/CellGame_Layers.package` (33). Resource types and instance
> names are 32-bit **FNV-1** hashes of lower-cased names, so they are fully recoverable.
> **Every resource type in both packages is now decoded at the structural level** — only the
> positional semantics of a few raw float runs (archetype stats, `globals`, `effectMap`) and
> the cell-save field ids remain unnamed (names not present in the shipped data).
> This note is the data model for the cell stage; the runtime skeleton is in
> `analysis/game-stages.md`.

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
| `0x5C74D18B` | `density` | 5 | density field (Layers pkg) |
| `0xDBA35AE2` | `look_algorithm` | 1 | id→value transition table (232 B) |
| `0x433FB70C` | `effectMap` | 1 | effect map |
| `0x00B1B104` | `prop` | 1 | **cell globals** table |
| `0x612B3191` | `backgroundMap` | 1 | background depth bands |
| `0x1F639D98` | `xls` | 1 | **animation table** (OLE2/Excel) |

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

Plus `globals`/`cellgame` (196 B raw struct) holding more scalar tuning — see the
**Cell globals** section below.

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
plus `_bg` background variants and `cell_empty`. Each record starts with an entry **count**
(`world_water` = 2, `world_ice` = 20, `cell_empty` = 0) followed by that many entries; two
record shapes occur (24-byte `{id, 1, idx, id, 0, idx}` region point, and 28-byte
`{idx, k, f, 0, 1.0, id, x, y}` layer/spawn row, e.g. `world_ice` rows ending `… 3.5 0 1.0`).
Sizes 16/28/64/136/544. The 64-byte `world_water` ends `… 3.5, 0, 1.0`. The entry lists are
positional; only the count is treated as a stable field.

## Structures — `structure` (144)

A cell **body model**: an **80-byte header** followed by `N` **32-byte part entries**
(112 B = 80 + 1 part, 144 B = 80 + 2 parts). Header fields (Little-endian):

| off | field |
|---|---|
| `+0x0C` | format marker `0x46748976` (present on 62/144) |
| `+0x10` | **death archetype** id (spawned when the cell dies: `cell_deathNPC`, `cell_creature_L0_food_death`) |
| `+0x28` | const `0x00000030` (48, on all 144) |
| `+0x40` | **base body** id (`amoeba_cell`, `hummingbird`, `Poisoner`) |
| `+0x44` | base scale xyz (f32 ×3) |

Part entry:

| off | field |
|---|---|
| `+0x00` | parent index (`0xFFFFFFFF` = root) |
| `+0x04` | flag (0/1) |
| `+0x10` | **part id** hash (`CG_poison_L1`, `cell_hero_L0_decorations`, `cell_creature_L0_food_decorations`, `cell_attack_poison_aura`) |
| `+0x14` | part scale xyz (f32 ×3) |

## Background map — `backgroundMap` (1)

An 8-byte header then **12 depth bands** of `{r, g, b, depth}` floats. `depth` runs
`5, 15, 50, 150, 500, 1500, 5000, 15000, 100000` — the parallax depth thresholds; `rgb`
is the per-band tint. This is what a background renderer scrolls at different rates.

## Effect map — `effectMap` (1, "CellEffects", 596 B)

An 8-byte header `{count, ?}` then a list of effect entries, each an id followed by a run of
floats (e.g. `{id}, 5, -1, -1, 1.5, 1.9, …` or `{id}, 5700, 1.5e4, 2e4, 2.2e4`). The
float runs vary in length (particle counts / radius / lifetime bands); entries are
positional.

## Cell globals — `globals`/`cellgame` (raw struct, 196 B)

Sixteen resource-id references (`+0x00..+0x3C`) then **~31 tuning floats** from `+0x40`
(`5.0, 0.5, 0.2, 1.0, 3.0, 250.0, 50.0, 5.0, 0.5, 35.0, 0.1, 0.1, 0.5, 1.0, 1.0, 40.0,
20.0, 0.5, 0.5, 100.0, 20.0, 1.0, 2.0, 7.0, 10.0, 6.0, 3.0, 15.0, 3.0`, tail `10, 2`). The
floats have no shipped key names (unlike `cell_properties`), so they are exposed raw.

## Background fields — `CellGame_Layers.package` (33)

`advect` (28) and `density` (5) resources are raw background fields:

| type | size | shape |
|---|---:|---|
| `advect` | 196608 | **128×128×3 f32** flow/current vector per cell (`cell_river`, `RandomFlow`, `CellRandomFlow`, `cell_vortex`, `cell_wave`, `cell_line`, `default`, `CellStarryNightFlow`, …) |
| `density` | 65536 | **128×128 f32** scalar density (`Level1-zone1/2`, `Level1-bubbles`, `spiral`, `InverseSpiral`) |

These are the parallax water currents and prop-density maps the cell stage scrolls behind
the play area (`cell_advect_spirals`, `cell_advection_racetrack`, `CellMickeyFlow`, …).

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
├── structure       144  body models (80B header + N 32B parts)
├── backgroundMap     1  12 parallax depth bands {rgb, depth}
├── effectMap         1  effect id + float runs
├── look_algorithm    1  id→value transition table
├── prop              1  cell_properties   (17 named tuning curves)
├── globals           1  cellgame          (16 id refs + ~31 raw floats)
└── xls               1  animation table    (74 rows)

CellGame_Layers.package
├── advect           28  128×128×3 f32 background flow fields
└── density           5  128×128 f32 background density fields
```

## Runtime — from data to a live world

`source/engine/cell_world.{h,cpp}` turns this data into entities and is the first step that
unites the decode with the engine:

* `load_default()` resolves `Data/CellGame.package` through `os::fs` and decodes it (a missing
  package is an error, not a crash).
* `set_level("level_water")` spawns one entity per unit of each `populate` entry's count
  (160 cells for `level_water`), resolving each archetype's body model via `structure_of()`
  (archetype `+0x000` template id == structure id).
* The player is the shipped hero archetype `cell_hero_L0` (else `cell_minno1`, else the first
  cell), found by its FNV-1 id so it resolves without the name registry.
* `tick(dt)` runs a small food chain: a hash-stable size and a name-derived role (`Food` vs
  `Creature`) per entity, mouse steering for the player, predator/prey AI, eating/growth, HP,
  death/respawn, an evolve threshold and a follow camera. **Positions, sizes, role split,
  growth/HP/aggression rates and the camera are OUR seeds** (the archetype stat slots are
  positional and unnamed) — the shipped spawn list and body structures are the game's data.
* `game_stages.cpp` drives it (`cell_enter`/`cell_tick`/`cell_exit`) and
  `source/states/cellpresent.cpp` draws it (tinted discs + HUD) and steers from the pointer.
  The presenter also loads `Data/CellGame_Layers.package` (the `density`/`advect` fields) and
  `backgroundMap` depth bands to paint a data-driven tide-pool backdrop.
  See [game-stages](game-stages.md).

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
build\Debug\cell_game_verify.exe "S:\Games\Other\Spore\Data\CellGame.package"
build\Debug\cell_game_verify.exe "S:\Games\Other\Spore\Data\CellGame_Layers.package"
```
