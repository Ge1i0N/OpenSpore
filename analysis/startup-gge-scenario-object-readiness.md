# ScenarioPlayMode object synchronization and readiness aggregation

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native resource completion, terrain/world creation or gameplay
tick was observed. Continuation of [startup-gge-scenarioplay-initialization.md](startup-gge-scenarioplay-initialization.md).

## `EF25E0`: play-side object-list preparation

The play-side preparation requires ScenarioMode internal strategy `+0xCC == 2` and a nonzero
service-provided byte from `0x015FD918`. Under those guards it:

1. compacts/releases a temporary pointer vector at `+0x74/+0x78`;
2. calls `EF18B0` with the Scenario-record range at `+0x2C10/+0x2C14` and index `+0x2C24`;
3. walks records with stride `0x238`;
4. calls each record's virtual `+0xAC`;
5. clears returned-object flag bit 0, increments returned-object `+0x40`, and appends the object;
6. calls `F45E70(2, 0, 0)`.

These are object/reference bookkeeping operations. The conditional construction and back-link
boundary is detailed in [startup-gge-scenario-object-construction.md](startup-gge-scenario-object-construction.md).
The returned object class and its relation to terrain, planet, avatar or stage remain Unknown.

## `F45E70`: synchronization wrapper

The wrapper performs two passes:

```text
param_3 == 0 → F411A0()
F45AD0(param_1, param_2, 1, 1)
B228F0()
F45AD0(param_1, param_2, 0, 0)
```

The play preparation supplies `(2, 0, 0)`. No direct terrain/stage/planet constructor is present
in this wrapper; `B228F0` remains an unclassified intervening service.

## `F45AD0`: conditional object creation and record links

`F45AD0` walks `0x238`-byte Scenario records and reads selected bytes from `0x4E0`-byte payloads.
Its branches can obtain/create an associated object through `EEBFF0`/`F3F8C0`, retain it in the
controller map, and synchronize flags through `F3D930`/`F3C4B0`.

Observed record/object links after creation:

```text
object +0x18 = Scenario record pointer
object +0x1C = record identity/value
```

When the transform differs from graphics/service defaults, the path reaches `F432C0`, which is a
`Simulator/cScenarioMarker` record path. Its creation branch also calls `F3F8C0` and can reach
conditional current-creature assignment; see the
[herd assignment follow-up](startup-gge-scenario-herd-assignment-join.md). Neither the marker
record nor that conditional assignment proves terrain construction or playable readiness.

## `F40210`: readiness bookkeeping

`F40210` clears record/service bit `0x5`, resolves the selected record, and invokes `F400E0` over
the selected/all valid records. `F400E0` calls `F3D620`/`F3EFB0`, sets or clears record bit
`0x10`, and on failed classification sets bit `0x4` on the record and related record. Status/wait
codes may be written at record `+0x230`.

The Boolean return is an aggregate of these record/service predicates:

```text
record/resource checks
  → record bits +0x4/+0x10 and status +0x230
  → aggregate Boolean
```

It is not promoted to universal world-ready, mounted-resource-ready or playable-state evidence.
In the non-play strategy, the path additionally calls `F3D7A0`; this is a strategy-specific
continuation, not an alternate readiness proof.

## Boundary result

```text
ScenarioMode +0xCC == 2
  → EF25E0 object vector
  → F45E70 two synchronization passes
  → F45AD0 conditional object creation/links
  → F40210 record/status aggregate
```

The `EEDD40` dispatcher, play-specific `F237B0` lookup, and separate `F432C0` marker path are
documented in [startup-gge-scenario-object-factory-dispatch.md](startup-gge-scenario-object-factory-dispatch.md).

Remaining Unknown:

- successful object lookup/allocation and returned-class semantics;
- resource mount/completion and consumer mapping;
- terrain/world/stage activation;
- actor/input activation and playable gameplay.
