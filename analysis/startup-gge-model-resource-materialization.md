# Captain model resource acquisition and model-data acceptance

## Authority and scope

Selected x86 GA PE `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Preferred-image VAs; read-only short `objdump` slices, cached disassembly and PE bytes.
No original process, resource load, native factory, scene attachment, input or save write ran.
Continuation of [selected captain key and animation world](startup-gge-captain-key-anim-world.md).

## Result

**Observed (static):** animation-world creation does not load the supplied key unchanged.
`4BB500` first substitutes resource type `0x0F43029A` and queries interface `0x03E1C247`.
It has a distinct group-variant recursion and an original-key conversion fallback.
`9CBE10` expands the acquired resource into model-data records, remaps hierarchy indices,
computes transforms/bounds and returns the result of `9C4950`. That finalizer requires
structural acceptance and a final record count no greater than `255`.

**Observed (static):** the returned animation instance can acquire/reuse a scene-side object
through `A05B60`; this supplies the `+0x180` receiver used by the already recovered creature
association helper. This does not establish a renderable actor, completed attachment or input.

**Unknown:** actual selected resource contents/availability, native load success, complete
conversion internals, concrete scene-side factory class, input ownership and playable readiness.

## Which key enters the model loader?

`C09B40` stores its third argument at creature `+0xB34` (`C09BC4`). The positive herd caller
`C6E861` supplies `1`. `C21D50` uses that field to select the three-word profile key:

```text
profile == null → skip this acquisition path
creature +0xB34 == 0 → profile +0x510/+0x514/+0x518
creature +0xB34 != 0 → profile +0x504/+0x508/+0x50C
selected key word 0 == 0 → skip this acquisition path
```

Thus the examined assignment-enabled herd route uses profile `+0x504`, not the alternative
`+0x510` key. The later selected-captain update supplies a new profile through `C22450`; this
routine does not itself rewrite `+0xB34` in the examined body. Other writers/runtime mutations
are not exhaustively covered. No extra semantic age/type name is assigned to these two profile
key fields just from their offsets.

The normal Gonzago world call eventually reaches
`A0B1C0(key, 0, 0, 0, position, orientation, 1)`. On the uncached path it allocates model data,
calls `4BB500(key, &resource)`, then `9CBE10(data, resource)`. Both returns are tested before
the ordinary context/instance continuation. A failed acquisition/materialization releases
temporary references and reaches null return `A0B367`. Cache hits can bypass these operations;
these are not claims about every model creation.

## `4BB500`: type substitution, variant and fallback

For input tuple `(word0, word1, word2)`, `4BB511..4BB537` constructs:

```text
derived = (input.word0, 0x0F43029A, input.word2)
variant = (input.word2 >> 24) & 0x1F
```

Word positions are retained exactly; no conversion of every key namespace is inferred.

### Variant `1`

`4BB55B..4BB5BC` clears those five group bits (`& 0xE0FFFFFF`), preserves input words 0/1,
and recursively calls `4BB500` on the normalized tuple. Recursive false returns false after
release. On true, it allocates `0x128`, copies through `46AB00`, applies `46AD70`, replaces
resource key fields `+0x08/+0x0C/+0x10` with the derived tuple, transfers the result to the
output and returns true. The constructor installs table `0x013EED80`; its `+0x0C = 4BB1C0`
accepts interface `0x03E1C247`.

The later diagnostic string suffix ` (baby)` corroborates a variant label, not a complete
semantic interpretation of every group flag. Allocation-null paths are not a checked transaction:
some continue into helpers/dereferences. No native variant conversion was executed.

### Other variants: derived resource first

`4BB6D2..4BB6F0` invokes resource service `67DC90`, virtual `+0x0C`, with the derived key,
an output address and four zero controls. If that call returns true, `4BC4A0` queries returned
object virtual `+0x0C(0x03E1C247)`:

- non-null requested interface: store it in caller output, transfer ownership of the retained
  resource reference, return true;
- null interface: release the local resource and return false, **without** taking original-key
  conversion merely because the interface query failed.

This is a concrete request/interface acceptance route, not observed mounting or file parsing.

### Derived service call false: original-key conversion fallback

Only a false service result at the earlier gate leads to `4BB764`. It requests the **original**
input tuple through the same service slot. A true result is adapted through `421E70`; the
returned object's `+0x18` identity must be one of these ten inspected constants:

```text
DFAD9F51  9EA3031A  372E2C04  CCC35C46  65672ADE
4178B8E8  37148141  B8669EC9  BCD73E89  06D5C48C
```

On an accepted identity it constructs a new `0x128` resource (`4BA2E0`, same table
`0x013EED80`) and calls `46DD10(originalObject, newResource)`. False conversion releases
intermediates and returns false. On true it sets the derived key, performs additional helper
calls `6AC060`, `6AD030` and a property-gated `6B1F90`, transfers output and returns true.
Those helpers' full effects and the converter internals remain outside this slice.

Failure paths do not uniformly write a null to caller output. The animation caller explicitly
clears/releases its own local output before calling. Keep Failed distinct from an Empty output
or unavailable resource observation.

## `9CBE10`: resource → expanded model data

Two stack arguments are consumed as data destination and acquired resource. The function:

1. resets destination through `9C1CE0(0)`;
2. copies resource key-derived fields from `+0x08/+0x10` to data `+0x318/+0x31C`, classifies
   resource `+0x18` through `4BBDA0` into data `+0x320`;
3. builds a pollen-metadata key with type `0x030BDEE3`, clears the variant group bits for that
   query, and formats an optional name; absent metadata yields literal `No Pollen Metadata`;
4. copies transform-related values and sizes data vector `+0x384` from resource range
   `+0x98/+0x9C`, whose inspected source records have stride `0x8C`;
5. expands/remaps them into `0x468`-byte destination records: parent/root indices, flags,
   identifiers, channel-like arrays and transform/bounds arithmetic;
6. conditionally invokes `9CBCE0`, then always invokes finalizer `9C4950(data)`;
7. on accepted finalization, builds a bounded index map and expands the second resource range
   `+0x114/+0x118` into destination range `+0x398` with transform-related values;
8. returns the saved finalizer Boolean, not a conjunction of every subordinate helper result.

The full decoded `9CBE10..9CCF00` was cached once outside the repository and read in bounded
sections. Arithmetic/layout observations do not assign unproven skeleton/bone names to every
record. The `joint_b%d_n%d` diagnostic is a useful hint, not a complete recovered schema.
Record copying and metadata naming are not scene registration.

### Exact final acceptance boundary

At `9CCCC8` the caller invokes `9C4950`, stores `AL` at local `+0x13`, and branches directly
to epilogue when false. Its accepted post-processing restores that saved byte before return.
`9C4950` first calls `9C31E0(data, 0)`. The inspected early checks include:

```text
data +0x384 range nonempty
selected index (0 here) < record count
selected record +0x154 bit 0x08 set
selected record +0x1FC == -1
```

Further structural/hierarchy processing can fail; this report does not replace the whole
predicate with those four early checks. On success the finalizer computes bounds and other
derived fields, then `9C4A99..9C4AA7` returns unsigned `recordCount <= 0xFF` for stride
`0x468`. A larger count returns false; an empty vector already fails the earlier structural
gate. It does not clamp the vector to `255` or guarantee GPU/animation/world readiness.

## `A05B60`: next scene-side object association, not input activation

After the previous animation-world instance construction, `A05B60(instance)` handles:

```text
context = instance +0x17C
optional = context +0x2C0
optional != null → instance +0x180 = optional +0x08, return
otherwise if instance +0x180 != null → return
otherwise:
  world = instance +0x190
  service = world +0x44
  service virtual +0x0C(0x2EBCD6A6, 0xD94352ED, 0)
  instance +0x180 = result
  increment result +0x40, set result +0x04 bit 0x200
  update result +0x70 bounds and result +0x6C from model data
```

The factory-like result is dereferenced for `+0x40` **before** the subsequent null test.
Its class/interface and success are not established by the two ID arguments alone. This
installs the scene-side pointer used by `A04A30`, which retains/replaces the creature at
that object's `+0x64`. The earlier `A04A30` report remains valid; the new join explains one
concrete producer of its receiver. No input owner, active controller or completed frame is
established by reference association and bounding-volume writes.

## Evidence, regression and next edge

Decoded `4BB500..4BB9C7`, `4BC4A0..4BC4E6`, `4BB1C0..4BB1EF`, focused constructors
`4BA2E0/46AB00`, conversion prefix `46DD10..46DDFD`, complete cached
`9CBE10..9CCF00`, `9C4950..9C4AAE`, structural prefix `9C31E0..9C330F` and its
true/false tails, `A05B60..A05C94`, and caller/profile-key slices. Raw constant/return scans
were candidate discovery only. Selected anchors are in `tools/verify_startup_state_route.py`.

The [scene-container follow-up](startup-gge-animation-scene-container.md) identifies world
`+0x44` as the constructed container table `0x0140D7D8` and its `+0x0C = 74D520` definition
request. It registers a pooled wrapper and returns wrapper `+8`, with separate immediate and
deferred initialization. The runtime queue/update and input contracts remain unresolved.
Alternatively expand the resource converter if an actual failed model input requires it.
Do not promote resource/interface acceptance, model-data validity, instance existence or
scene-side reference assignment into observed playable readiness. Object-74 delivery remains
Unknown.
