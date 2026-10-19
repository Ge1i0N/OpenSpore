# Scenario invalidation event and gameplay-phantom boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static shipped-binary evidence only. No native execution, resource completion, terrain/world
creation or gameplay tick was observed.

Continuation of [startup-gge-scenario-object-state-sync.md](startup-gge-scenario-object-state-sync.md).

## Event `0x01A0219E` is generic lifecycle signaling

The event value `0x01A0219E` appears in multiple independent paths. The bounded references include
object invalidation/release, a manager shutdown/reset path, and another subsystem cleanup path. One
small producer (`AC1430`) forwards the event through manager virtual `+0x20`; another (`B452F0`)
publishes a group of event values including `0x0182C582`, `0x01A0219E`, `0x025630B7`, `0x031018B9`
and `0x00F62DEF`; `FDE170` publishes `0x01A0219E` together with `0x03795725`.

Therefore the event proves internal lifecycle/message publication, not a Scenario-only completion
signal. Its consumers, payload interpretation and runtime delivery remain Unknown.

The object invalidation producer remains concrete:

```text
B20CC0 failed-validity path
  → event 0x01A0219E
  → component +0xA4 callback
  → object invalidation flag +0x21
  → release linked owner/reference fields
```

## `B48870`: phantom policy gate

`B48870` resolves `useGameplayPhantom`, checks object identity/component `0x01186577`, and branches
by controller mode. In mode `0`, it reaches `B56160`; in mode `1`, it can reach `B43810/B47FE0`
after an index comparison. The path also checks an additional identity/configuration key
`0xCF3B981D` before entering the phantom-specific handling.

This is policy selection and object update routing, not evidence that the game has entered a
playable tick.

## `B56160`: concrete phantom/collision proxy setup

The phantom-specific path has a stronger but still bounded result. It requires, among other
guards:

```text
useGameplayPhantom enabled
mode tag == 0x01654C10
matching object/controller identity
```

It then observes object key `hasCollision` and calls `B550F0` to prepare a proxy/geometry context.
The path creates or configures simulator-side data and, in the non-null auxiliary-object branch,
allocates a `Simulator` object through `F47240`, initializes bounds/transform/collision-related
fields, and publishes internal setup records. The exact native class and successful runtime
attachment remain Unknown.

The observed `Simulator` allocation is evidence of a collision/phantom proxy family, not proof of
terrain, planet, stage, actor or avatar creation. The mode tag remains distinct from the hashed
state name `ScenarioMode`.

```text
B48870
  → useGameplayPhantom/configuration gates
  → B56160
  → hasCollision / bounds / proxy preparation
  → optional Simulator allocation
```

## Boundary result

Established statically:

- `0x01A0219E` is emitted by multiple lifecycle/cleanup producers;
- `B20CC0` emits it on object invalidation before release cleanup;
- `B48870` selects phantom-related policy by configuration and controller mode;
- `B56160` contains a `Simulator` proxy/collision setup branch under `useGameplayPhantom` and
  mode-tag guards.

Still Unknown:

- event consumer behavior and runtime delivery;
- exact `Simulator` class and attachment ownership;
- resource population, terrain/world/stage construction;
- actor/avatar activation, input activation, gameplay tick and playable readiness.

## Evidence

Focused REA records for the selected authority:

- `ev_f7ed40c05df8d1805366700d6058792344d6191d25e52b299ad81bfcedf0702d` — references to event
  `0x01A0219E`;
- `ev_6d1c8bca771c7bb51e8375392039dee74be5aab0e6fd9fd3232a7466c1478a54` — `B20CC0` event and
  invalidation path;
- `ev_ef15ce6fd3148ba04adca9a4d5d15f8b54f4c533f819bad97413eff0d05f9d0a` — `AC1430` event
  forwarding;
- `ev_3d997c5272d824be9aba6954ecf863af91305f68ac9e28ca98e3c55d2d8cdbfe` — `B452F0` grouped
  event publication;
- `ev_bf084d581a199cd026d475c2b5baf522baeac3c5118ae9f70d5ad0bf600285e8` — `FDE170` cleanup
  event publication;
- `ev_1b53857d8179ca457df2f7d5741d086dfa0f5842ee6dbaaed4dc062c6acc3af1` — `B48870` phantom
  policy;
- `ev_b1d2dac8a0f323bf7e0d74299b71089463d8d647572ecb9106a50af9b1905a9b` — `B56160` phantom/
  collision proxy setup.
