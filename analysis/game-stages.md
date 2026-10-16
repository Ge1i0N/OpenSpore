# Playable stage drivers — first gameplay step (Cell skeleton)

> **Current verdict, 2026-10-16:** the five playable-stage rows (mode/tag/registrar/object
> size/state/next) are held in `source/engine/game_stages.cpp`, cross-checked against the
> binary and the shipped script. `Game_Cell` owns a skeleton driver attached to the `CellGame`
> state: enter parks/resets a tick counter, each frame ticks it, exit parks it. **No
> simulation, no rendering, no input, no save/load.** The other four stages parse but have
> no behaviour; their states keep the honest "mode NOT EXECUTED" placeholder.

## Evidence

* Registration calls (GA oracle, see [modes](modes.md)): `Game_Cell` tag `0x01654C00`
  from `0x00e616c0` (obj `0x0c`), `Game_Creature` `0x01654C01`/`0x00d3bd20` (`0xe8`),
  `Game_Tribe` `0x01654C02`/`0x00cdb4e0` (`0x3a8`), `Game_Civ` `0x01654C04`/`0x00cfa480`
  (`0x150`), `Game_Space` `0x01654C05`/`0x00fddb30` (`0x198`).
* Stage/state pairing and progression (`CellGame` → `CreatureGame` → `TribeGame` →
  `CivilisationGame` → `SpaceGame`): the shipped `SporeAppStates` trigger, extracted
  verbatim to [scripts/SporeAppStates.txt](scripts/SporeAppStates.txt). See
  [states](states.md). `CellGame`'s own block is only `mode Game_Cell` — no `uilayout`,
  no messages — which is why a tick-counter skeleton is the honest first step.
* The mode→state mapper `FUN_00b5dcd0` switches on the same tags ([modes](modes.md)),
  so the table joins three agreeing artifacts (registrations, script, mapper).

## What the skeleton does

* `CellGame` entry: `cell_enter()` (counter reset, active), log line
  ``CellGame: `Game_Cell` skeleton driver active`` instead of the "driver not implemented"
  warning. Every other stage mode keeps that warning.
* Each frame: `on_tick` → `cell_tick(dt)` counts one tick while active; ticks while idle
  are dropped; re-entry restarts the count.
* `CellGame` exit: `cell_exit()` parks back to idle.
* The generic placeholder frame (`source/app.cpp`) names the skeleton and its tick count
  for driver-owned states instead of printing NOT EXECUTED.
* `has_driver()` is true for `Game_Cell` only, matched by mode NAME (the script's
  namespace), never by tag address.

## Boundaries

* No tide-pool scene, creature, food, movement, camera, HUD, or audio. A capture of
  `-state:CellGame` shows the placeholder frame with the SKELETON DRIVER line — that is
  the deliverable, not a game.
* No `CreatureGame`/`TribeGame`/`CivilisationGame`/`SpaceGame` behaviour; no editor
  drivers; no `FUN_00b5dcd0` tag dispatch in the runtime.
* Host policies: case-insensitive name match (same rule as `appmodes`), `u64` tick
  counter, idle/active/reset lifecycle. None of these are GA behaviour claims.

## Reproduce

```powershell
cmake --build build --config Debug --target game_stages_verify
.\build\Debug\game_stages_verify.exe
python tools/tests/test_game_stages_table.py  # cross-checks the table against modes.md + script
.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:CellGame -quitAfter 30 -capture cell.bmp -captureAfter 20
```

`game_stages_verify` checks table transcription, case-insensitive/unknown lookups, the
`next` chain, driver coverage (Cell only) and the idle/active/exit lifecycle. The runtime
smoke must exit 0 with `skeleton driver active` in `OpenSpore.log` and no aborts/asserts.
