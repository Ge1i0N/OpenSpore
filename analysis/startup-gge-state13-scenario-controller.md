# GGE state-13 selection and Scenario-controller feedback boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native state transition, ScenarioMode launch, world creation or
gameplay tick was observed. Continuation of [startup-gge-component-activation-state.md](startup-gge-component-activation-state.md).

## `DE8710`: controller selection

`DE8710` selects the active internal GGE controller through the ordered map at root `+0x124`:

```text
current key = root +0x108
  → map lookup
  → if root +0x1EC is in 1..5, override key to 1
  → old primary +0x10 exit
  → root +0x38 = selected controller
  → new primary +0x0C enter
```

The `+0x1EC` override is an observed guard and is not collapsed into a simple state-13 mapping.
This function is a controller-selection/update lifecycle, not proof of an action producer.

## State 13 registration

`DE7660` registers the native Scenario controller instance under ordered-map key `13`:

```text
allocate 0x178 Simulator object
  → DFB990 constructor
  → primary +0x2C / DFC6C0 initializer
  → root +0x124 map[13]
```

The corrected Scenario controller vtable has primary `+0x58 = DFE430`. Therefore, when key 13 is
selected and the normal guards pass, the active controller's forwarded component-activation route
can reach `DFE430`.

## `DFE430`: Scenario-controller component family

`DFE430` accepts the same component-activation message type `0x287259F6` and dispatches a distinct
set of serialized Scenario-controller IDs. The bounded family includes:

```text
0x05933FE0..0x05933FE3 → selection/record helpers
0x05951103 / 0x05951104 → DFAE20 → local state 10
0x0733B57F             → local state 9
0x0733FE08/09          → separate state/action helpers
0x073BBA58             → local state 5
0x074D0932             → local state 7
```

`DFAE20` writes controller local state `+0x20 = 10`, `+0x28 = 10` and `+0x80 = 1`. The shipped
state label for local state 10 is `ReadyToLaunchScenarioForPlay`; later ScenarioMode logic maps
the internal play strategy separately.

These are positive Scenario-controller edges, but the upstream event that would select
`0x05951103/04` is not statically joined to object-74 ControlID `0x070D7509`.

## Separation from object 74 and world readiness

The bounded route is:

```text
component 0x070D74E7
  → root state 13
  → DE8710 map[13]
  → active Scenario controller +0x58 / DFE430
  → controller IDs 0x05951103/04
  → DFAE20 local state 10
```

The distinct object-74 route remains:

```text
ControlID 0x070D7509
  → generic payload 0x070D7509
  → conversion to 0x070D74E7 or 0x05951103/04: Unknown
```

Neither state 13 nor local state 10 proves:

- `0x00E1132F` CommandID delivery;
- `0x01654C10` ScenarioMode request;
- successful ScenarioMode transition;
- terrain/world/stage construction;
- actor/avatar/input activation or gameplay tick.

## Status

Established statically:

- ordered-map controller selection and key-1 override;
- Scenario controller registration at key 13;
- active primary `+0x58 = DFE430`;
- Scenario-controller event IDs and state-10 writer;
- separation from object-74's unresolved producer.

Still Unknown:

- object-74 event conversion and runtime delivery;
- selected adventure/record semantic payload;
- ScenarioMode request/transition;
- world/stage creation and playable gameplay.
