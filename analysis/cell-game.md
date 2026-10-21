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

> **Engine note (fixed).** `engine/cell_game.cpp` used to read this resource as a flat
> run of `(x, y)` pairs and key it by the *resource* instance, so every
> `World::curve_value()` lookup fell back to a hardcoded default and all 17 shipped
> curves were inert. The real layout is `u32 BE entry_count` then, per entry,
> `{u32 BE key, u32 BE type, u32 BE item_count, u32 BE item_size}` followed by that
> much item data (the float2 points are little-endian). The decoder now walks that
> table and keys each curve by its **property name**, resolved from the hashed
> property id through `reg_property.txt` (a new `naming::property_name()`).
> `cell_game_verify` now has a case that decodes a synthetic property table and
> asserts the curve is keyed `time_dilation`; with the real package the decoder
> reports **17 property curves**.

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

### The archetype record layout (recovered from the GA binary)

The record is **not** an opaque blob: the GA `cell` type descriptor
(`FUN_01375e30` @ `0x1375e30`, registered by the thunk at `0x1375df0` with the type
name `"cell"`) names every field and its **byte offset**. The descriptor is 26
entries of 40 bytes (`{char* name; u32 nameHash; u32 offset; u32 flags; ...}`) and
its offsets are 4-byte-strided, so they are struct byte offsets, not property ids.

Top-level fields (all inside the shipped 400-byte record):

| offset | field | observed |
|---|---|---|
| `+0x000` | `structure` (template id) | `cell_hero_L0` → `0xCBCD1287` |
| `+0x004` | UTF-16LE display name | `"Snorf"`, `"Chomper"`, `"Squirty"` |
| `+0x0A4` | part count (u16) | 0..25 |
| `+0x0A8` | `hp` (tier) | hero 3, squid 2, Ducky 2 |
| `+0x0AC` | `fixedOrientation` | 0 |
| `+0x0B0` | `flags` | 1..4 |
| `+0x0B4` | `cellType` | 0 |
| `+0x0B8` | `unlockType` | 0..4 |
| `+0x0BC` | `density` | 0 |
| `+0x0C0` | `sound` | 0 |
| `+0x0C4` | `break` | effect id |
| `+0x0C8` | `pieces` | 0 |
| `+0x0CC` | `leak` | 0 |
| `+0x0D0` | `expel` | effect id |
| `+0x0D4` | `loot` | loot-table id |
| `+0x0D8` | `explosionTable` | effect id |
| `+0x0DC` | `poison` | effect id (0x1000 / 0x1006) |
| `+0x0E0` | `ai` — a **`cAIData`** block (0xb4 B) | see below |

The descriptor also names `ai_hard` @ `+0x194`, `ai_easy` @ `+0x248`,
`friendGroup` @ `+0x2FC`, `wontAttackPlayer` @ `+0x300`,
`wontAttackPlayerWhenSmall` @ `+0x301`, `size` @ `+0x304`, `eat` @ `+0x30C` and
`triggersEscapeMission` @ `+0x318`, and the registered record size is **0x31C
(796 B)** — larger than the 400 B the shipped resources carry, so only the
`ai` block and the scalars above are readable from the shipped data.

### `cAIData` — the per-cell AI block (record `+0xE0`)

The `cAIData` type (registered at `0x13746e0`, descriptor built at `0x1374725`,
size 0xb4) names its fields in this order. Offsets are relative to the block
base; the absolute offset adds the block base (`+0xE0` for the default `ai`):

| off | field | abs | observed (Cell_DuckyMed) |
|---|---|---|---|
| `+0x04` | `awarenessRadius` | `+0xE4` | 0 |
| `+0x08` | `awarenessRadiusFood` | `+0xE8` | 5 |
| `+0x0C` | `awarenessRadiusPredator` | `+0xEC` | 0 |
| `+0x10` | `movementStyle` | `+0xF0` | 0 |
| `+0x14` | `flocking` | `+0xF4` | 1 |
| `+0x18` | `speed` | `+0xF8` | 11 |
| `+0x1C` | `chaseSpeed` | `+0xFC` | 8 |
| `+0x20` | `wanderSpeed` | `+0x100` | 15 |
| `+0x24` | `fleeSpeed` | `+0x104` | 0 |
| `+0x28` | `fearsNearbyDamageRadius` | `+0x108` | 0 |
| `+0x2C` | `fearsNearbyDeathRadius` | `+0x10C` | 0 |
| `+0x30` | `fearsNearbyDamageTime` | `+0x110` | 0 |
| `+0x34` | `fearsNearbyDeathTime` | `+0x114` | 1 |
| `+0x38` | `protectRadius` | `+0x118` | 3 |
| `+0x3C` | `protectTime` | `+0x11C` | 0 |
| `+0x40` | `turnFactor` | `+0x120` | 0 |
| `+0x44` | `axialMovement` | `+0x124` | 0 |
| `+0x48` | `spawnTime` | `+0x128` | 0 |
| `+0x4C` | `spawnRestTime` | `+0x12C` | 0 |
| `+0x50` | `spawnOutput` | `+0x130` | 0 |
| `+0x54` | `arcLength` | `+0x134` | 0 |
| `+0x58` | `arcLengthSecondary` | `+0x138` | 4 |
| `+0x5C` | `numArcs` | `+0x13C` | 0 |
| `+0x60` | `keyTransformation` | `+0x140` | 2 |
| `+0x64` | `keyProjectile` | `+0x144` | 0 |
| `+0x68` | `food` | `+0x148` | 180 |
| `+0x6C` | `growCount` | `+0x14C` | 1 |
| `+0x70` | `digestionCount` | `+0x150` | 0 |
| `+0x74` | `digestionTime` | `+0x154` | 0 |
| `+0x78` | `digestionOutput` | `+0x158` | 0 |
| `+0x7C` | `fleeTime` | `+0x15C` | 0 |
| `+0x80` | `fleeRestTime` | `+0x160` | 0 |
| `+0x84` | `chaseTime` | `+0x164` | 10 |
| `+0x88` | `chaseRestTime` | `+0x168` | 0 |
| `+0x8C` | `chasesDamage` | `+0x16C` | u8 |
| `+0x8D` | `fearsMouths` | `+0x16D` | u8 |
| `+0x8E` | `fearsWeapons` | `+0x16E` | u8 |
| `+0x8F` | `fearsDamage` | `+0x16F` | u8 |
| `+0x90` | `fearsElectric` | `+0x170` | u8 |
| `+0x91` | `fearsPoison` | `+0x171` | u8 |
| `+0x92` | `ignoresFood` | `+0x172` | u8 |
| `+0x94` | `awakeTime` | `+0x174` | 0 |
| `+0x98` | `sleepTime` | `+0x178` | 0 |
| `+0x9C` | `growAmount` | `+0x17C` | 0.4 |
| `+0xA0` | `hatchDuration` | `+0x180` | 0.4 |
| `+0xA4` | `poisonRecharge` | `+0x184` | 0 |
| `+0xA8` | `electricRecharge` | `+0x188` | 0 |
| `+0xAC` | `electricRechargeVsSmall` | `+0x18C` | 0 |
| `+0xB0` | `electricDischarge` | `+0x190` | 0 |

Cross-checks on the shipped records: `cell_hero_L0` → `food` 10000, `growAmount`
0.275, `chaseTime` 10, `protectRadius` 3; `cell_squid` → `food` 6, `growCount` 4,
`speed` 4, `chaseSpeed` 2, `growAmount` 0.26; `cell_poisonerMed` → `food` 4,
`speed` 20, `chaseSpeed` 15, `wanderSpeed` 20, `growAmount` 0.3.
These are the **game's** numbers, and `engine/cell_world.cpp` now reads the AI
block and the scalars by these names instead of guessing offsets.

The old `kStatOffsets` list in `engine/cell_game.h` is superseded: it read the
`+0x0E0` block with the wrong assumption that every slot was a float, which is
why `tools/decode_cell_stats.py` printed garbage (that tool also called
`refpack.decode` with three arguments; the signature is `decode(data, expect)`).
`tools/dump_cell_stats.cpp` now prints the named fields.

## Loot tables — `loottable` (48)

Defined by `docs/LOOTTABLE_FORMAT.md`. They are the **part-unlock / drop tables**: each
entry is `{type, cellID, tableID, weight(float), count, countDelta, levelOffset}`.

* `loot_booster` → `cell_unlock_mandible` (w 90), `cell_unlock_jet` (w 10)
* `loot_bonesb` → mandible/spike (5), jet/chemical (1), cilia/proboscis (0.2), electric (0.1)
* `loot_grubby`, `loot_squid`, `loot_chomper`, `loot_jawhead`, … one per archetype
* biome/rock tables: `loot_water`, `loot_beach`, `loot_ice`, `loot_rock`, `loot_plant`,
  `loot_random`, `loot_ice_chunk`, `loot_rock_chunk`, `solid_pieces`
* `loot_unlocks` (all part unlocks), `loot_placeholder`

The table header also carries `mustHavePart` (record `+0x1c`, an int32 the docs describe as a
flag): a table with it set only drops when the player already has a part equipped. In the
shipped package exactly one table sets it — `loot_random`. The engine decodes the field and
honours it as a drop gate.

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
| `+0x44` | base scale xyz (f32 ×3) — observed `(0,0,0)` in every shipped record, so it is **not** a usable size source |

The base body id doubles as a `gmdl` **model instance id** in the content packages (`Squid`,
`Pinky`, `Booster`, ...): 28 of the tide-pool bodies carry a `gmdl` in `Spore_Content.package`, and
every `gmdl` there has a paired `raster` (865/865). That is how the runtime draws real cell
geometry. A structure whose base body is **0** (the hero `cell_hero_L0`, and the generic cells)
has no single body model; its header instead references the `neutralCells` random table at `+0x3C`
(`0x9A391385` for the hero), so the runtime draws it with one of the `NeutralCell*` models from
`PreBaked.package` (the `neutralCells` table names `NeutralCell`, `NeutralCell_01/02/03`, all four
shipped as `gmdl` there). Bodies with neither a base gmdl nor a neutral table (the food/flora/rock
props - Spore renders those as effect motes, not models) keep the disc fallback.

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

**The references ARE named** (they resolve through the registry):

| ref | id | name | ref | id | name |
|---|---|---|---|---|---|
| 0 | `0x8393271F` | `world_ice` | 8 | `0x5F180E4F` | `world_water_bg` |
| 1 | `0x3AAF1035` | `world_rock` | 9 | `0x664987EB` | `world_beach_bg` |
| 2 | `0x81A666D9` | `world_plant` | 10 | `0x6F95F97B` | `world_random` |
| 3 | `0x17BE63FB` | `world_water` | 11 | `0x29C95ACF` | `world_random_bg` |
| 4 | `0xDB9066CF` | `world_beach` | 12 | `0xCBCD1287` | `cell_hero_L0` (the player) |
| 5 | `0x33BEB73B` | `world_ice_bg` | 13 | `0xB716C401` | (unresolved) |
| 6 | `0x06A14891` | `world_rock_bg` | 14 | `0x9D14A758` | (unresolved) |
| 7 | `0xC30611CD` | `world_plant_bg` | 15 | `0xAACC3A75` | `CellEffects` (the effect map) |

So refs 0-11 are the **shipped biome list** (five playable biomes, their `_bg` variants, then
`random`), ref 12 is the player archetype and ref 15 is the effect map. The engine now reads the
biome list from here (`World::level_list()`) and walks it for the level progression
(`World::next_level()`), instead of guessing with a `name + "_2"` string hack. The list *order* is
the record's order; that it is also the **campaign** order is not independently confirmed.

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
  death/respawn, an evolve threshold and a follow camera. Eating splits into a **diet**: plankton
  (`Role::Food`) is herbivore, flesh (`Role::Creature`/`Role::Meat`) is carnivore, and a diet that
  mixes both reads omnivore. Each meal awards **DNA** (plant 1, chunk 2, live prey 3), and eating a
  creature **drops a `Meat` chunk** where it died (`kMeatTtl` 14 s) for the carnivore loop. Grazed
  plankton is **restocked** every 3 s back to the level's shipped count (`m_target_food`), so the
  pool refills instead of dwindling. **Positions, sizes, role split, growth/HP/aggression/DNA rates,
  the meat lifetime/restock cadence and the camera are OUR seeds** (the archetype stat slots are
  positional and unnamed) — the shipped spawn list and body structures are the game's data.
* `game_stages.cpp` drives it (`cell_enter`/`cell_tick`/`cell_exit`) and
  `source/states/cellpresent.cpp` draws it (real gmdl cell bodies + HUD) and steers from the
  pointer. The presenter also loads `Data/CellGame_Layers.package` (the `density`/`advect` fields)
  and `backgroundMap` depth bands to paint a data-driven tide-pool backdrop. A structure's base
  body id (`+0x40`) doubles as a `gmdl` model instance id in the content packages, which is how
  the cells get real geometry; a base-0 structure (the hero) is drawn with a `NeutralCell*` model
  from `PreBaked.package`. See [game-stages](game-stages.md).

## Boundaries

* The archetype **field names and offsets are now recovered** (see *The archetype
  record layout* above): the `cAIData` AI block at `+0xE0` and the scalars at
  `+0xA8..+0xDC` are named and read by name in `engine/cell_world.cpp`. What is
  still **not** recovered is the *formula* each value feeds in the original
  (e.g. how `hp`/tier becomes hit points, and the size→HP progression): the
  descriptor gives the data, not the consuming arithmetic, and no size→HP table
  exists in the binary.
* The descriptor also names fields at offsets **beyond the shipped 400-byte
  record** (`ai_hard` `+0x194`, `ai_easy` `+0x248`, `friendGroup` `+0x2FC`,
  `size` `+0x304`, `eat` `+0x30C`, `triggersEscapeMission` `+0x318`; the
  registered record size is 0x31C). They are part of the runtime structure and
  are **not** in the on-disk archetype, so they stay unread.
* The 80-byte `structure` records are still positional (part parent/flag/id/scale).
* The save-field ids in `analysis/audit/cell-field-descriptors.json` remain structural;
  the FNV registry does **not** contain them (they look like per-class sequential
  property indices, not name hashes). The *runtime* creature fields, however, are
  named: `mHealthPoints`/`mMaxHealthPoints` (registration `FUN_013326c0`),
  `mFoodValue`/`mSpeciesKey`/`mAge`/`mArchetype` (`FUN_01335000`),
  `mPosition`/`mOrientation`/`mScale`/`mBoundingRadius` (`FUN_01353d90`).
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
