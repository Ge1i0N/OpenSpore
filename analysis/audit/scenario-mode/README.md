# GA ScenarioMode ↔ SDK IGameMode vtable cross-check

Reviewed **2026-10-17**. Static GA-unpacked evidence only; no original process execution.

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

Reproduction:

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_list.py --dump 0x0148AFD8 32
```

Raw output is retained in [vtable.txt](vtable.txt).
