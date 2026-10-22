# Playable stage drivers — the Cell stage (playable)

> **Current verdict, 2026-10-22:** the five playable-stage rows (mode/tag/registrar/object
> size/state/next) are held in `source/engine/game_stages.cpp`, cross-checked against the
> binary and the shipped script. `Game_Cell` owns the first real driver attached to the
> `CellGame` state: on enter it loads `Data/CellGame.package`, spawns a level's cells from the
> decoded `populate` table (see [cell-game](cell-game.md)), and **simulates and draws a
> playable tide pool** — you steer a hero cell with the mouse, eat food and smaller cells to
> grow, flee and get bitten by larger ones, lose HP and respawn, and evolve at size 3.0. The
> other four stages parse but have no behaviour; their states keep the honest "mode NOT
> EXECUTED" placeholder.
>
> **What is now the game's data (2026-10-22).** The cell archetype record's field names and
> byte offsets were recovered from the GA binary (`FUN_01375e30` / the `cAIData` descriptor
> at `0x1374725`): the per-cell AI block at record `+0xE0` carries `speed`, `chaseSpeed`,
> `wanderSpeed`, `fleeSpeed`, `awarenessRadius{,Food,Predator}`, `flocking`, `turnFactor`,
> `protectRadius`, `food`, `growCount`, `growAmount`, `hatchDuration`, `chaseTime`, … and
> the scalars at `+0xA8..+0xDC` carry `hp`, `flags`, `cellType`, `unlockType`, `loot`,
> `poison`, …. The engine reads them **by name** now, so the simulation uses the shipped
> numbers for the fields it consumes (Ducky: speed 11 / chase 8; hero: food 10000 /
> growAmount 0.275). The 17 shipped tuning curves are decoded from the
> `prop` resource and keyed by property name; the simulation **consumes 9** of them
> (`time_dilation`, `camera_lookahead`, `camera_lerpfactor`, `mouse_zoom`,
> `awareness_dilation`, `anim_dilation`, `poison_count`, `electric_jumps_count`,
> `electric_range_count`). The other 8 - `evo_points`, `npc_turn_factor`, `eye_count`,
> `jet_count`, `cilia_count`, `cilia_graph`, `flagella_count`,
> `electric_recharge_count` - are decoded but not consumed yet: their consumers were not
> recovered, so the engine still uses the archetype's `turnFactor` and a hardcoded
> evolve threshold.
>
> **Honest scope.** The spawn list, the body structures, the per-cell AI/stat fields and the
> tuning curves are the game's data. Still ours: entity **positions**, the **arena**, the
> camera framing, the body **radius** (the archetype's size field is outside the shipped
> record), the HP **formula** (the archetype's `hp` is a tier, the size→HP progression was not
> found in the binary), the DNA costs, and the **particle/audio** presentation. The scene is
> drawn with the real `gmdl` cell bodies (top-down), not the original's RenderWare tide-pool
> scene, and there is no game audio.

## Evidence

* Registration calls (GA oracle, see [modes](modes.md)): `Game_Cell` tag `0x01654C00`
  from `0x00e616c0` (obj `0x0c`), `Game_Creature` `0x01654C01`/`0x00d3bd20` (`0xe8`),
  `Game_Tribe` `0x01654C02`/`0x00cdb4e0` (`0x3a8`), `Game_Civ` `0x01654C04`/`0x00cfa480`
  (`0x150`), `Game_Space` `0x01654C05`/`0x00fddb30` (`0x198`).
* Stage/state pairing and progression (`CellGame` → `CreatureGame` → `TribeGame` →
  `CivilisationGame` → `SpaceGame`): the shipped `SporeAppStates` trigger, extracted
  verbatim to [scripts/SporeAppStates.txt](scripts/SporeAppStates.txt). See
  [states](states.md). `CellGame`'s own block is only `mode Game_Cell` — no `uilayout`,
  no messages.
* The mode→state mapper `FUN_00b5dcd0` switches on the same tags ([modes](modes.md)),
  so the table joins three agreeing artifacts (registrations, script, mapper).
* The world's spawn input is the decoded `populate` resource (per-level `cell` archetype id
  at `+0x18` and spawn count at `+0x2C`) and each archetype's `structure` body model
  (archetype `+0x000` template id == structure id) — both from `CellGame.package`
  ([cell-game](cell-game.md)).

## What the driver does

* `CellGame` entry: `cell_enter()` loads `Data/CellGame.package` (via `os::fs`), spawns
  `level_water` (160 cells: 5+3+… per the shipped spawn counts) and logs
  ``cell world: level_water: 160 cells, player=cell_hero_L0``. Every other stage mode keeps
  the "driver not implemented" warning.
* Each frame: `on_tick` → `cell_tick(dt)` runs the simulation; `on_render` →
  `states/cellpresent.cpp` draws the world and steers the player from the pointer.
* `CellGame` exit: `cell_exit()` clears the world and parks back to idle.
* `has_driver()` is true for `Game_Cell` only, matched by mode NAME (the script's
  namespace), never by tag address.

### Simulation (`engine/cell_world.{h,cpp}`, D3D-free, deterministic)

* **Spawn** (`set_level`): one `Entity` per unit of each `populate` entry's count; structure
  resolved from the archetype's `+0x000` template id. Player = the `cell_hero_L0` archetype
  (found by id), else `cell_minno1`, else the first cell.
* **Roles**: a cell whose name contains food/flora/plant/clump/egg is `Food` (always
  edible); everything else is a `Creature` (prey when smaller than the player, predator when
  larger). The level's **`_bg` population** is scenery: it spawns as `Role::Prop`, so it drifts
  but never hunts, never is eaten, and never eats the entity budget. That covers the whole
  background layer, **including its food**: `level_random_bg` lists 113 `cell_food_L0`
  (background plankton) next to 1130 unnamed props, and they are scenery too - the
  background is a separate, non-interactive scene. The foreground levels keep their own
  food edible.
* **Sizes and AI** come from the archetype's `cAIData` block at record `+0xE0`. What the
  simulation actually reads: `speed` / `chaseSpeed` / `fleeSpeed` (the archetype's speed values -
  the GA binary scales them by `npcSpeedMultiplier`, which is 1.0, and no per-mode base speed was
  recovered, so the engine uses the shipped value directly as a world-unit speed; the shipped
  range is 0.2 (a nearly stationary "booster") to 45 (a darting small creature), both plausible
  against the player's 24 s·⁻¹ seed. `chaseSpeed` drives a predator and `speed` a normal move;
  the shipped `fleeSpeed` is 0 in 188 of the 189 archetypes, so it cannot be the flee speed and
  `speed` is the fallback), `awarenessRadiusFood` / `awarenessRadiusPredator` (the detection
  radii), `turnFactor` (the steering weight - an INTEGER, not a float), `flocking` (the cohesion
  weight), `protectTime` (the post-hit invulnerability window - it is read from the archetype but
  is **0 in all 189 shipped archetypes**, so the documented 0.5 s seed is what actually runs) and
  `hatchDuration` (the egg's hatch time). Decoded but **not** used by the simulation: `food`, `growCount`, `growAmount`,
  `protectRadius`, `wanderSpeed`, `awarenessRadius` (0 in every shipped archetype), `chaseTime`,
  the digestion trio, the sleep/awake times, the recharge timers and the `fears*` flags. The
  block is **mixed-type**: the integer fields were measured over all 189 shipped archetypes, and
  reading one as a float yields a denormal (turnFactor 3 as f32 = 4.2e-45) that silently
  disables the steering. See
  [cell-game](cell-game.md#the-archetype-record-layout-recovered-from-the-ga-binary).
* **Body radius is a seed, not shipped data.** The archetype's real size field (`size` @ record
  `+0x304`) lies outside the shipped 400-byte record, so no size ships with the data. `growAmount`
  is a GROWTH amount, not a size. The engine therefore derives the radius from a hash of the
  archetype id (documented seed). The game's real rule is a discrete size CLASS: a threshold
  table (0, 50, 100, 150, 200, 250, 300 …, with `size == 1000 → 10`) maps a raw size to a class,
  and the predator/prey relation is `clamp(class_predator - class_player + 2, 0, 4)`; the engine's
  size-ratio test is a seed approximation of that rule.
* **Player**: accelerates toward the world-space pointer target (mouse input), with drag and a
  speed cap. Eating (within the sum of the radii, edible) grows it by area; a larger creature
  in contact drains HP; at 0 HP the player dies and respawns at the arena centre after 1.5 s;
  at size 3.0 it is "ready to evolve" (the Creature stage is not implemented).
* **AI**: a creature bigger than the player is a predator and chases it; a smaller one is prey
  and flees. Both use the archetype's own chase/flee speed, the `npc_mad` / `npc_scared` look
  tables, the archetype's `turnFactor` and its awareness radius. Same-archetype neighbours
  flock together when the archetype's `flocking` weight is non-zero. (This replaced the old
  test that read the **speed** slot to decide "predator", which made every moving cell a
  predator and left the flee branch dead.)
* **Damage**: the player's bite damages the creature it overlaps, so NPCs have HP and can die
  (they drop a meat chunk and their loot); the bite amount is a documented seed.
* **Camera**: follows the player; zoom scales with size so the whole arena stays legible.
* The RNG is seeded from the level name, so the same data always produces the same world.
* `tools/cell_world_verify.cpp` unit-tests it all offline (steering, camera, eating/growth,
  predator death, respawn, determinism, unknown-level error, cleared-world tick drop, the
  save round-trip, the `_bg` scenery role, and — with a real package — the archetype's
  `hp`/`growAmount`/`food` values).

### Presentation (`source/states/cellpresent.cpp`)

* Top-down follow camera. **Real cell-body geometry**: a cell's `structure` base-body id
  (`header +0x40`, e.g. `Booster`/`Squid`) is *also* the instance id of a `gmdl` model shipped in
  the game packages (28 of the tide-pool bodies carry one in `Spore_Content.package`, each with a
  paired `raster` texture — 865/865). Loaded once by base id and drawn top-down through
  `render::Mesh::draw_at()` (one `set_topdown_camera()` per frame, one WORLD matrix per cell,
  depth buffer separating them), each with its own texture.

* **The geometry load is the frame cost.** The shipped bodies are whole cell models
  (about 6,300 vertices and 31,000 indices each), and `level_water` spawns 714 of
  them - about 3.9M vertices and 18.7M indices per frame. With the device's
  **software** vertex processing that frame took over a second (the stage ran at
  0.9 fps); the device now asks for **hardware** vertex processing and falls back
  to software when the adapter refuses it, which puts the stage at ~99 fps on the
  same machine. The draw count itself is the game's own: `level_water` spawns 163
  foreground cells and 550 background ones, exactly as the shipped `populate`
  records say.
* The player is white with a cyan ring and a heading notch; predators carry a red ring; cells with
  no gmdl body (the procedural `amoeba_cell`, flora/rock props) keep a tinted-disc fallback, so
  nothing disappears. Food is green.
* **Backdrop from the game's own Layers data:** the 12 `backgroundMap` depth bands are painted as
  a vertical colour gradient, and a `density` field of `CellGame_Layers.package` (e.g.
  `Level1-zone1`) is uploaded as a 128x128 texture and drawn over the arena, drifting along that
  layer's mean `advect` (flow) direction. Which density/flow layer a level uses is our choice by
  name; the field contents are the game's.
* HUD: level, cell/eaten counts, size and time, HP bar, GROW bar (evolve at 3.00), a death /
  evolve banner, and a control hint.
* **Progression:** once evolved (size 3.0) a click advances to the `next` state of the shipped
  `SporeAppStates` chain (`CellGame` -> `CreatureGame`). The Creature driver is not implemented,
  so that state shows its honest placeholder — this is the real progression link, not a fake
  transition.

## Boundaries

* No sprite-atlas or RenderWare tide-pool *scene* rendering (we draw the decoded gmdl bodies, not
  the original's RenderWare world), and no game audio (the sound is a `Beep()` stand-in). The
  overlay/HUD is our own immediate-mode 2D.
* Entity **positions**, the **arena**, the camera framing, the **HP formula** (the archetype's
  `hp` is a tier; no size→HP table exists in the binary), the DNA **costs** and the bite damage
  are still **our seeds**. The original's placement and player-creation are not recovered.
* The archetype fields beyond the shipped 400-byte record (`ai_hard`, `ai_easy`, `size`, `eat`,
  `friendGroup`, …) are named in the binary but not present in the data, so the engine reads only
  the default `ai` block.
* The **level progression** comes from the shipped `world` records: every world lists the level
  populations it hosts, and the `world_ice` record lists all ten campaign levels in order —
  `level_ice, level_ice_2, level_rock, level_rock_2, level_plant, level_plant_2, level_water,
  level_water_2, level_beach, level_beach_2` — followed by the randomised `level_random`. The
  globals record's world references give the order of the worlds; the slot `flag` separates the
  playable worlds from the `_bg` ones (scenery only). The campaign order is therefore confirmed
  by two independent sources: the globals' reference order and the world records' slot list.
  The old implementation synthesised the `_2` levels by string concatenation and never reached
  `level_beach_2`.
* The DNA a killed cell is worth is still a seed (`kPlantDna`/`kMeatDna`/`kPreyDna`); the
  archetype's `food` value is decoded but the original's food→DNA formula was not recovered.
* No `CreatureGame`/`TribeGame`/`CivilisationGame`/`SpaceGame` behaviour; no editor
  drivers; no `FUN_00b5dcd0` tag dispatch in the runtime.
* Host policies: case-insensitive name match (same rule as `appmodes`), `u64` tick
  counter, idle/active/reset lifecycle, level matched by resolved name or its own FNV-1 id.
  None of these are GA behaviour claims.

## Reproduce

```powershell
cmake --build build --config Debug --target game_stages_verify cell_world_verify cell_game_verify
.\build\Debug\game_stages_verify.exe
.\build\Debug\cell_world_verify.exe "S:\Games\Other\Spore\Data\CellGame.package"
.\build\Debug\cell_game_verify.exe "S:\Games\Other\Spore\Data\CellGame.package"
python tools/tests/test_game_stages_table.py  # cross-checks the table against modes.md + script
# the live stage (the app lives in build\bin\Debug\, next to its Data\ tree):
.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:CellGame -quitAfter 140 -capture cell.bmp -captureAfter 120
```

> The verification tools are built into `build\bin\Debug\` and **mirrored into `build\Debug\`**,
> so the paths above always run the fresh binary. (Before the mirror existed, `build\Debug\`
> held stale executables from an older output layout, so the documented commands silently
> verified the wrong build.)

`game_stages_verify` checks table transcription, case-insensitive/unknown lookups, the
`next` chain, driver coverage (Cell only), the idle/active/exit lifecycle and the offline
"no world data" path. `cell_world_verify` checks spawning (5+3 cells from a constructed
package), structure resolution, food/creature roles, the player fallback, steering + camera,
eating/growth, predator death + respawn, deterministic re-spawn, the unknown-level error and
the cleared-world tick drop; with a real package it prints the spawned level. The runtime smoke
must exit 0 with `cell world: level_water:` in `OpenSpore.log` and a frame showing the tide pool.
