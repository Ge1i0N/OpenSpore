# Current-creature assignment: caller flag audit

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump` and PE-byte inspection. No Ghidra, native creature construction, callback,
resource completion or save-writing path was executed.

Continuation of [player scalar/current-creature boundary](startup-gge-player-scalar-current-creature.md).

## Exact assignment argument, not the trailing flag

`C09B40` is called with six stack arguments, pushed right-to-left. Its prologue reserves `0x48`
bytes and saves four registers, so at `C09C48`:

```text
[ESP +0x6C] = fifth caller argument
C09C48: compare its low byte with zero
zero → skip B1FCA0
nonzero → B1FCA0(objectManager, selectedCreature)
```

The sixth argument is at `[ESP +0x70]` in the prefix and gates an earlier `AD2020` reuse path.
It is not the current-creature assignment flag. Thus the frequent final argument pair `(0, 1)`
does **not** select the created creature as current. The retained manager reference at `+0x54`
and bit `0x200` update remain as documented in the preceding audit.

## Bounded direct-call inventory

A `.text` relative-call/jump byte scan produced twenty direct-call candidates for `C09B40`.
Each listed call instruction and its argument-producing prefix was checked with `objdump`.
Indirect calls, inlined copies, runtime patches and alternate paths into these prefixes are not
exhaustively covered. Table values describe the inspected call paths, not observed execution.

| Decoded callsite | Fifth argument | Sixth argument | Bounded observation |
|---|---:|---:|---|
| `AD0498` | 0 | 1 | Object/context creation branch. |
| `AD4CD6` | 0 | 1 | Lookup/list fallback creation branch. |
| `AD4E8C` | 0 | 1 | Resource/context creation branch. |
| `C6B4B1` | 0 | 1 | Source/context-based creature creation. |
| `C6E0C9` | 0 | 1 | Third argument is computed; assignment remains zero. |
| `C6E861` | **1** | 0 | Source role byte and empty-current-reference gates. |
| `CD9200` | 0 | 1 | Record/list creation branch. |
| `D44187` | **1** | 0 | Sets source role byte `+0x84 = 1` before creation. |
| `D444DE` | **1** | 0 | Empty-current-reference gate; source/context may be null. |
| `D4EFB3` | 0 | 1 | Counted creation loop. |
| `D4F868` | 0 | 1 | Counted creation loop with later object flags. |
| `D51232` | 0 | 1 | Position calculation and post-creation updates. |
| `D51997` | 0 | 1 | Uses current object's resource/context to create another object. |
| `D520EC` | 0 | 1 | Counted creation loop. |
| `D529F4` | 0 | 1 | Position/context-based creation. |
| `D52B86` | 0 | 1 | Current-object context-based creation. |
| `EEE142` | 0 | 0 | Previously bounded `EEDD40` Scenario record branch. |
| `F3B516` | 0 | 0 | Scenario strategy check and resource-context path. |
| `FEFAEB` | 0 | 1 | Randomized third argument; assignment remains zero. |
| `FF00FB` | 0 | 1 | Randomized third argument; assignment remains zero. |

This inventory identifies **three positive assignment callers**, not twenty actor-selection paths.
It does not establish that all twenty callsites belong to ScenarioMode.

## `C6E690` body, callsite `C6E861`

After local parameter/container setup, the inspected path at `C6E820` requires:

```text
source byte +0x84 != 0
B1FD00(objectManager) == null
```

It obtains a position-related pointer either through `source +0x160`, adjusted by `+0x34` and a
virtual `+0x2C` call, or directly from `source +0x34`. It then calls:

```text
C09B40(position, source +0xA4, 1, source, 1, 0)
C6B6E0(source)
```

The nonzero fifth argument reaches current-reference assignment **if** creature acquisition in
`C09B40` succeeds. The examined body does not test the returned creature before proceeding to
`C6B6E0`. That helper derives a deficit from object-list/count fields and may submit further
creation through `C0A100`; it is not an acceptance check for the current-creature assignment.

The body has unrelated checks involving mode tag `0x01654C05`; that is not ScenarioMode
`0x01654C10`. Neither the address family nor the existence of a role byte proves the Scenario
play-entry join. The caller/vtable connection and the writer of role byte `+0x84` on the relevant
source remain the next questions.

## Controller body `D43DE0`: two positive callsites

`D43DE0` dispatches controller-local `+0xA0` through a thirteen-entry jump table at `D454A4`.
The callsites below occur within the inspected branch beginning at `D43E0C`. Those controller
state numbers must not be merged with GGE states or ScenarioPlayMode phases.

- At `D44187`, a prior resource/context path obtains a non-null source, sets its byte
  `+0x84 = 1`, and calls `C09B40(position, source +0xA4, 1, source, 1, 0)`. The returned value
  is discarded at this callsite. A subsequent `B1FD00` query has a non-null gate, but it does not
  prove this call succeeded or created a new actor.
- At `D444DE`, the path requires `B1FD00(objectManager) == null`, obtains resource state from
  `401090/4DF490`, optionally reads a source position, and calls
  `C09B40(position, resourceContext, 1, sourceOrNull, 1, 0)`. Its return is also not tested here.

The surrounding branch checks several mode tags and performs service, lighting and UI work.
Its registration/activation and reachability from ScenarioMode play entry are not established in
this slice. These are real conditional current-creature producers, but are not yet proven
Scenario captain-spawn producers.

## Scenario entry consumes an existing reference

The inspected `F1F060` entry branch for launch contexts `1/2`:

```text
F1F105..F1F132: manager/service calls and a string-like value from F261E0
F1F134..F1F144: B1FD00(objectManager), skip object-specific work if null
F1F14A: current object +0xB58 |= 0x800
```

`F261E0` delegates to `6B5550` or returns a stored string pointer; its result must not be called
an actor merely because it precedes the getter. The earlier virtual calls remain a separate
unresolved producer candidate. `F1F060` itself does not supply a nonzero fifth argument to
`C09B40` in the examined body. At least this entry-side bit/list work consumes an already
selected object; it does not prove who selected it or when.

## Result and next boundary

This section records the direct-call audit boundary. The later
[herd assignment follow-up](startup-gge-scenario-herd-assignment-join.md) establishes a conditional
Scenario producer join into `C6E690`; native success and captain identity remain unobserved.

- **Established statically:** six-argument distinction, twenty decoded direct-call prefixes and
  three conditional current-creature assignment callers.
- **Not established:** Scenario play-entry reachability of those three callers, successful
  creature creation/selection, captain identity, input ownership or playable readiness.
- **Unchanged:** world/terrain/stage construction and object-74 `Play Scenario 0x070D7509`
  producer/delivery join remain Unknown.

The [herd assignment follow-up](startup-gge-scenario-herd-assignment-join.md) resolves `C6E690`
as `cHerd +0x54` and finds a conditional Scenario marker-record join through `B23920`'s role
writer. It also resolves the pre-getter virtual call in `F1F060` as string setup, not a factory.
This closes static caller joins, not runtime spawn, captain identity or playable readiness.

## Static evidence scope

Read-only disassembly covered every call prefix in the table; larger caller bodies were examined
only where stated. Focused slices include `C6E690..C6E88F`, `C6B6E0..C6B745`,
`D43DE0..D43E6F`, `D4405F..D4421F`, `D44300..D4454F`, and `F1F060..F1F282`.
`C09B40` argument mapping uses its prologue and decoded `C09C48..C09C57` predicate/call.
Selected instruction/argument anchors are pinned in `tools/verify_startup_state_route.py`.
