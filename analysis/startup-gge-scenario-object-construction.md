# Scenario record object-construction boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native object factory execution, resource mount, terrain/world
creation or gameplay tick was observed. Continuation of [startup-gge-scenario-object-readiness.md](startup-gge-scenario-object-readiness.md).

## `F3F8C0`: conditional object creation

`F3F8C0` first compares the candidate/current record object with the owner-side object at `+0x8C`.
When the candidate is not already the current object, it resolves the Scenario service object and
requires the play strategy `ScenarioMode +0xCC == 2` before taking the play-side back-link branch.

The play-side branch calls `F237B0`, then writes:

```text
returned object +0x1C = source object identity
returned object +0x18 = source Scenario object/record pointer
```

If that branch is not taken, the function computes an index from local range fields and calls
`EEDD40` with the source pointer, range/index values, a transform/position-related float and
additional arguments. The returned object receives the same `+0x18/+0x1C` back-links.

This is the most direct object-construction edge in the bounded record path. `EEDD40` is an
identity dispatcher with a shared object/service finalizer rather than a single proven world
factory; the detailed boundary is documented in
[startup-gge-scenario-object-factory-dispatch.md](startup-gge-scenario-object-factory-dispatch.md).
The native bytes do not identify the returned class as terrain, planet, avatar or stage, and no
downstream world consumer is established here.

## `F3F8C0` guards and failure shape

Observed guards include:

```text
candidate == owner +0x8C → no replacement construction
service lookup failure → no play back-link branch
ScenarioMode +0xCC != 2 → no play-specific branch
EEDD40 result → back-links only after returned object is available
```

The function also has a non-play/index-based branch that computes a count from `+0x28/+0x2C` and
passes the resulting index into the construction call. The count/index arithmetic is retained as
native structure, not given a guessed semantic name.

## Relation to readiness aggregation

```text
F45AD0
  → F3F8C0 conditional object construction
  → object +0x18/+0x1C record association
  → F3D930/F3C4B0 flag synchronization

F40210/F400E0/F3EFB0
  → record status bits +0x4/+0x10
  → status code +0x230
  → aggregate Boolean
```

These edges establish object/record association and status bookkeeping. They do not establish that
the object is a materialized world or that the aggregate Boolean means playable readiness.

## Status

Established statically:

- play-mode guard and candidate/current-object comparison;
- conditional `F237B0`/`EEDD40` construction paths;
- object record back-links at `+0x18/+0x1C`;
- separation from record status aggregation.

Still Unknown:

- returned object class and successful construction semantics;
- resource population/mount and world/stage consumer;
- actor/input activation and gameplay readiness.
