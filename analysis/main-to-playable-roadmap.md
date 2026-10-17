# Main → playable: current reverse-engineering route

Reviewed **2026-10-17**. This is a prioritization map, not a claim that any native service is
available at runtime. The GA-unpacked binary remains the authority; SDK contracts provide names,
method order and field hypotheses only after GA corroboration.

## Verdict

The overall route is correct:

```text
WinMain → app/run → services/state manager → GalaxyGameEntry → Play Spore
         → catalogue → star UI/camera → selected star → stage/gameplay
```

The DebugConsole work was a useful bounded calibration pass: it recovered the main native console
path, validated the SDK↔Ghidra workflow and closed the exact `Ctrl+Shift+C` branch. It is now
low priority. Continuing to name every console/message-manager virtual slot would be tactical drift
unless a later gameplay probe needs that service.

## Layer status

| Layer | Current status | Meaning for the next step |
|---|---|---|
| CRT/WinMain/message pump/D3D9 | host boot and GA static chain are established | no need to keep walking startup except for a concrete missing service |
| App services and state data | service getters and shipped state scripts are substantially mapped; `IStateManager` SDK contract is a secondary guide | map only the native transition boundary needed to enter state 2 |
| GalaxyGameEntry registration/UI | registrar, controller, `GlobalUIGGE-9`, zoom layouts and lifecycle slots are recovered | serialized primary UI is not the galaxy; do not spend more time on layout enumeration alone |
| State 2 / Play Spore | `E008B0`, name `Play Spore`, `DF9740`, `DF95A0`, `DF6010` and record layout are recovered statically | identify the outer dispatch/caller and then feed a real/explicit catalogue |
| Catalogue | saved/candidate vectors and native builders are bounded; three authorized real-save corpora already have decoded saved/starter observations | join the existing observations without falsely promoting persisted starters to a post-refill live vector |
| Camera/viewer/scene | projection and host scene geometry are implemented; native viewer/camera producer and mount remain Unknown | use `IGameModeManager` only to map the native camera seam, not to fabricate a camera |
| Selection/phase callbacks | native F8/FC and phase bodies are bounded; automatic callback/CommandID route remains open | defer exact input dispatch until a real card scene exists |
| Stage/gameplay | not implemented | only after selected-star identity and transition target are real |

## Priority order

### P0 — real catalogue input and one end-to-end card scene

1. Reuse the already validated, explicitly selected `UserData1`, `UserData` and `UserDataRonikari`
   Game0 corpora. [Real-save evidence](gge-real-save.md) and `starsdb_gge_capture` already preserve
   saved rows, all six persisted starter references, planets and individual header bindings.
   No new profile search or save request is necessary.
2. Resolve the remaining terrain/property, post-load and refill observations needed by typed
   `CatalogInput`. The diagnostic `GGE_SAVE_OBSERVATIONS_V1` is intentionally **not** a production
   `GGE_CATALOG_V1`: it must retain Unknown instead of falsely encoding an unexecuted terrain
   lookup as query failure. Persisted starter references are not the post-refill live vector.
3. Only when these input gates are established, run the existing scene/lifecycle path and validate
   a real saved card, image, name, position and reverse pick. A separately labelled diagnostic
   persisted-save view would not establish native GGE/catalogue readiness.

### P1 — one focused native state-2 pass

Use persistent Ghidra sequentially, with no broad import:

1. The handler registration is now bounded: `DE7660` creates/initializes the Simulator state
   handlers, `E008B0` installs the Play Spore vtable `0x147EF50`, `DF9620` is its state-2 enter
   slot, and `DF9740` is its state-2 refresh slot. This is direct GA vtable/data evidence.
2. The record rebuild boundary is also closed: `DF9620`, `DF9740` and `DE32B0` converge on
   `DF95A0`; it clears old per-record UI data, reads saved/candidate spans, resizes the `0xBC`
   array, emits candidates into the post-saved region, then saved entries, UI data and effects.
3. The remaining dispatch question is only the outer caller that invokes GA vtable slot `+0x38`;
   do not map it to SDK `IGameMode::OnMouseWheel` without a signature match. GA's vtable is
   shifted/specialized (`DF9620 +0x0C`, `DF85B0 +0x10`, `DF8700 +0x14`, `DF9740 +0x38`). Do not
   broaden this into a generic state-manager rewrite.
4. Keep `DF9620 → DF95A0 → DF6010 → DF8300 → event 0x080D7451` and
   `DF9740 → DF95A0 → DF6C10` as the retained native orders. The current static result is:
   `state +0x108 == 2` → show `0x0615A5FF`/GlobalUI → rebuild records → continuation.
5. Compare only the relevant SDK contracts: `IStateManager::SwitchToState` and
   `IGameModeManager::{SetActiveMode,GetCameraManager,GetViewer}`. Do not accept SDK addresses
   without GA body/vtable confirmation.

### P2 — native camera and input seam

After P0 or a caller-supplied catalogue is available, map the producer for the viewer and active
ICamera used by `DE3C20`/`DE46D0`, then the real selection callback route (`DF7BF0`/`DF7AC0`). This
has a visible payoff; exact MessageManager/Console naming does not.

### P3 — selected star to actual stage

Only once selected-star identity, save/planet data and transition target are observed should we walk
the stage-specific mode (`Creature`, `Tribe`, `Civ`, `Space`) and gameplay factories. A successful
UI transition or native return value alone is not gameplay readiness.

## Deliberately parked

- naming `FUN_0067CA60()->vtable+0x84` and `FUN_0067DDE0()->vtable+0x44` beyond their observed
  guard/toggle semantics;
- exhaustive MessageManager listener registration and console vtable reconstruction;
- full FLOSS extraction, unless a bounded string question cannot be answered by Ghidra/REA;
- interpreting SDK address tables as this GA build's exact addresses;
- making the host `levels -unlock`, camera, catalogue or gameplay services appear ready without
  corroborated backend evidence.

## Immediate next probe

The state-2 entry/refresh and destructive record rebuild are now backed by retained
[publication extracts](audit/gge-publication/README.md). Event `0x080D7451` carries enter/exit
payloads to a guarded flag/span handler; it is not a refill completion shortcut. Next, reuse the
existing real-save observations and focus on the terrain/property and post-load/refill gap before
admitting a production catalogue. The outer state-2 selector remains unresolved, not a reason to
repeat the already recovered builder probes.
