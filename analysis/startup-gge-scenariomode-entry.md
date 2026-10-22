# ScenarioMode registration, mode-change publication and entry callback boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native mode message delivery, ScenarioMode OnEnter completion,
world construction or gameplay tick was observed. Continuation of
[startup-gge-play-record-preparation.md](startup-gge-play-record-preparation.md).

## ScenarioMode registrar and constructor

`EF1870` allocates `0xE0` bytes, invokes constructor `EF0210`, then registers the object with:

```text
mode tag  = 0x01654C10
name ptr  = 0x014627FC
```

`EF0210` installs primary vtable `0x0148AFD8`, initializes the mode fields and publishes the
object through the registrar. The observed vtable order matches the generic `IGameMode` lifecycle
shape, including Initialize, Dispose, OnEnter, OnExit, input slots and Update.

This is registration/shape evidence, not proof that a runtime state request selected the object.

## Table-driven mode change

The generic mode table stores entries of the form:

```text
entry +0x00 = mode object
entry +0x04 = mode tag
```

`7D8670` changes the selected table index and publishes the mode-change message pair. The mode
manager's surrounding endpoints find entries by tag/name, validate an index and call `7D8670`.
The ScenarioMode tag is therefore a registered mode identity, not a numeric state ID.

The retained static message route is:

```text
7D8670
  → mode-change event 0x0212D3E7 (old/new tags in payload)
  → selected mode enter/exit lifecycle
  → mode-change event 0x022D1ADC after enter
```

ScenarioMode's own handler `EF2250` reads message payload word `+0x08` as a mode tag. If the tag is
`0x01654C10` and mode field `+0xCC == 0`, it calls `EF1D20`; if a non-ScenarioMode/non-Editors tag
arrives while active, it takes the cleanup path. Listener registration is static; delivery and
handler success remain Unknown.

## Separation from state-name request

The earlier completion route is:

```text
DFBD40
  → StateManager +0x28(0x21851EBE)
  → hashed state name ScenarioMode
  → state script mode ScenarioMode
```

The mode-driver route is distinct:

```text
mode tag 0x01654C10
  → mode table lookup / 7D8670
  → mode-change messages
  → ScenarioMode EF2250 / EF1D20 callbacks
```

`0x21851EBE` and `0x01654C10` remain different identity namespaces. Neither static route proves
that the other completed successfully.

## First entry-side callback boundary

`EF1D20` performs ScenarioMode entry-side strategy/resource setup based on the rebuilt launch record
and internal mode request. The loading-context and Update phase gate are bounded in
[startup-gge-scenariomode-preplay-update.md](startup-gge-scenariomode-preplay-update.md). This is the beginning of ScenarioMode-specific processing, but the
bounded entry path does not establish terrain/world/stage materialization or a playable tick.

The mode object may own edit/play strategies and scenario data services. Those are separate from
the GGE controller's local state 10 and from the unresolved object-74 action conversion.

## Status

Established statically:

- ScenarioMode allocation, constructor and registration;
- tag/name identity and vtable order;
- table entry/tag selection and mode-change message publication;
- ScenarioMode message handler tag check;
- separation of state-name hash and mode tag;
- entry-side EF1D20 boundary.

Still Unknown:

- queued mode message delivery and OnEnter completion;
- resource/strategy readiness;
- terrain/world/stage creation and actor/input activation;
- object-74 producer join, playable gameplay tick and successful launch.
