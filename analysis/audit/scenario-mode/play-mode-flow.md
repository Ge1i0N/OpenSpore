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

## Post-act record consumer: `F11380`

`F11380` is reached from the ScenarioMode transition path after `F0E5A0`. It requires a
non-null service/context at `+0x2E0`, initializes a descriptor/list at `+0x2EC`, and submits an
initial descriptor through `E10560`. It then walks the Scenario record range with the familiar
`0x238` stride, resolves record objects through `F3D620/F3E7A0`, checks the selected byte in the
`0x4E0` payload, obtains the act-side entry through `F3BD00`, and walks act entries with `0x188`
stride. Entries with `+4 == -2` or inactive `+0x10` are skipped. Matching entries are converted
to descriptors and submitted through `E10560` and `F0FD40`.

After the record walk it submits a final descriptor through the same service path. This is a
concrete post-act record/descriptor consumer and may be part of scene presentation or object
registration. The native bytes do not identify the descriptor as terrain, nor do they prove that
the downstream service accepted or materialized it.

## `F41C60`: bounded goal/resource outcome classifier

`F41C60` initializes record result field `+0x184` to `-1`, dispatches on record state through a
local table, and evaluates object/resource conditions via `F41550`, `F3B210`, `F40370`, `F3E740`
and `F25D50`. Depending on those conditions it writes local outcome values including `0x11`,
`0x13`, `0x14`, `0x15` and `0x16`.

These values are not promoted to semantic names and are not treated as generic "resource not
ready" or "world ready" without a proven consumer mapping. The function returns a Boolean
alongside this local result, so the caller's `F1C390` decision remains a separate gate.

## Descriptor registry boundary: `F0FD40 → ABFE60` and `E10560`

The descriptor path itself is now bounded more precisely:

- `F0FD40` performs a tree lower-bound search using descriptor key `+0x00`; on a miss it calls
  `ABFE60` and returns the entry payload at `+0x14`;
- `ABFE60` performs the ordered-tree insertion/rebalance and returns the inserted node; it does
  not call a terrain or stage constructor;
- `E10560` reads the descriptor vector/flags, resolves an identity through the descriptor's
  service vtable, and maps flags to downstream command/identity constants in the `0x05E51A..`
  range;
- it then looks up a service entry through `8062B0`, creates/gets an entry through
  `951220/951230`, initializes its interface and sends descriptor-derived values through virtual
  slots.

`E10560` has explicit handling for ScenarioMode tag `0x01654C10`; that branch selects a value
from the input descriptor context. This proves a ScenarioMode-specific downstream command path,
not a world/terrain allocation. The registry and service/UI entry may be necessary for later
presentation or object control, but their successful downstream effects remain Unknown.

## Service/UI registry implementation details

The immediate consumers are bounded as follows:

- `951220` returns global registry pointer `0x0166AAB4`.
- `951230` normalizes an allocation size, then either calls a registry virtual slot on the
  supplied service object or falls back to `928CC0`; on success it stores the service owner at
  the returned block and returns `block + 8`.
- `966CE0` initializes one of the concrete entry interfaces used by this service/UI layer. Its
  writes install vtables and initialize entry fields; they do not call a terrain/world factory.
- `F0F050` creates a small descriptor node, copies the three-word vector, calls `921930`, and
  increments the owning registry count at `+0x14`. It returns that node to `ABFE60`/`F0FD40`.
- `8062B0` looks up a service by hash through `810650`, then queries identities
  `0xEF3C47CF` or `0x103C1908` through virtual slots. A null result is returned when neither
  lookup succeeds.

Thus the downstream side is a service/UI registry plus ordered descriptor ownership. It is a
real native consumer of the Scenario path, but it remains separate from stage/terrain creation
and from observed gameplay readiness.

## Registry node and consumer details

The allocator-facing helpers preserve the same boundary:

- `921930` initializes a node's key/value/links and inserts it into an ordered tree. Its writes
  are generic node bookkeeping; no Scenario or terrain identity is hard-coded there.
- `928CC0` retains the registry's allocator-side object at `+0x4E4`, calls `928570` with the
  supplied construction arguments, and releases the temporary retained object afterward.
- `810650` is an owner wrapper: it follows owner `+0x14` and delegates lookup to `8102A0`.

After the entry is obtained, the tail of `E10560` calls entry virtual slots for values, bounds,
flags and object state, and submits descriptor geometry through `E0BC90`/`E0BF20`. The branch
checks descriptor flags and identity values, including a distinct `0x01654C02` case; this must
not be conflated with the ScenarioMode tag `0x01654C10` checked earlier in the same dispatcher.
These are object/service-consumer effects. Their downstream acceptance and any eventual scene
materialization remain Unknown.

## Spatial/geometry consumer boundary

`E0BC90` and `E0BF20` explain what the descriptor geometry is used for:

- `E0BC90` obtains a global service/context, calls a virtual geometry getter, transforms the
  returned values using scale/count fields at `+0x23C/+0x240`, and updates the local spatial
  coordinates through `E0AA60`.
- `E0AA60` converts a scaled coordinate into a bounded `0..0x80` lookup index, samples the
  byte grid at `+0x280`, applies the local scale fields `+0x21C/+0x220`, and returns a spatial
  value. It is a grid/geometry lookup, not a terrain allocator.
- `E0BF20` computes extents and a grid index from the entry bounds, calls the entry's virtual
  geometry method, and compares the current identity against `0x01654C02` and `0x01654C10`.
  It then performs range checks and invokes entry virtual slots for the resulting spatial values.

The ScenarioMode-specific branch can therefore affect spatial/object presentation, but the
observed operations are bounds, grid sampling and virtual entry updates. No direct stage or
terrain construction call appears in this boundary.

The host preview remains separate and no saves were modified.
