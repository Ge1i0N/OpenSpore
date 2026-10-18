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
  connection to object 74 and later table-index selection remain Unknown. Exact phases and retained
  REA records are in [message-route.md](message-route.md).
- `EF24C0` (`OnEnter`) starts ScenarioMode services, sets the enter latch at `+0xDC`, calls the
  scenario initialization helper `EF1BC0`, and starts the effect/simulator side. It returns success
  statically; no runtime ScenarioMode instance was observed.
- `EF2EF0` (`Update`) branches on `cScenarioMode::mMode` at `+0xCC`: mode `1` follows the edit
  path; mode `2` follows the play path and invokes play-mode/goal/terrain helpers. When the initial
  latch is set it requests the `ScenarioPlanet` simulator resource and performs the play/edit
  transition work. These calls are retained as native candidates, not host readiness.

This makes ScenarioMode a real next reverse-engineering target after a selected star, but it does
not connect the GGE `Play Scenario` ControlID `0x070D7509` to `ScenarioMode` or prove an adventure
resource/planet was loaded.

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
