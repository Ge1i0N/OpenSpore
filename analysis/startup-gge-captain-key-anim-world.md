# Selected captain key to avatar profile; Gonzago animation-world dispatch

## Authority and scope

Selected `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
x86 PE preferred image VAs. Short `objdump` slices and file-backed byte/table reads only;
no original process, resource loading, callback, save or native allocation was executed.
Continuation of [avatar asset/profile setup](startup-gge-avatar-asset-profile-setup.md).

Two bounded questions: does the controller's selected captain key reach `mAvatarAsset`, and
which constructed provider supplies the model-side `Gonzago +0x30` call?

## Result: two static joins, not native captain success

**Observed (static):** in the play-completion branch, controller key `+0xA4` is copied to launch
record `+0x18`, then after successful Scenario loading, `EF1D20` calls `F42420` to write it into
Scenario data `+0x130` (`mAvatarAsset`). If a marker record exists, it additionally submits
the selected key to the **existing current object**, with a creature-specific profile-update
branch. This is not evidence that the selected key was used during the first creature allocation.

**Observed (static):** startup constructs an animation manager whose virtual `+0x20` creates a
new animation world. Simulator requests that operation with the name `Gonzago`, retains its
result, and the resulting world table has `+0x30 = A0B8E0 → A0B1C0`. The latter constructs
model-side data/context/instance objects under resource and allocation conditions.

**Unknown:** native delivery, live selected key, successful resource/model construction, game
world/stage activation, input ownership and completed gameplay ticks. Object-74
`Play Scenario 0x070D7509` still has no new producer/delivery join.

## Controller selection → launch-record override

The [selection-flow audit](audit/scenario-mode/selection-flow.md) supplies the UI/controller
context. The decisive decoded paths are:

```text
E00170: nonzero first key word, second word == 0x2B978C46
  → DF9CF0(key)
DF9CF0: local state == 2 (SelectingAvatarForPlay)
  → controller +0xA4/+0xA8/+0xAC = key
DFBD40: completion branch other than local state 9 (includes guarded state 10)
  → launch record +0x24 = 2, +0x28 = 1
  → launch record +0x18/+0x1C/+0x20 = controller +0xA4/+0xA8/+0xAC
```

`DF9CF0` handles state 4 differently, writing a posse record; do not identify every key in the
`0x2B978C46` family as the captain selection. The state-2 writer is the concrete join here.
`DFBD40` can perform captain-related conversion/name/service work before its final key copy;
this report does not execute or infer the result of those operations. No nonzero key is
fabricated if selection is unavailable.

`EF1D20` loads the controller using shared record base `0x015ACCC8`. It records whether the
primary Scenario-key first word is zero (`EF1E81..EF1E9D`). Only when that Boolean is false
does it call `F464D0(recordBase)` at `EF1F19`. Only after that routine returns true and
`record +0x18` first word (`0x015ACCE0`) is nonzero does `EF1F33` call `F42420(record +0x18)`.

`F42420` copies all three supplied words to controller data `+0x130..+0x138`. If controller
marker pointer `+0x8C` exists, it then gets `B1FD00(manager)` and invokes
`F41A50(currentObject, controller, key)` (object in EAX, remaining arguments on stack).
It does not allocate a creature or call the current-reference setter itself.

In `F41A50`, component/interface `0x0175CDC9` with field `+0x38 == 9` can skip the update.
Otherwise native class ID `0x018EB45E` selects the creature branch:

```text
AC88A0 → query interface 0xD0036E08
401090 → 4DF5C0(selectedKey)
non-null profile → C22450(creature, profile)
object +0xB58 bit 0x200 → additionally 4DF450(profile), update default species key
```

As established in the previous audit, `C22450` can attempt model replacement but does not
propagate its success. No runtime resource/profile/update result was observed. Null current
object is not cleanly handled by the full examined `F41A50` body; marker presence is not a
proof that the current-creature query succeeded.

## Ordering: first synchronization precedes the selected-key override

`F464D0` first calls `F44A90` and returns false if it fails. That loader builds resource keys,
queries interfaces including `0xE742574A` and copies loaded data via `DFFA00`. On the accepted
path it calls `F2B270` and `F43DC0`, then performs:

```text
F411A0: clear controller maps/marker record, with further lifecycle work
F45AD0(strategy, selectedIndex, 1, 1)
B228F0
F45AD0(strategy, selectedIndex, 0, 0)
... further resource/service and bookkeeping work ...
F4666D: local return latch BL = 1
F46726: return BL
```

`F45AD0`'s first pass can create the marker/native-object route before `F464D0` returns, under
its existing transform/control/resource gates. `EF1D20` applies the launch-record captain key
**after** that return. The load return latch is not a conjunction of all object/model setup
results. Thus the decoded route supports an initial Scenario-data/fallback profile followed
by selected-captain profile replacement on the existing creature. This is an ordering inference
under the shown gates, not proof that two models were successfully created in a live launch.

## Concrete provider construction: `Gonzago` is an animation-world name

The provider chain, conditional on initialization/allocations and later global replacement:

```text
5D819E → A0BBC0: allocate 0x88, label Anim/Manager/singleton
  → A0B910: primary table 0x01448F90
5D81D4 → 67CBE0: publish manager at global 0x015FCC6C
67CAE0: return that manager
B357B1..B357DD, only if global 0x0167EA58 == null:
  manager virtual +0x20(UTF-16 "Gonzago") = A0BB30
    → allocate 0x303C, label Anim/Manager/new_world
    → A09DD0(manager, name), primary table 0x01448F10
    → retain in manager world collection +0x68
  retain/store result at global 0x0167EA58
B34050: return retained world
```

The earlier "named service lookup" shorthand was incomplete: for this constructed manager
table the operation creates a world, not a name lookup. The UTF-16 name is copied into the
world's local storage; no inference from the word `Gonzago` alone is needed for this dispatch.
The existing-nonnull-global branch skips creation. Publisher failure/cleanup paths and runtime
replacement remain separate from the positive construction chain.

Simulator then invokes world `+0x1C` with a separate object and controls; for this table that is
`A05450`. It retains a pointer at world `+0x44` and writes controls including `+0x48`,
`+0x3039/+0x303A`. Its unconditional true return is not model-readiness proof. An animation
world/container is **not** a terrain/planet/stage constructor just because it is called world.

## `+0x30 → A0B8E0 → A0B1C0`: bounded model creation contract

The `C21D50` normal provider path calls world `+0x30` with five stack arguments:

```text
(keyPointer, 0, positionPointer, orientationPointer, 1)
A0B8E0 → A0B1C0(keyPointer, 0, 0, 0, positionPointer, orientationPointer, 1)
```

This is distinct from animation manager `+0x30 = A0D4E0`; the receiver table matters.
With the final control nonzero, `A0B1C0` derives an internal identity from key word 0 and word 2,
checks a manager-side cache, and can reuse retained data. On a miss it allocates `0x410`
(`Anim/World/new_csd`, constructor `9C1E80`), tries resource load `4BB500(key, output)` and
then `9CBE10(data, resource)`. The shared failed-acquisition branch releases intermediates
and returns null at `A0B367..A0B370`. The loader/materializer internals were not fully traced.

After accepted/reused data it allocates a `0x1930` context (`Anim/World/new_cid`, `9C55E0`),
invokes `9C51C0` with position/orientation and other context, then allocates a `0x19C`
instance (`Anim/World/new_ac`, `A05CA0`). It retains/stores the context in instance `+0x17C`,
sets world back-reference `+0x190`, appends to world `+0x24` vector, invokes `A05B60`, releases
temporary references and returns the instance at `A0B7F2`.

Optional `new_cmid`/world `+0x44` processing has its own status: failed optional setup can
clear context `+0x2C0` and set a local failure flag that later contributes bit `0x2` to instance
`+0x88`. It does not always return null. Several allocation-null paths continue to dereferences
without a clean transaction boundary. `9C51C0` also has a false path; its return is not checked
at the examined caller. Do not model this as one checked all-or-nothing successful factory.

The output feeds the prior chain `59ACB0 → creature +0xB54`, then model-side association through
`A04A30`. Even a non-null instance/context remains different from completed model loading,
scene attachment, enabled input or successful simulation.

## Evidence and continuation

Focused decoded slices: `E00170..E0022C`, `DF9CF0..DF9D85`, `DFBD40..DFBF42`,
`EF1E81..EF1F42`, `F42420..F42461`, `F41A50..F41B0E`, focused loader/sync/latch slices
in `F44A90..F44C64` and `F464D0..F4672E`, `5D818F..5D820D`, `67CAE0/67CBE0`,
`A0BBC0..A0BBE7`, `A0B910..A0B9A7`, `A0BB30..A0BBBA`, `A09DD0..A09E9F`,
`B357A4..B35864`, `A05450..A054B3`, `A0B8E0..A0B904`, and the bounded subregions of
`A0B1C0..A0B7FC` described above. Pinned anchors are in `tools/verify_startup_state_route.py`.
Raw scans found candidates only; alternate global writers, indirect edges and model internals
are not exhaustively covered.

Next useful boundary is the resource load/materialization `4BB500 → 9CBE10` and the selected
captain profile's model key choice on replacement. Follow the native result to an actual
attachment/input consumer rather than treating the animation-world instance as a playable actor.
