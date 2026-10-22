# GGE component-activation message and state-13 feedback branch

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native message delivery, property service or state transition
was executed. Continuation of [startup-gge-generic-button-dispatch.md](startup-gge-generic-button-dispatch.md).

## `DE99B0` message shape

The listener first rejects its lifecycle-disabled paths, optionally forwards through the active
controller target at receiver `+0x34`, then checks:

```text
message +0x08 == 0x287259F6
message object/interface
  → interface vtable +0x1C
  → component identity
```

The component identity, not the original serialized WinButton ControlID by itself, selects the
later GGE branches.

## `0x070D74E7` branch

The bounded switch compares the returned component identity with `0x070D74E7`. Under its observed
guards, the branch reaches `DE9C4E`, which:

```text
DE9C4E
  → resource/property service lookup
  → SetProperty(0x080D7DE1, value 1)
  → internal controller state fields +0x104/+0x108 = 13
```

`0x080D7DE1` is retained as the shipped ScenarioGAButtonClicked property identity. This establishes
a concrete **component-activation feedback branch for component ID `0x070D74E7`**.

It does not establish that the `Play Scenario` WinButton ControlID `0x070D7509` generates this
component identity. The generic producer audit establishes object 74's serialized `CommandID=0`
and generic activation payload `0x070D7509`; no static conversion to `0x070D74E7` was recovered.

## State feedback, not launch

The branch writes internal state/request fields and updates the GGE UI feedback surface. It is
separate from:

```text
controller +0x108 == 2
  → DF9740
  → DF95A0 catalogue rebuild
  → DF6C10 continuation
```

The state-13 feedback branch and its separate controller-selection continuation are bounded in
[startup-gge-state13-scenario-controller.md](startup-gge-state13-scenario-controller.md). It also does not prove:

- `0x00E1132F` CommandID dispatch;
- `0x01654C10` ScenarioMode delivery;
- object-74 action execution;
- world/stage construction or playable gameplay.

## Boundary result

```text
component-activation message 0x287259F6
  → component ID 0x070D74E7
  → ScenarioGAButtonClicked property 0x080D7DE1
  → GGE internal state 13

Play Scenario ControlID 0x070D7509
  → generic payload 0x070D7509
  → conversion to 0x070D74E7 remains Unknown
```

The two identities must remain separate namespaces until a runtime or instruction-level producer
join is proven.
