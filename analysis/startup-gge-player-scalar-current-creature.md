# Player scalar versus current-creature association

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump` and PE-byte inspection. No Ghidra, game process, native creation/cleanup or
save-writing path was executed.

Continuation of [metadata/tutorial boundary](startup-gge-scenarioplay-metadata-tutorial-boundary.md).

## `cPlayer +0x1248` is a scalar, not an avatar pointer

The full setter/getter bodies are:

```text
C755B0(value): player +0x1248 = value; return
C755C0(): return player +0x1248
```

A raw relative-call/jump candidate scan of the selected `.text` found one direct setter call;
its decoded caller is `B271D0`, with callsite `B272F5`. This is a bounded direct-edge search,
not proof that no inlined, indirect or alias-based writes exist.

`B271D0` first checks whether `F67D40(objectManager)` returns a non-null player. It copies a
three-word source value through `C75BC0`, passes another field through `C75720`, then maps source
`+0xA0` values `0`, `1`, `2` to immediate scalar arguments `0`, `1`, `2` for `C755B0`.
Other values bypass that setter. No pointer source is used at this callsite.

Two decoded getter consumers corroborate an index/policy role:

- `B709D0` defaults its local index to `1` when manager/player is missing; otherwise `B709F5`
  gets `+0x1248`. At `B70A24`, that value indexes the three-DWORD identity table at
  `0x01464EA4` before a resource/service query. The instructions do not establish universal
  bounds checking for every possible writer.
- `B1DA30` optionally obtains this scalar, defaults to `1`, and indexes a mode-dependent table.
  Its separate ScenarioMode-tag `0x01654C10` branch reads `receiver +0x90 + index*4`.

This rules out treating `+0x1248` as the requested avatar association. A semantic name such as
difficulty is not required by this audit; the proven behavior is a numeric selector/policy value.

## Distinct object-manager fields

```text
F67D40(manager) → manager +0x74: cPlayer service/object context
B1FD00(manager) → manager +0x54: separate current-object reference
```

`B1FCA0` is the setter for `+0x54` (body `B1FCA0..B1FCEF`):

1. if the old object is non-null, clear bit `0x200` at old object `+0xB58`;
2. if the new object is non-null, set the same bit on the new object;
3. compare old/new pointers; if different, retain new through virtual `+0`, store manager
   `+0x54`, and release old through virtual `+4`.

Passing the same non-null object still clears and then sets its bit before the pointer comparison.
Passing null clears/releases the old reference; no actor is created inside this setter.

## `C09B40`: creature creation and optional current-object assignment

The inspected prefix of `C09B40` can obtain/reuse an object through `AD2020`, or create class
identity `0x018EB45E` through `B20BF0` and query interface `0xD0036E08`. A concrete constructor
table association uses the same mechanism as the recovered player factory:

```text
B250A6: class key 0x018EB45E
B250AE: B475C0(key) → table value
B250C2: *value = B1ED20
B1ED20:
  F47240(0x16A0, "Simulator/cCreatureAnimal" at 0x0145E168, ...)
  null → return null
  non-null → tail-jump C06960
```

Constructor `C06960` installs object vtable `0x0146A080`; its `+0x0C` slot is `C02930`.
`C02930` returns the receiver for interface `0xD0036E08`. For interface `0x023A6CC8` it returns
the receiver only when object `+0xB58` bit `0x200` is set. This directly connects the setter bit
to an object role/interface predicate, without assigning an unproven avatar/input meaning.

`C09B40` then handles several associations, including retaining/replacing object `+0x1674` from a
caller argument. Getter `C046F0` returns that field. Its semantic class remains Unknown here.

At `C09C48`, the **fifth stack argument's low byte** gates assignment:

```text
fifth argument != 0
  → B1FCA0(objectManager, selectedCreature)
then C03920(...) checks further setup
```

Assignment precedes the `C03920` result check. If that setup returns false, the body calls
`B22560(selectedCreature)` and returns null. This pass does not claim a complete rollback of
the manager reference or successful native activation.

## Scenario record creation does not automatically select the creature

The already recovered `EEDD40` identity branch at `EEE139..EEE147` passes six stack arguments
to `C09B40`: `(source, resourceContext, 1, objectContext, 0, 0)`. Therefore its fifth argument is
zero: **this call does not execute the current-object setter at `C09C57`**. That distinguishes
Scenario record creature construction from selecting a current actor.

Another Scenario-adjacent call at `F3B516` similarly pushes zero for the fifth argument. These are
specific decoded callsites, not a global absence claim for all Scenario actor-creation paths.

The inspected cleanup-side sequence at `EF1140..EF1178` obtains `B1FD00`, clears manager `+0x54`
with `B1FCA0(0)`, then sends the saved object through `B22560`. This proves reference clearing and
object-lifecycle handling, not launch or spawn.

## Result and next boundary

- **Established:** player `+0x1248` is used as a scalar selector; manager `+0x54` is a distinct
  retained object reference with flag/interface synchronization; a cCreatureAnimal factory and
  conditional assignment callsite are present.
- **Conditional:** constructor selection/allocation, interface acquisition and caller assignment
  flag must succeed/be set. Current-object assignment alone does not prove later setup success.
- **Unknown:** which Scenario play-entry producer creates/selects the captain/avatar, resource
  population, world attachment, input activation and playable gameplay.
- Object-74 `Play Scenario 0x070D7509` still has no new producer/delivery join.

Next useful work: find Scenario play-entry callers that supply a **nonzero fifth argument** to
`C09B40`, or a distinct actor assignment path; do not expand the scalar-policy consumers as if
they were avatar pointers.

## Reproduction scope

Complete inspected bodies: `C755B0..C755BA`, `C755C0..C755C6`, `B271D0..B2731A`,
`B1DA30..B1DA93`, `B1FCA0..B1FCEF`, `B1FD00..B1FD03`, `B1ED20..B1ED47`,
`C046F0..C046F6`. Partial constructor/interface/consumer slices: `C06960..C069DF`,
`C02930..C029AF`, `B709D0..B70AFF`, `C09B40..C09CFF`, `EEE0B9..EEE199`,
`F3B480..F3B5AF`, `EF10E0..EF11ED`. Selected instructions, factory slots and labels are pinned
in `tools/verify_startup_state_route.py`. Byte scans find candidates only; unexamined indirect
and inlined paths remain Unknown.
