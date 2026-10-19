# Scene-container queue completion and child-resource setup

## Authority and question

Selected `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, x86 PE, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Preferred image VAs; short `objdump` slices, cached address-range output and PE-byte reads.
No original execution, async job completion, callback, native resource load or save write ran.
Continuation of [animation scene-container registration](startup-gge-animation-scene-container.md).

Question: what drives the deferred initializer and which children does `74A990` acquire?
The Scenario Update → scheduling join is sought, not assumed from a nearby update-like routine.

## Result and correction

**Observed (static):** the previously inspected `751ADF → 74A990` call is inside completion
helper `7515D0`. It is not itself an ordinary per-frame queue drain. A manager virtual method
walks containers and invokes an eight-slot async pump that conditionally calls this helper
for numeric job statuses `7/8`. A separate force/wait path can also call it.

**Observed (static):** `74A990` acquires up to four primary child resources and two additional
property-gated/optional slots through `754C00`. That helper tries resource type `0x00E6BCE5`,
then `0x2F4E681B`; it constructs a child object and copies/adapts resource content. Missing
children do not prevent the initializer tail's flag, counter and optional owner notification.

**Still Unknown:** the actual Scenario Update caller of the constructed manager slot, async
submission/completion at runtime, successful render-object setup, enabled input and playable
readiness. Static manager traversal is not evidence that an original gameplay tick occurred.

## Deferred scheduling: manager → container → eight async slots

Follow-up: the [outer-frame scheduling audit](startup-gge-scene-pump-outer-frame.md) closes
the upstream caller as `7E9E80 → AppMode +0x5C / 7A4E50 → host +0x28 / manager +0x38`.
It is common graphics/service work after selected-driver Update, not a direct call inside
ScenarioMode Update or proof of native frame execution. Earlier unresolved-caller statements
below describe the boundary of this original slice and are superseded by that positive join.

The preceding audit pins manager primary table `0x0140D768` and the container map rooted at
manager `+0x0C`. An additional slot has now been decoded:

```text
manager table +0x38 = 7535A0
  walk manager map, use each node +0x14 as container
  7535C5 → 752680(container, two stack scalar values)
752680:
  752689..752693 → 751E50(container +0x314)
  only afterwards check container byte +0x1D for the remaining work
751E50(queue):
  walk 8 slots, stride 0x50, first slot at queue +4
  active byte slot +0x1C != 0:
    68F930(slot +0x24) → numeric status
    unsigned (status - 7) <= 1 → 7515D0(queue.owner, slot)
  inactive slot can pull a queued 0x1C-byte request through 74FF10
```

`7535A0` returns with `ret 8`; its call copies two DWORD-sized values via x87 stack operations.
Time-related names are plausible but not required for this call chain. The manager slot must
not be confused with **container** `+0x38 = 74EBE0` or unrelated services' `+0x38` queries.

Queue submission `74BE10` works at container `+0x314`, and `74FF10` installs the request into
an async slot, updates its active/state fields, obtains job/service objects and submits more
work in its body. This audit establishes the pump's predecessor and status gate, not a complete
job API/state enum. In particular numeric `7/8` are not renamed "success" without evidence.

A second decoded caller is `753690` (reached by container `+0x58 = 753850`): it selects an
existing/free/round-robin slot and, if it is active, calls `7532D0(container, slot)`. That helper
waits through `692670`, checks that the slot remains active and the job pointer unchanged,
performs additional work and calls `7515D0` at `753434`. A wait/force route is distinct from
the ordinary status-polling pump and is not proof of an enabled game input loop.

## Completion helper and the saved-request replay

`7515D0(container, slot)` starts from `slot[0]` (embedded object pointer), subtracts `8` to
recover its wrapper, then clears/replaces child references from the slot's prepared values.
It checks queue membership with `7476B0` and can take a short path without the later replay.
Otherwise it marks wrapper flags and can invoke the slot's callback with the embedded pointer.

It also walks captured `0x1C`-byte requests. At `751A90..751ADF` it retains the embedded
object reference and definition, reads saved owner/control/callback values, subtracts `8`,
and calls `74A990`. The saved callback is then conditionally invoked and references released.
The existing audit correctly identified this local replay but had not classified its enclosing
completion routine. Do not model every queue pump call as unconditional `74A990` execution.

Raw E8/E9 scans found the decoded `7535C5 → 752680`, `752693 → 751E50`, and two calls to
`7515D0`; a raw table pointer scan located manager `+0x38`. Bounded direct-getter/slot-encoding
searches in graphics owner/Simulator/Scenario ranges did **not** establish the upstream
Scenario caller. Such discovery scans do not exclude indirect, cached-pointer, inlined or
alternate scheduling paths. No global absence of Scenario ticking is claimed.

## `74A990`: child slots, optionality and tail notification

The receiver is the container; three stack arguments are `(definition, wrapper, control)`.
It first clears/releases six paired child-reference slots and invalidates local index bytes.
The decoded primary resource loop then processes property IDs `0x00F9EFBB..0x00F9EFBE`:

```text
6A1250(definition, propertyID, &threeWordTuple)
  false → exit the four-entry loop (not a whole-initializer false return)
  true:
    tuple.word2 == -1 → use definition +0x10 as group value
    otherwise use tuple.word2
    754C00(tuple.word0, chosenGroup, &wrapper.childSlot, control)
    advance child slot +4 and property index
```

The primary slots are wrapper `+0x9C/+0xA0/+0xA4/+0xA8`. Helper results are not aggregated
into a Boolean readiness return. After that loop, control bit `0x20` gates property `0x00F9EFC1`
and wrapper slot `+0xB0`; property `0x00F9EFC0` separately fills wrapper `+0xAC` (including
the `word2 == -1` special branch). Missing definitions/parts remain distinct from a ready model.

After `7473B0` child processing, the initializer selects an available child to derive bounds
and scale fields. Its final tail `74ACAF..74ACEA` always reaches wrapper flag `0x4000` and
increments WORD `+0x70` on this normal-return path; those writes are not a conjunction of
successful child acquisition. If wrapper `+0x6C` is non-null, it queries virtual
`+0x0C(0x07BBC4FF)`. A non-null queried interface receives wrapper `+8` through its first
virtual slot. This is a concrete optional owner notification, not identified input activation.

The raw exact-ID scan found one other occurrence, decoded in `C88680`, which accepts
`0x07BBC4FF` and `0x01186577`. That is an interface candidate, **not** proof that the selected
creature's vtable returns it or that this launch executes the callback. The containing class,
caller/interface delegation and first-slot behavior were unresolved in that slice. The later
[creature notification audit](startup-gge-creature-scene-owner-notification.md) resolves the
constructed creature query to component `+0xC0` and first slot `C88600`: only byte `+0xA5=1`,
without reading the supplied wrapper pointer. The constructor already initializes that byte
to `1`; callback occurrence is not proof of a new activation/readiness transition.

## `754C00 → 7549C0`: concrete child acquisition

`754C00(instanceWord, groupWord, outputAddress, control)` builds a three-word resource key
and calls `7549C0` first with type `0x00E6BCE5`. Only a false result triggers the second
attempt with type `0x2F4E681B`. These types are not replaced by guessed asset filenames.

For each attempt `7549C0` requests a resource via `67DC90 +0x0C` and tests the return. On a
true result it allocates `0x100`, constructs child table `0x0140D680` through `740F10`, and:

- type `0x00E6BCE5`: calls `742BB0(child, resource)`, stores the child in caller output and
  returns true; `742BB0` retains the resource at child `+0xC4`, copies bounds and other fields,
  optionally obtains another resource-side interface and copies local ranges. Its caller does
  not test an independent completion Boolean from this setter;
- type `0x2F4E681B`: has control/validation-dependent adaptation, can use helpers
  `757DA0/759300/6AD000` and then `742BB0`, or calls `741AC0`. That routine retains the
  resource, invokes `741290` and returns false if child range `+8/+0x0C` is empty; otherwise
  it invokes `740440` and returns true;
- resource request false or rejected adaptation: releases temporaries and returns false.

Allocation-null and dependent interface failures are not consistently checked before later
dereferences. This is native data/child construction, not a safe all-or-nothing transaction.
The full resource parser, graphics submission and GPU residency remain outside this slice.

## Evidence and next boundary

Decoded focused slices cover `7535A0..7535DC`, `752680..7526A1`, `751E50..751F3F`,
`7515D0..75174E`, `7517FE..751B92` in bounded completion/replay slices,
`753690..75378D`, `7532D0..75334E` and `7533D7..753439`,
`74A990..74ACEB` in successive slices, `754C00..754C5C`, `7549C0..754BF0`,
`740F10..740F7C`, `742BB0..742CFC`, `741AC0..741B19`, and interface candidate
`C88680..C8869E`. Selected predicates/slots/calls are pinned in
`tools/verify_startup_state_route.py`. Cached disassembly is temporary, not an imported runtime.

The owner notification is resolved by the follow-up above; its flag's later role remains
Unknown. The outer-frame follow-up also closes manager `+0x38` scheduling. Next choose a
concrete actor/input consumer rather than expanding child resource helpers indefinitely as a
substitute for gameplay.
