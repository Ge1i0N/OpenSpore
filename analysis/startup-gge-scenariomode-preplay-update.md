# ScenarioMode entry/update pre-play boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native resource request, update delivery, world/stage creation or
gameplay tick was observed. Continuation of [startup-gge-scenariomode-entry.md](startup-gge-scenariomode-entry.md).

## `EF24C0`: OnEnter initialization

The ScenarioMode OnEnter body establishes a separate loading context:

```text
EF24C0
  → initialize mode-side fields/vector
  → create/obtain service-side object
  → set ScenarioMode +0xDC = 1
  → reset global loading context at 0x015ACC30 through EF1BC0
```

`EF1D20` separately initializes the launch record/resource side, including:

- dependency/vector fields around ScenarioMode `+0x80..+0x90`;
- launch context at `+0xD0` from record `+0x28`;
- a service/resource object allocated with size `0x1CC`;
- replacement of the mode resource pointer at `+0x18`;
- internal strategy `+0xCC = 1` or `2` based on the rebuilt record.

These are concrete entry-side allocations and field transitions. They do not prove that the
resource object is populated, that a stage exists, or that an actor can spawn.

## `EF2EF0`: Update gate and global loading phases

Before ordinary mode-specific update, `EF2EF0` checks the global loading context phase at
`0x015ACC34`:

```text
phase != 4
  → increment 0x015ACC40 only when already nonzero
  → EF2930(context)
  → return

phase == 4
  → inspect ScenarioMode +0xDC enter latch
  → clear latch and perform entry-side continuation
  → dispatch by internal strategy +0xCC
```

`EF2930` dispatches phases `0..3` through a jump table. The observed actions include permission
initialization, service wait predicates, model/object checks, dependency submissions and finishing
service work. Every phase advancement returns to the caller; phase 0's fallback counter does not
bypass later phase predicates.

This is a pre-play loading/synchronization state machine. Small integer phase values are not
merged with GGE root state, controller local state, ScenarioMode `+0xCC`, or world readiness.

## Internal play/edit split

After global phase 4 and entry-latch handling, `EF2EF0` branches by ScenarioMode `+0xCC`:

```text
+0xCC == 2 → EF0E00 play-side setup
+0xCC == 1 → EF0990 edit-side setup
```

The play-side setup reaches `F1F060` and associated ScenarioPlayMode routines. The edit-side path
has its own preparation and resource calls. This identifies a native internal strategy split, not
a proof of successful play-mode initialization.

## Relation to world/stage construction

The bounded pre-play route is:

```text
ScenarioMode OnEnter
  → loading context phase 0..3
  → phase 4 gate
  → internal +0xCC strategy
  → EF0E00 or EF0990
  → Scenario play/edit helpers
```

The later play-side object request and ordinary-update phase table are bounded in
[startup-gge-scenarioplay-service-phase-progression.md](startup-gge-scenarioplay-service-phase-progression.md).
That slice identifies a registered cPlayer factory candidate but does not prove native avatar
creation or world attachment.

Nearby `EF25E0`, `F40210`, `F45E70`, `F1F060` and later ScenarioPlayMode paths are retained as
world/scene-adjacent candidates, but their return values, resource population and stage semantics
remain separate investigations. No native terrain/world factory success is promoted here.

## Status

Established statically:

- OnEnter loading-context initialization;
- EF1D20 launch-context/resource setup;
- global phase 0..3 dispatch and phase-4 gate;
- entry-latch handling;
- internal edit/play split at ScenarioMode `+0xCC`.

Still Unknown:

- runtime dependency/resource completion;
- ScenarioMode message delivery/acceptance;
- successful play-side initialization;
- terrain/world/stage materialization, actor/input activation and gameplay readiness.
