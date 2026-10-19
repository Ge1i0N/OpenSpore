# GGE local state-10 record preparation and ScenarioMode request

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native controller update, state-manager delivery, ScenarioMode
entry, world creation or gameplay tick was observed. Continuation of
[startup-gge-state13-scenario-controller.md](startup-gge-state13-scenario-controller.md).

## `DFAE20`: local state 10 writer

The Scenario-controller handler branch for the recovered play-ready controller action calls
`DFAE20`. Its bounded writes are:

```text
controller +0x20 = 10
controller +0x28 = 10
controller +0x80 = 1
```

The shipped state-name table labels local state 10 `ReadyToLaunchScenarioForPlay`. This is a
controller-local phase and not an outer AppState, not the GGE root internal state `+0x108`, and
not proof of a playable world.

## `DFBD40`: rebuild launch record

`DFBD40` calls `EF1910` to obtain/rebuild a shared launch record. Its bounded state split is:

```text
local controller state == 9
  → record +0x24 = 1
  → record +0x28 = 0
  → copy controller +0x8C/+0x90/+0x94 to record +0x00/+0x04/+0x08
  → copy controller +0x8C/+0x90/+0x94-derived data to record +0x0C..+0x14

other completion branch (including state 10 path)
  → record +0x24 = 2
  → record +0x28 = 1
  → copy primary key fields
  → prepare additional key/asset collection fields
```

The exact semantic types of the copied three-word keys and auxiliary collection remain Unknown.
`DFBD40` is record preparation, not terrain/world creation.

## `EF1D20 → EF11F0`: ScenarioMode internal strategy

The rebuilt record is consumed through these fields:

```text
record +0x28 / 0x015ACCF0 → ScenarioMode +0xD0
record +0x24 / 0x015ACCEC → EF11F0(requested internal mode)
record base 0x015ACCC8    → Scenario services
```

`EF11F0` has explicit transitions:

```text
ScenarioMode +0xCC == 1 and request 2
  → transition work
  → write +0xCC = 2

ScenarioMode +0xCC == 2 and request 1
  → transition work
  → write +0xCC = 1

otherwise
  → write requested internal mode
```

Existing retained Scenario evidence labels `+0xCC == 1` edit and `+0xCC == 2` play. These are
internal ScenarioMode strategy values, not proof that the outer `ScenarioMode` state was accepted
or that world/stage initialization completed.

## Native state request boundary

The completion route documented by the existing launch-gates audit is:

```text
DFBD40 completion
  → StateManager slot +0x28(0x21851EBE)
  → hashed state name ScenarioMode
  → outer state script entry:
      mode ScenarioMode
```

`0x21851EBE` is the lowercase FNV-1 identity of the state name `ScenarioMode`; it is not mode tag
`0x01654C10`. The subsequent registration/mode-change/entry boundary is documented in
[startup-gge-scenariomode-entry.md](startup-gge-scenariomode-entry.md). This is a state-request
edge only. Queued delivery, state acceptance, mode-driver activation, world construction and
gameplay remain separate boundaries.

## Current unresolved join

The strongest static chain is:

```text
component 0x070D74E7
  → state 13
  → Scenario controller DFE430
  → controller IDs 0x05951103/04
  → DFAE20 local state 10
  → DFBD40 launch record
  → ScenarioMode state-name request
```

Object 74 remains separate:

```text
Play Scenario ControlID 0x070D7509
  → generic activation payload 0x070D7509
  → conversion to 0x070D74E7 or controller IDs: Unknown
```

No direct comparison or conversion in this bounded pass closes that join. Do not promote the
state-10/ScenarioMode request path to object-74 action execution or playable readiness.
