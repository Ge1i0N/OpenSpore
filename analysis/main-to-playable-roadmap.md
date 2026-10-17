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

The DebugConsole work was a useful bounded calibration pass: it recovered a complete native
subsystem, validated the SDK↔Ghidra workflow and closed the exact `Ctrl+Shift+C` branch. It is now
low priority. Continuing to name every console/message-manager virtual slot would be tactical drift
unless a later gameplay probe needs that service.

## Layer status

| Layer | Current status | Meaning for the next step |
|---|---|---|
| CRT/WinMain/message pump/D3D9 | host boot and GA static chain are established | no need to keep walking startup except for a concrete missing service |
| App services and state data | service getters and shipped state scripts are substantially mapped; `IStateManager` SDK contract is a secondary guide | map only the native transition boundary needed to enter state 2 |
| GalaxyGameEntry registration/UI | registrar, controller, `GlobalUIGGE-9`, zoom layouts and lifecycle slots are recovered | serialized primary UI is not the galaxy; do not spend more time on layout enumeration alone |
| State 2 / Play Spore | `E008B0`, name `Play Spore`, `DF9740`, `DF95A0`, `DF6010` and record layout are recovered statically | identify the outer dispatch/caller and then feed a real/explicit catalogue |
| Catalogue | saved/candidate vectors, `DF95A0`, `BB61C0`, `BB8C90`, `BB4D40` are bounded; host readers/adapters exist | real selected save/catalogue evidence is the highest-value data milestone |
| Camera/viewer/scene | projection and host scene geometry are implemented; native viewer/camera producer and mount remain Unknown | use `IGameModeManager` only to map the native camera seam, not to fabricate a camera |
| Selection/phase callbacks | native F8/FC and phase bodies are bounded; automatic callback/CommandID route remains open | defer exact input dispatch until a real card scene exists |
| Stage/gameplay | not implemented | only after selected-star identity and transition target are real |

## Priority order

### P0 — real catalogue input and one end-to-end card scene

1. Validate the read-only stars.db/save adapters against one explicitly selected real file when it
   is available. Do not scan profiles or turn missing data into an empty catalogue.
2. Produce a typed `CatalogInput` with explicit manager, stars.db, planet, header and terrain
   statuses. Preserve Unknown/Absent/Available/Failed separately.
3. Run the existing scene/lifecycle path from that input and validate a real saved card, image,
   name, position and reverse pick. This is the shortest route from current controlled preview to
   a genuine data-backed GGE screen.

### P1 — one focused native state-2 pass

Use persistent Ghidra sequentially, with no broad import:

1. The handler registration is now bounded: `DE7660` creates/initializes the Simulator state
   handlers, `E008B0` installs the Play Spore vtable `0x147EF50`, `DF9620` is its state-2 enter
   slot, and `DF9740` is its state-2 refresh slot. This is direct GA vtable/data evidence.
2. The record rebuild boundary is also closed: `DF9620`, `DF9740` and `DE32B0` converge on
   `DF95A0`; it clears old per-record UI data, reads saved/candidate spans, resizes the `0xBC`
   array, emits candidates into the post-saved region, then saved entries, UI data and effects.
3. The remaining dispatch question is only the outer caller that selects state 2; do not broaden
   this into a generic state-manager rewrite.
4. Keep `DF9620 → DF95A0 → DF6010 → DF8300 → event 0x080D7451` and
   `DF9740 → DF95A0 → DF6C10` as the retained native orders. The current static result is:
   `state +0x108 == 2` → show `0x0615A5FF`/GlobalUI → rebuild records → continuation.
4. Compare only the relevant SDK contracts: `IStateManager::SwitchToState` and
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

The next reverse-engineering pass should be the narrow state-2 dispatch probe, followed by an
explicit real-save validation if a user-selected save is supplied. This keeps the work on the
Main→playable path while preserving the current evidence boundaries.
