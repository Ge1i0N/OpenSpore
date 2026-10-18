# GA ScenarioMode ↔ SDK IGameMode vtable cross-check

Reviewed **2026-10-18**. Static GA-unpacked evidence only; no original process execution.

## Registrar and object shape

GA registrar `0x00EF1870` allocates `0xE0` bytes, constructs through `0x00EF0210`, and registers
the object with mode tag `0x01654C10` and name pointer `0x014627FC`. This matches the SDK
`cScenarioMode` size (`0xE0`) and `CommonIDs::kScenarioMode` tag. The SDK address lead for the
constructor (`0xEF03F0`) is a nearby-version mismatch; the GA body is `0xEF0210`.

## Exact vtable order

GA vtable `0x0148AFD8` matches the SDK `App::IGameMode` contract at the core lifecycle/input slots:

| Slot | GA target | SDK semantic |
|---:|---:|---|
| `+0x10` | `0x00EF2290` | `Initialize` |
| `+0x14` | `0x00EF23F0` | `Dispose` |
| `+0x18` | `0x00EF24C0` | `OnEnter` |
| `+0x1C` | `0x00EF1580` | `OnExit` |
| `+0x20` | `0x00EF06F0` | `func20h` |
| `+0x24` | `0x00EF15C0` | `OnKeyDown` |
| `+0x28` | `0x00EF07D0` | `OnKeyUp` |
| `+0x2C` | `0x00EF0590` | `OnMouseDown` |
| `+0x30` | `0x00EF0650` | `OnMouseUp` |
| `+0x34` | `0x00EF0820` | `OnMouseMove` |
| `+0x38` | `0x00EF0910` | `OnMouseWheel` |
| `+0x3C` | `0x00EF2EF0` | `Update` |

This is a positive SDK mapping: method order and GA target roles agree. It contrasts with the GGE
Play Spore vtable `0x0147EF50`, whose `+0x38` is `DF9740` but whose earlier slots are shifted and
specialized. Therefore ScenarioMode is a valid SDK↔GA calibration object, while `DF9740` remains a
GA-specific handler rather than an `IGameMode::OnMouseWheel` rename.

## Scope boundary

This proves ScenarioMode registration and interface order, not that the `Play Scenario` control
enters ScenarioMode. The shipped state table defines a `ScenarioMode` state with mode
`ScenarioMode`; the ControlID `0x070D7509` has caption `Play Scenario`, but its click/message
producer remains unresolved. `cScenarioMode` owns edit/play strategies and scenario data/resources
according to the SDK, but their runtime readiness and stage factory path remain unobserved.

## GA lifecycle sample

The bounded GA bodies add a concrete, still-static lifecycle shape:

- `EF2290` (`Initialize`) allocates a `0xA8` Simulator-side object, creates another strategy/data
  object, obtains the GameModeManager and registers a listener through MessageManager `+0x28` for
  event `0x212D3E7` plus a second listener set. This is consistent with SDK `MessageListenerData`
  ownership patterns, but does not prove listener delivery.
- The instruction-level registration order is more precise: `EF2290` stores MessageManager and
  listener-data fields at `+0x94/+0x98/+0x9C`, calls MessageManager `+0x28` with handler
  `0x00EF2250` and event `0x0212D3E7`, then calls MessageManager `+0x24` for two IDs:
  `0x04249453` and `0x07FB11B4`. SDK `IMessageManager` names `+0x24` `AddUnmanagedListener` and
  `+0x28` `AddHandler`, which is a positive contract match; the callback delivery/runtime result
  remains Unknown.
- `EF23F0` removes the handler through MessageManager `+0x30` and the listener through `+0x20`.
- The registered handler `EF2250` reads message payload word `+0x08` as a mode tag. If it is
  `0x01654C10` (`ScenarioMode`) and `mMode` at `+0xCC` is zero, it calls `EF1D20` (ScenarioMode
  enter-side initialization). If the tag is neither ScenarioMode nor `0x00DBDBA1` (`Editors`) while
  ScenarioMode is active, it calls `EF1380` (leave/cleanup-side path). Editors is excluded, not the
  cleanup trigger. This is a native mode-manager payload route, not a
  direct `Play Scenario` ControlID route.
- The same event ID `0x0212D3E7` is also consumed by the app-level handler `0x007E7160`, which
  clears two app/controller float fields and handles the message without comparing the ScenarioMode
  tag. `54DF60` subscribes to the event alongside `0x01EE100A` and `0x02319914`; its earlier
  classification as a producer was wrong. The real table-driven producer `7D8670` sends
  `0x0212D3E7` before OnEnter and `0x022D1ADC` after OnEnter, with new/old tags at payload
  `+0x08/+0x10`.
- A numeric-state notification `0x074BAF67` producer exists at `ED57A0`. Its local controller
  callers pass states `-1`, `2`, `1`, `0`; this is not an `IGameMode` tag-selection command, and
  the calls do not establish GGE ownership of that controller.
- For `ED57A0`, the apparent dynamic value is now bounded: `ED5130` initializes object `+0x1C` to
  `-1`, and all five direct callers pass only `-1`, `2`, `1` or `0`; `ED6910` later reduces the
  value through lifecycle branches `0/1/2`. No resource/property-derived ScenarioMode tag is
  recovered in this producer family.
- `B63650` routes the pre-enter mode message through `B5F180 → B33A80`. The exact pair
  `old 0x01654C05 → new 0x01654C10` invokes `B335F0` service preparation; the post-enter route
  is `B5DCD0 → B336E0 → B33650`. These callbacks consume a selected pair, not request the switch.
- `DFAD20` prepares a shared launch record through `EF1910`, then passes `0x01654C10` to
  loading-UI helper `E3E350`. This is a concrete launch-preparation candidate, but its upstream
  connection to object 74 remains Unknown. `DFC210 → DFBD40` later rebuilds the record and requests
  state ID `0x21851EBE` (`ScenarioMode`); local state 9 selects internal edit mode 1 and state 10
  selects play mode 2. This prevents mistaking DFAD20's edit route for playable launch.
  The [launch-state audit](launch-state.md) also resolves generic name/tag table setters without
  claiming original queued-command delivery or successful OnEnter.
- The adjacent `DFE430` component-activation family contains the exact playable-state candidate:
   event `0x05951103/04` reaches `DFAE20`, which writes local state `+0x20 = 10`, context `+0x28 = 10`
   and enables `+0x80`. Other bounded IDs route to `DF9E30` (`0x05933FE0..E3`), state 9
   (`0x0733B57F`), state 5
   (`0x073BBA58`) and state 7 (`0x074D0932`). `0x0733FE08` is a separate branch to `DFE1F0`, not
   the state-10 writer. `DF9B00` and `DF9F90` also use `0x05951103/04` in the controller's
   layout-local UI synchronization path. EP1 SPUI record `13754` contains those IDs as concrete
   `WinButton` objects `1375` and `1580`; they are not GGE object 74. The event-to-Play-Scenario
   semantic join remains Unknown because no `0x070D7509 → 0x05951103/04` conversion is present.
- `DFB580` names local state 9 `ReadyToLaunchScenarioForEdit` and state 10
  `ReadyToLaunchScenarioForPlay`. This upgrades the state-10 interpretation from a numeric phase
  to a native controller/UI semantic, but still does not prove runtime click delivery or launch.
- The [selection-flow audit](selection-flow.md) resolves forward/back record navigation, conditional
  panel skips, pending-to-selected key promotion, state 8 `WaitingForAutoPollinate`, and the
  separate internal-message wait route. Selected key, name and `EC42D0` conditions feed UI gates;
  resource-service readiness and notification delivery are not assumed.
- The [launch-gates audit](launch-gates.md) separates waiting subphases `0/1/2`, service/list
  guards, presentation latch `0x016A0B04` and countdown. Ten decrementing Update calls return;
  only a later call observing zero can reach completion. The latch has a timed presentation
  writer and an alternative lookup-miss write, so it must not be treated as universal resource readiness.
- The constructor-established primary vtable is `0x0147EDA0`; its `+0x2C` method `DFC6C0`
  requests `GGENewScenariov3` (FNV-1 identity `0x611E869D`) and creates the labelled records
  `choose adventure → 0x05933FE0`, `choose captain name → 0x05933FE1/2`, and
  `choose posse → 0x05933FE3`. `DFE430` is the vtable's `+0x58` handler. `0x0147EDCC` was
  previously mislabeled as a table base; it is the initializer slot inside the primary table. This identifies the
  state-10 path as part of the native Scenario controller, while the object-74 click conversion
  remains Unknown.
- In EP1 layout record `13754`, the state-10 buttons share parent panels with initializer buttons:
  `0x05951103` and `0x05933FE2` both use parent `1552`; `0x05951104` and `0x05933FE3` both use
  parent `1975`. This is a shipped-layout owner correlation, not runtime click evidence.
- The secondary interface at object `+8` uses `0x0147ED9C`, with one observed slot `E00520`:
  subtract `8` from ECX, then jump to `E00170`. Adjacent words belong to the primary vtable,
  not extra secondary methods. `E00170` handles internal payload IDs `0x366A930D`/`0x2B978C46`
  and dispatches existing controller states; it has no direct `0x070D7509` or `0x01654C10` branch.
- Constructor `DFB990` writes primary vtable `0x0147EDA0` and secondary interface `0x0147ED9C`,
  while `DE7660` allocates the `0x178`-byte `Simulator` object, invokes the constructor and
  invokes primary `+0x2C = DFC6C0`, and publishes it in the ordered map at GGE root `+0x124`
  under key `13`, guarded by `6854E0(2)`. Direct call `DDE1B0 → DE7660` exists even though
  the provider does not carve its containing procedure. This is construction evidence, not runtime readiness.
- `DE8710` selects map[current GGE state], except for the explicit `+0x1EC in 1..5 → key 1`
  override, and writes the active controller at root `+0x38`. It calls primary exit `+0x10`,
  enter `+0x0C`, and update `+0x14` (`DFA7E0`, `DFAE60`, `DFC210` for Scenario).
  `DE99B0` receives **root+4**, so its `+0x34` is active **root+0x38**; its forwarding
  `+0x58` is `DFE430`. This closes a static GGE-to-Scenario-controller routing edge for the
  distinct `Galactic Adventures 0x070D74E7 → state 13` branch, not object 74's Play Scenario action.
- [Controller route evidence](controller-route-evidence.json) retains the new REA observations
  (`ev_cae5a3b713879dea52342d92d2e1b6a2d1e9ab3cf88ee3f8a8b7c72c662963e5`,
  `ev_0b6d72174988bb5124ea7649f3db8be93373b783cb62438b7e7bbdfed4d776bf`,
  `ev_2593c32aa01598cd2a9de96c36b7da110847561da38e09fbe795f60b2bc1f887`,
  `ev_881b0ef7f732c1e509181dba6bdfde7ef80d68118e038cacb4002d83955345fb`).
- Retained shipped-binary evidence: [`scenario-mode-current-evidence.json`](scenario-mode-current-evidence.json).
   The focused REA records include `DFE430` (`ev_a8c978345364376176b078d900546eb9cf8ce24596eb4d0e9b4b16011372ed58`),
   controller UI sync (`DF9B00`/`DF9F90`, `ev_6a482fca4d92d699ab63e87065a207d8bba0d74a7655b40f8bb05a1d418e110d`,
   `ev_b2fe7003d0e60764742542aa4c8df3f18dece1cfda44d7982bbd9e06a73e0204`) and the state-name table
   (`DFB580`, `ev_e30359937af9477c2104c3ff1e79aeee82e07240c9b9343c4d5ee9b2362f5982`).
- `EF24C0` (`OnEnter`) starts ScenarioMode services, sets the enter latch at `+0xDC`, calls the
  scenario initialization helper `EF1BC0`, and starts the effect/simulator side. It returns success
  statically; no runtime ScenarioMode instance was observed.
- `EF2EF0` (`Update`) branches on `cScenarioMode::mMode` at `+0xCC`: mode `1` follows the edit
  path; mode `2` follows the play path and invokes play-mode/goal/terrain helpers. When the initial
  latch is set it performs play/edit setup and selects the **ScenarioPlanet lighting configuration**
  on graphics world global `0x0167EA54`; this is not a simulator planet-creation request. The
  [pre-play audit](preplay.md) resolves the preceding separate loading-context phases `0..4`,
  service count, model predicates and the concrete graphics receiver chain. All remain static
  observations, not host readiness or successful world creation.
- [scene-object-flow.md](scene-object-flow.md) separates `EF25E0` object-list preparation,
  `F45E70/F45AD0` record synchronization and conditional object construction from `F40210/F400E0`
  readiness aggregation. These edges do not prove terrain or stage creation.
- [play-mode-flow.md](play-mode-flow.md) follows `ScenarioPlayMode::Initialize`, current-act setup,
  goal scanning and act advancement. It establishes the next native state-machine boundary without
  promoting play-mode entry to successful world/gameplay readiness.

This makes ScenarioMode a real next reverse-engineering target after a selected star, but it does
not connect the GGE `Play Scenario` ControlID `0x070D7509` to `ScenarioMode` or prove an adventure
resource/planet was loaded.

The [zoom-button flags audit](../gge-message-dispatch/fallback-route.md) further limits that
object-74 hypothesis: its own serialized Visible bit is false (`0x0A`), while the distinct
`Galactic Adventures` object's bit is true (`0x0B`). Neither implies effective runtime visibility.
The recovered state-13 entry is a separate candidate route; a missing object-74 consumer is not
proof that every playable entry must pass through that hidden serialized control.

## Separate mode-message corroboration

The SDK `ScenarioMode.h` comments identify general mode-manager messages:

```text
kMsgEnterMode = 0x074BAF67
kMsgExitMode  = 0x0462C656
```

Bounded GA immediate scans find both values in Scenario-related numeric-mode handlers. They are
distinct from the table-driven `IGameMode` notifications `0x0212D3E7`/`0x022D1ADC` and from the
two Scenario-specific listener IDs registered by `EF2290`. A shared word "mode" does not make
these channels equivalent or identify the `Play Scenario` button's semantic consumer.

The SDK `cGameModeManager` also identifies `kScenarioMode = 0x01654C10` and classifies it as a
simulator game mode. GA registration uses the same tag. The state script's standalone `ScenarioMode`
state is therefore a valid mode entry, while the GGE UI button-to-state transition remains open.

Reproduction:

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_list.py --dump 0x0148AFD8 32
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --scenario
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --scenario-message
```

Raw output is retained in [vtable.txt](vtable.txt).
