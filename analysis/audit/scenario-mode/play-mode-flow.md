# ScenarioPlayMode initialization and act/goal lifecycle

Static analysis of the selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native runtime was launched and no save/resource writer was executed.

This audit follows the boundary documented in
[scene-object-flow.md](scene-object-flow.md). The relevant native routines match the semantic
roles listed by the external SDK headers, but the shipped GA instructions are the authority here:

| VA | Static role used in this audit |
|---:|---|
| `F1F060` | ScenarioPlayMode initialization entry |
| `F1EE70` | current-act setup/selection |
| `F1C390` | goal/update scan and ScenarioMode transition request |
| `F1F3C0` | jump/advance to a requested act |
| `F1F4E0` | goal-progress update entry called by the play update loop |
| `F1F960` | play-mode update dispatcher |

These names describe control-flow roles, not proof that all dependencies succeeded.

## Initialization entry: `EF0E00 → F1F060`

The play-side enter routine calls `F1F060` with the ScenarioPlayMode receiver at GGE/Scenario
play-mode storage `+0x78`. `F1F060` then:

1. calls `F1DF00` and clears play-mode byte `+0xB4`;
2. obtains a service-side pointer from the global context and `EED7E0`, storing it at `+0xC4`;
3. checks local play phase/state `+0x90`;
4. when `+0x90 == 2`, clears it to `0` and calls `F1EE70(0, 0)`;
5. reads the ScenarioMode launch context at global `+0xD0`.

For launch-context values `1` or `2`, it enters the initialization branch at `F1F105`. The
instructions there obtain a manager object through `67DE00`, call its virtual `+0x20`, create a
service/resource object, set a service bit (`+0xB58 |= 0x800`), set a manager bit selected by
identity `0x06669387`, clear a timer/accumulator at the selected service `+0x80`, and walk a
linked object list. For each eligible object, its virtual methods `+0x10`, `+0x2C` and `+0x30`
are called with `1`.

The list walk and service fields are concrete observations. Their resource names, completion
meaning and relation to terrain are not established here.

For other launch-context values, `F1F060` calls `F1C390`, and if the phase remains zero it writes
phase `3` and conditionally jumps to `E148B0`. Thus the mode does not unconditionally become
playable after `F1F060`; the branch depends on both local phase and global launch context.

The tail of the initialization path calls `B3D5D0`/`AE08B0` with an entry selected from the
global table at `0x016C82BC..0x016C82C0`, then writes play-mode phase `+0x90 = 1`. This is a
native phase transition, not an observation that a stage loaded.

## Current-act setup: `F1EE70`

`F1EE70` receives an act/index value and stores it at play-mode `+0xB8`. It calls `F1ABE0` and
`F1EBD0`, then—unless its boolean control argument is nonzero—runs `F1B330` and `F1E580`.

It obtains the current act-side object through `F3BD00`. If an existing play resource at `+0xF0`
does not match the returned object identity at `+0x80`, it releases the old resource through
`572020` and clears `+0xF0/+0xF4`. If no current resource exists and the act object is valid, it:

```text
play-mode +0xF4 = act-object +0x80
play-mode +0xF0 = A20670(act-object +0x80)
```

It then reads a position vector and sends setup calls carrying native IDs
`0x12D2A4D4` and `0xBA134192`, followed by `572070`, `DD1BE0` and `657270` operations. Those
IDs and calls are retained as shipped values; their semantic resource names are Unknown.

Finally it calls `F0E5A0` and `F11380`. This is the strongest recovered act-context setup edge,
but it still does not expose a terrain constructor or a successful resource mount.

## Goal/update scan: `F1C390`

`F1C390` only performs its main scan when play-mode phase `+0x90 == 3`. It iterates the act data
range `+0x64..+0x68`, using `0x1AC`-byte records. For an inactive record it calls `F41C60` with
the record's `+0x24` identity and current act/index information.

When that check succeeds, it:

- writes play-mode state `+0xB0 = 4`;
- builds a name/request buffer around the ScenarioMode tag `0x21851EBE`;
- submits it through `6B5440/6B5550`;
- calls `F1A9C0(6)`;
- clears the local scan latch and continues.

For records with act state `10` or `11`, the scan maps the result to status `3` or `2` before
calling `F1B610`. State `11` increments play-mode `+0x18`; every handled record increments
`+0x28`.

When the requested index reaches the native act count (`F3BCD0() - 1`), it calls `F1A9C0(5)`.
Otherwise it requests a two-phase transition through `F462B0`, updates `+0x90 = 3`, and calls
`F1EE70(nextIndex, ...)` followed by another `F1C390` pass.

This is a concrete act/goal state machine. The state values `2/3/4/5/6` here are local
play-mode states and must not be merged with GGE controller states or the global loading-context
phase values.

## Jump/advance path: `F1F3C0`

`F1F3C0` checks phase `+0x90 == 3` and bounds the requested index against `F3BCD0()`. It sets
byte `+0xEC = 1`, performs `F45E70(2, 0, 0)`, and calls `F1EE70(requestedIndex, requestedIndex != 0)`.

If the requested index is not yet current, it repeatedly calls `F1DD10` and `F462B0`, advances
the current index, and invokes `F1EE70` for each step. Once the requested index is reached it
calls `F1C390`, clears `+0xEC`, and calls `F0E5A0`.

The important ordering is therefore:

```text
stop/synchronize Scenario records
  → set current act and act resource context
  → update goals
  → clear jump latch
```

No branch in this recovered sequence proves that `F3BCD0` returned a valid world, that all goal
resources mounted, or that input/gameplay became active.

## Play update remains a separate gate

`F1F960` is the update dispatcher. It synchronizes a service byte into `+0xB4`, calls `F1F4E0`,
then dispatches local phase `+0x90 - 1` through a six-entry jump table at `0x00F1FF60`.
The update path includes timer arithmetic, service lookup, list traversal and state transitions;
it is not equivalent to `F1F060` initialization.

The external SDK layout corroborates that `+0xF8` is an intro fade timer, `+0xFC` is its active
flag and `+0xFD` is an adventure-active flag. In this audit, those labels remain semantic
corroboration only; the native bytes establish the writes and comparisons, not successful visual
intro completion.

## Current boundary

The recovered route is now:

```text
ScenarioMode pre-play gates
  → object/record synchronization and readiness bookkeeping
  → ScenarioPlayMode::Initialize
  → current-act setup
  → goal/act scan and local play phases
  → play update dispatcher
```

Still unresolved:

- successful results of `F41C60`, `A20670`, `F0E5A0`, `F11380`, `F462B0` and the service calls;
- terrain/world construction and stage activation;
- avatar/captain spawn and input activation;
- the exact queued execution of state requests;
- native evidence of a completed playable gameplay tick.

The host preview remains separate and no saves were modified.
