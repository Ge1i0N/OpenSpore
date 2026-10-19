# Animation scene-side container: definition request, wrapper and registration

## Authority and question

Selected x86 PE `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Preferred image VAs; short read-only `objdump` slices and PE table/byte inspection.
No original execution, resource service, callbacks, save writes or native allocations ran.
Continuation of [model resource materialization](startup-gge-model-resource-materialization.md).

Question: which concrete service occupies animation world `+0x44`, what does its
`+0x0C(0x2EBCD6A6, 0xD94352ED, 0)` return, and where is that result registered?

## Finding

**Observed (static):** the constructed provider chain reaches container table `0x0140D7D8`,
slot `+0x0C = 74D520`. This is a definition-loading and pooled-wrapper creation path, not a
Simulator creature factory or query-interface call. Its successful normal path registers a
wrapper in a container list and returns the embedded object at **wrapper +8**. Model instance
`+0x180` receives that embedded pointer, whose later `+0x64` association can retain the creature.

**Observed (static):** initialization may be immediate or deferred through a container queue;
the queue has a decoded consumer calling the same initializer. Wrapper existence therefore
does not imply finished initialization, actual rendering, input activation or a successful tick.

**Unknown:** live manager/container selection, definition-resource availability/content,
initialization completion, rendering, scene integration beyond the bounded registration, input
ownership and playable-world readiness. Object-74 `Play Scenario 0x070D7509` is unchanged.

## Provider identity and table join

The [animation-world audit](startup-gge-captain-key-anim-world.md) establishes world
`0x01448F10 +0x1C = A05450`, which retains its first argument at world `+0x44`.
The corresponding Simulator call gets this argument from `B3D620` and invokes world `+0x1C`
with that pointer and controls `(1,1,1,0)` at `B35808..B3581C`.

The concrete chain under the constructed startup tables is:

```text
7A7FCA → 74FEE0 → allocate 0x60 → 74FE00
  primary manager table 0x0140D768
7A8003 → 67DF50 → publish global 0x015FD8CC
67DD40 → return manager
B3D620 → manager +0x1C(0x00EB9968)
  table +0x1C = 746D30 → keyed manager-map lookup
B3AF54 fallback if that lookup was null:
  manager +0x14(0x00EB9968, 0, 0)
  table +0x14 = 753D70
    allocate 0x8B8 → 753960(manager, thirdArgumentByte)
    primary container table 0x0140D7D8
    initialize through container +0x174 = 752060
    retain/store container in manager map +0x0C under supplied identity
B357F5 → B3D620 → container
  → A05450 retains container at animation world +0x44
A05B60 → container +0x0C = 74D520
```

`B3D620` itself does not create the container; it performs a lookup and a balanced retain/release
before returning. The separate caller fallback is required for the positive construction path.
Existing manager-map values, runtime table changes and other creation callers are not excluded.
Generic allocation label `0x013EB8A4` is not a concrete class name; the report keeps addresses
and table roles rather than inferring a Simulator `cSpatialObject` from nearby string hits.

`753D70` consumes three stack arguments; the first is used as the manager map key, the third
low byte becomes container `+0x1E` through `753960`. The inspected Simulator fallback supplies
zero for that byte. The second argument is not assigned an unproven semantic type here.
`752060` marks container `+0x1C`, registers two message listeners through service `+0x20`,
initializes local slots and returns true; those subscriptions are not message publications or
proof of resource/render success.

## `74D520`: pair of IDs → definition → embedded object

The animation caller's three arguments are retained exactly:

```text
arg1 = 0x2EBCD6A6
arg2 = 0xD94352ED
arg3 = 0
```

`74D548` obtains service `67DDF0` (global `0x015FD8A8`), and `74D562..74D578` passes the
two ID arguments and a local output address to service virtual `+0x2C`. This is a loaded
definition/resource request. The IDs are not proven Simulator class/interface identities.
Separate resource registration/configuration calls elsewhere use the same pair; their full
semantics were not followed, and are not needed to identify this decoded consumer.

### Resource request true

The normal branch obtains a record from pool/free list rooted at `0x0162ED08/0x0162ED18`,
constructing via `746EC0(container)` when a usable record is obtained. The constructor creates
an embedded object at record `+8` with helper `746000`, initializes child/reference slots,
flags, transform-related defaults and links to its owner container. This record is not a
new `cCreatureAnimal`, nor is the embedded pointer shown to carry its own C++ vtable.

For `arg3 = 0`, bit 31 is clear, so `74D5CD..74D5E6` links the record into the container's
list with sentinel `+0x19C`, marks record `+0x0C |= 0x8000`, then sets bit `1` and stores
the control word at record `+0x138`. Other controls can select a separate list or setup path.
It retains/stores the loaded definition at record `+0x98` and increments container `+0x894`.

Control bit `0x10` takes direct `74B7D0`; the examined zero control instead calls `74C980`.
After that call, it snapshots record flags into `+0x12C`, adjusts the pointer by `+8`, and
returns it. This is the exact origin of model instance `+0x180` in the normal branch.

### Resource request false

`74D6D7` checks container byte `+0x1E`. If zero, the method reaches null return
`74D8C3..74D8D0`. Thus the constructed Simulator fallback container does **not** enable the
alternate failed-load fallback by default.

If that byte is nonzero, a separate branch calls `754C00`, can try a different hardcoded pair,
build another record, and returns another embedded pointer on success. This report records
the branch but does not promote it to the normal Scenario path or claim fallback success.

Allocation/pool failure is not a clean checked transaction throughout. Independently,
`A05B60` stores the returned pointer then increments result `+0x40` before testing null.
Do not replace a missing definition or failed operation with a safe/ready placeholder.

## Immediate versus deferred initialization

`74C980(container, record, definition, arg1, arg2)` clears a flag, checks control/sign state,
examines eight container-local slots and evaluates a service/property-dependent condition.
The decisive split is:

```text
deferred branch:
  capture record +8, retain definition, preserve IDs/control and callback fields
  74BE10(container +0x314, entry)
  return without calling 74A990 here
immediate branch:
  74BEB0(container +0x314, &embeddedPointer) removes/clears matching queued entries
  74A990(container, definition, record, control)
```

The queued entry layout is `0x1C` bytes in the inspected consumer. At `751A90..751ADF`,
the consumer retains the embedded record reference and definition, subtracts `8` to recover
the wrapper, and invokes `74A990` with the stored owner/control. It can then invoke a stored
callback and release temporary references. The enclosing consumer's scheduling/tick join is
not established by that local callsite; queue submission does not prove consumption occurred.

`74A990` begins by clearing/releasing per-record child reference slots, then queries definition
properties and delegates selected parts through `754C00`. This is an initialization/child
configuration boundary, not a Boolean whole-scene-ready contract. Its full definition schema,
downstream rendering primitives and per-frame update are not recovered in this pass.

## Joining back to the creature without conflating ownership and input

The previous audit's instance pointer and flags now have a concrete producer:

```text
74D520 → wrapper +8
  → instance +0x180
  → A05B60 reference count, flag and bounds writes
  → C21FED → A04A30(instance, creature)
  → retain/replace creature at embeddedObject +0x64 (wrapper +0x6C)
```

`A04A30` also handles model-context child associations separately. The offsets are relative
to their actual receivers; embeddedObject `+0x64` is not container `+0x64` or wrapper `+0x64`.
This is a real static retention/association join. It does not establish that queued child
initialization completed, that the object was rendered, or that a controller owns input.

## Evidence, guards and continuation

Decoded slices: `7A7FCA..7A8012`, `74FEE0..74FF04`, `74FE00..74FE5E`,
`67DD40/67DF50`, `B3D620..B3D64E`, `746D30..746D5E`, `B3AF33..B3AF77`,
`753D70..753E1D`, constructor parts `753960..753A5B` and `753B4D..753C05`,
`752060..75214F`, `B357F5..B35825`, `74D520..74D8D2` in successive focused slices,
`746EC0..747028`, `746000..7460BA`, `74C980..74CAD9`, queue-cancel prefix
`74BEB0..74BF59`, queued consumer `751A7D..751B4A`, and `74A990..74AB0F` prefix.
Selected slots, argument/control predicates, pointer adjustment, links and direct edges are
pinned in `tools/verify_startup_state_route.py`. Raw ID/table/edge scans found candidates only.

Next: decode `74A990`'s concrete definition-to-render-object children and/or establish the
container queue/update consumer's Scenario tick callsite. Input activation is a separate
unresolved contract; do not expand resource configuration indefinitely as if it were input.
