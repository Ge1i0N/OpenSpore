# Playable stage drivers — first gameplay step (Cell driver)

> **Current verdict, 2026-10-17:** the five playable-stage rows (mode/tag/registrar/object
> size/state/next) are held in `source/engine/game_stages.cpp`, cross-checked against the
> binary and the shipped script. `Game_Cell` owns the first real driver attached to the
> `CellGame` state: on enter it loads `Data/CellGame.package`, spawns a level's cells from the
> decoded `populate` table (see [cell-game](cell-game.md)) and advances them each frame; exit
> parks it. The other four stages parse but have no behaviour; their states keep the honest
> "mode NOT EXECUTED" placeholder.
>
> **Honest scope:** the world is a **data-driven sandbox** — the entities come from shipped
> data (archetype ids, spawn counts, body-structure ids), but their positions/velocities are
> placeholders and there is **no tide-pool rendering, camera, HUD, input or audio**. Without
> the package the driver degrades to the old tick counter (reported, not hidden).

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
* Each frame: `on_tick` → `cell_tick(dt)` integrates the cells' placeholder drift while
  active; ticks while idle are dropped; re-entry restarts.
* `CellGame` exit: `cell_exit()` clears the world and parks back to idle.
* The generic placeholder frame (`source/app.cpp`) names the driver and its live world (level,
  cell count, player) for driver-owned states instead of printing NOT EXECUTED.
* `has_driver()` is true for `Game_Cell` only, matched by mode NAME (the script's
  namespace), never by tag address.
* `engine/cell_world.{h,cpp}` is D3D-free and deterministic (RNG seeded from the level name),
  so it is unit-tested offline by `tools/cell_world_verify.cpp`.

## Boundaries

* No tide-pool scene, camera, HUD, input or audio. A capture of `-state:CellGame` shows the
  placeholder frame with the CELL DRIVER line and the world summary — that is the
  deliverable, not the original tide pool.
* Positions, velocities, arena bounds and the player choice are **seed placeholders**; the
  original's placement and player-creation are not recovered. Rendering the cells (meshes/
  sprites) is the next step.
* No `CreatureGame`/`TribeGame`/`CivilisationGame`/`SpaceGame` behaviour; no editor
  drivers; no `FUN_00b5dcd0` tag dispatch in the runtime.
* Host policies: case-insensitive name match (same rule as `appmodes`), `u64` tick
  counter, idle/active/reset lifecycle, level matched by resolved name or its own FNV-1 id.
  None of these are GA behaviour claims.

## Reproduce

```powershell
cmake --build build --config Debug --target game_stages_verify cell_world_verify
.\build\Debug\game_stages_verify.exe
.\build\Debug\cell_world_verify.exe "S:\Games\Other\Spore\Data\CellGame.package"
python tools/tests/test_game_stages_table.py  # cross-checks the table against modes.md + script
.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:CellGame -quitAfter 45 -capture cell.bmp -captureAfter 30
```

`game_stages_verify` checks table transcription, case-insensitive/unknown lookups, the
`next` chain, driver coverage (Cell only), the idle/active/exit lifecycle and the offline
"no world data" path. `cell_world_verify` checks spawning (5+3 cells from a constructed
package), structure resolution, the player fallback, deterministic re-spawn, the unknown-level
error and the cleared-world tick drop; with a real package it prints the spawned level. The
runtime smoke must exit 0 with `cell world: level_water:` in `OpenSpore.log` and no
aborts/asserts.
