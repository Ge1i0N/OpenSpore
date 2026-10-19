# Scenario avatar asset, profile setup and conditional cleanup

## Authority and scope

- Artifact: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, x86 PE.
- SHA-256: `086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
- Addresses: preferred image VAs, not runtime ASLR addresses.
- Method: short `objdump` slices, file-backed bytes and raw candidate discovery.
- Execution: none. No original game, resource service, callback or save-writing path was run.
- Question: what produces the marker resource key, what does `C03920` actually accept, and
  can its failure clear the previously assigned current-creature reference?

Continuation of [Scenario herd assignment](startup-gge-scenario-herd-assignment-join.md).

## Finding

**Observed (static):** Scenario data `+0x130` is associated with binary metadata name
`mAvatarAsset`. It feeds the previously recovered conditional ownership-enabled creation path.
`C03920` returns the presence of a species-profile pointer, not model/world/input readiness.
Its nested model setup has a different success predicate whose result is not propagated here.
The failure path contains a conditional current-reference clear through the creature's cleanup
virtual. A complete transactional rollback and an actual selected captain asset remain Unknown.

```text
Scenario data +0x130 (mAvatarAsset)
  → F432C0 → F3F8C0 → EEDD40 → B23920 → cHerd +0x54
  → C09B40(..., assignment=1, reuse=0)
  → B1FCA0(creature), possibly update default species key
  → C03920(profile, assignment, 0)
       → C22450: store/resolve profile and optionally attempt model setup
       → return creature +0xB20 != null
  false → B22560 → B20CC0 → virtual +0x44 = C083D0
       → if manager current reference == this creature: B1FCA0(null)
```

The arrows retain the gates and conditional factory selection from the preceding audit.

## Resource key: named field, fallback query and normalization

Metadata initializer `138CF2A..138CF84` associates ASCII `mAvatarAsset` at `0x0148CCB0`
with offset `0x130`, property ID `0x07102E78`, and handlers `F26A60/F26AA0` in a descriptor
copied to `0x015AF938`. A separate descriptor initializer `138D56E..138D5D2` uses the same
name/offset with ID `0x07905D42` and destination `0x015AFD38`. Keep those descriptor IDs
distinct; do not infer one serialized identity across formats from the common name.

`F26A60/F26AA0` delegate compound serialization through metadata `0x015ADBD0`; their final
`AL=1` does not prove successful resource acquisition. The name identifies the field role but
does not tell us the concrete key carried by a live Scenario launch.

Bounded producer observations:

| Address/path | Observed effect | Limit |
|---|---|---|
| `F432C0`, missing marker record | Validate data `+0x130`; failed/zero-key path calls `F42EB0`, copies three result words to `+0x130/+0x134/+0x138`. | Existing valid key is retained; actual key and service result are unobserved. |
| `F447C3`, `F44CA9` | Other decoded setup/reset prefixes call `F42EB0` and copy its tuple to the same data field. | Their complete entry/reachability contracts were not traced here. |
| `F43DC0 → F43EF4..F43F2D` | If `F3C1D0(data +0x130)` returns true, rewrite the tuple's middle word `+0x134` to `0x2B978C46` and submit a by-value tuple to `F43A90`. | This is conditional normalization/replacement, not unconditional captain selection. |
| `F43A90 → F43D38..F43D73` | After replacement-key processing, compare all three current avatar-key words with the supplied tuple; on equality copy the local replacement tuple into `mAvatarAsset`. | If replacement helper fails the local tuple can retain the supplied key; not every call selects a new asset. |
| `F43D79..F43D99` | If owner marker record exists, obtain current creature and call `F41A50` with the replacement key. | This can update an existing object; it is not a spawn producer. |

The raw store scan covered `EF0000..F46FFF` for selected `89/C7` base+disp32 forms only.
Not all hits refer to Scenario data; e.g. `F32040` uses offsets `+0x130..+0x138` as a vector
on another receiver. Offset equality is not object identity. No exhaustive writer inventory is
claimed. The historical conversion slice near `F2FBxx` was not followed to its enclosing entry.

`F3C1D0` distinguishes empty and malformed tuples: an all-zero tuple returns false; first word
zero with another word nonzero returns true. Nonzero-first-word keys go through a service load,
interface `0x03C609F8` and further checks. Failed load can also produce true. It is not simply
a "key exists" predicate; runtime availability was not tested.

`F42EB0` builds three query criteria through `558960`, preserving immediate pairs:

```text
0x02DC9D1E, 0, 0x4178B8E8
0x54A32960, 0, 1
0x03CC89B1, 0, 0x913B23BE
```

It queries service `67CB00`, virtual `+0x30`, with an output vector, value `1` and criteria.
Only when the service returns true **and** begin != end does it copy the first three-word
result. Otherwise it writes an all-zero tuple. That fallback does not distinguish a failed
query from an empty result. The property/operator meanings and concrete selected asset remain
Unknown; do not label these IDs captain-selection semantics solely by proximity.

`F41A50`'s class `0x018EB45E` branch queries interface through `AC88A0`, resolves the supplied
key through `4DF5C0`, and calls `C22450` only on a non-null profile. If object bit `0x200` is
set it additionally calls `4DF450(profile)`. This is an existing-creature profile update path,
not evidence that every `mAvatarAsset` change reconstructs a captain.

## `C03920`: exact inputs and return

`C09C6D..C09C77` passes three arguments:

```text
C03920(profile, C09B40.assignmentArgument, 0)
```

At entry, after the first saved register, `[ESP+0x10]` is argument three, loaded into `BL`.
Thus this caller supplies `BL=0` regardless of assignment being enabled. This flag must not
be conflated with `C09B40` argument five. The nonzero-BL update branch through `C14010` is
not taken on this path. `C03938 → C22450(profile)` runs first, followed by the zero-BL path
through `C211F0`, transform-related setup and virtual operations.

The entire return at `C03A94..C03AA4` is:

```text
EAX = 0
AL = (creature +0xB20 != null)
ret 0x0C
```

There is no combined success value for model creation, attachment or input. Later helper
results and the presence of a selected current reference are not part of this return predicate.

## Species profile versus model setup

`C22450` writes incoming pointer directly at creature `+0xB20`; when null it asks
`401090 → 4DF490` for the manager-default species profile and stores that result. Non-null
profiles supply `+0x53C` to creature `+0xB24`, and their three-word `+0x504` key to creature
`+0xB28..+0xB30`. It calculates change/equality flags before this fallback resolution.

The `4DF5C0` lookup/materialization route is independently decoded: it checks a cache, can
substitute manager key `+0xA4`, calls `4DA400`, and on an uncached successful path allocates
`0xA18` labelled `Simulator/cSpeciesProfile` and constructs via `4D3E30`. Allocation/lookup
and native profile success remain conditional; see also [StarRecord post-load](starsdb-postload.md).

Unless global byte `0x0168D904` is nonzero, `C22502..C22517` calls
`C21D50(changeFlag, pointerEqualFlag)`. The returned value is not tested; the next operation
loads the creature vtable and invokes `+0xAC`. Other profile-derived index/policy updates follow.

Within `C21D50`:

- model reference field is creature `+0xB54`;
- replacement gates can clear/release the old reference;
- profile and selected profile key must exist; key selection distinguishes profile `+0x510`
  and `+0x504`, using creature `+0xB34`;
- selected bit `0x200` plus mode tag `0x01654C05` takes a separate service path; the other
  path calls `B34050`, tests service presence, and invokes its virtual `+0x30` with key,
  transform/position context and control values;
- `59ACB0` stores the service result at `+0xB54`, followed by a null gate;
- `C21FED → A04A30` associates the new model-side receiver with the creature; selected-object
  bit `0x200` also controls later model/service virtual operations;
- final Boolean at `C22144..C2216B` requires **both** model `+0xB54` and model `+0x17C`.

`59ACB0` retains/stores/releases the model reference. `A04A30` conditionally retains/replaces
the creature at model-side `+0x180 → +0x64`, then walks additional entries through model
`+0x17C → +0x2C0` and similarly replaces their `+0x64` references. The inspected body
dereferences model `+0x17C` without a null gate at that point. This is concrete object
association, not a checked scene-attachment transaction.

The [resource-materialization audit](startup-gge-model-resource-materialization.md) establishes
that the positive herd caller stores `+0xB34=1` and so selects profile `+0x504`, not `+0x510`.
It separately follows the later `A05B60` producer of instance `+0x180`; this is not input/world
readiness.

The `B34050` global is populated by a manager virtual call carrying UTF-16 `Gonzago` in the
examined `B357B1..B357DD` prefix. The later
[animation-world audit](startup-gge-captain-key-anim-world.md) resolves that constructed manager
slot as world creation (`A0BB30`), not lookup, and pins the resulting model-provider slot.
`C21600` internals and full scene-registration semantics remain outside this slice.
Therefore model-side association is established statically, full scene attachment
and resource completion are not.

**Important failure distinction:** `C21D50` can return false while `+0xB20` remains non-null;
its Boolean is discarded by `C22450`, and `C03920` can still return true. Static code presence
does not establish that either outcome occurred at runtime.

## Failed setup: current-reference cleanup is conditional, not a full rollback

Before setup, assignment-enabled `C09B40` calls `B1FCA0(creature)`. If incoming profile differs
from the previously resolved default, it also calls `4DF450(profile)` before `C03920`.
`4DF450 → 4DF380` copies profile key `+0x504` to manager default key `+0xA4`, then performs
service/message work. A later creature failure does not show restoration of this old key.

When `C03920` returns false, `C09C80..C09C96` invokes `B22560(creature)` and returns null.
`B22560` marks object byte `+0x20`, retains/queues the object, calls `B20CC0`, and then
`B200F0`. In `B20CC0`, virtual `+0x2C` is the already-cleaned guard. Under the constructed
`cCreatureAnimal` table `0x0146A080`, `+0x2C = B18310` returns byte `+0x21` and
`+0x44 = C083D0`. Base constructor `B185A0`, called through `C1FA30`, initializes `+0x21=0`.

If not already cleaned, `B20CC0` sends lifecycle payload code `4` and invokes virtual `+0x44`.
`C083EA..C08403` then compares `B1FD00(manager)` with the cleanup receiver. On equality it
calls `B1FCA0(null)`, clearing/releasing manager `+0x54` and the selected-object bit. The
caller later marks `+0x21=1` and continues release/list handling.

This closes a **conditional reference-clearing join** for the known factory/table. It does not
restore a previous creature reference, default profile key, all associations or external service
state. Message callbacks and alternate runtime vtables remain outside observed execution.

## Coverage, guards and next boundary

Decoded ranges include `138CF2A..138CF84`, `138D560..138D5D3`, `F26A60..F26AD7`,
`F3C1D0..F3C2B7`, `F43DC0..F43F45` (decisive normalization tail),
`F43A90..F43B10` and `F43C50..F43DBD`, `F41A50..F41B82`, focused `F42EB0` query/result
slices, `C09B40..C09C96`, `C03920..C03AA4`, `C22450..C225C8`, `C21D50..C2216B`,
`4DF380..4DF4AB`, `4DF5C0..4DF732`, `B22560..B225D6`, `B20CC0..B20DC8`, and
`C083D0..C0845E`, `59ACB0..59ACDE` and `A04A30..A04AEF`.
Raw store/pointer scan hits were discovery only, not exhaustive decoded xrefs.
Selected instruction/slot/label anchors are in `tools/verify_startup_state_route.py`.

The [selected-key/animation-world follow-up](startup-gge-captain-key-anim-world.md) connects
controller selection through record `+0x18` and `F42420` to `mAvatarAsset`, after first Scenario
synchronization. It also resolves the constructed Gonzago provider `+0x30` to `A0B8E0/A0B1C0`.
Its model/resource success remains conditional. Native startup delivery, playable stage/world,
input and completed gameplay ticks remain
Unknown. Object-74 `Play Scenario 0x070D7509` has no new producer/delivery join.
