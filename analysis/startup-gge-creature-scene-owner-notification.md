# Creature scene-owner notification: interface join and flag-only callback

## Authority and scope

Selected x86 PE `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Preferred image VAs; short read-only `objdump` slices and PE table/byte inspection.
No native execution, async completion, callback invocation, input or save writing occurred.
Continuation of [scene completion/children](startup-gge-scene-queue-completion-children.md).

Question: can the optional `0x07BBC4FF` owner notification reach the constructed creature,
and does its first slot activate the captain or input?

## Result

**Observed (static):** when the wrapper's retained owner is the constructed `cCreatureAnimal`,
its interface-query chain returns the creature's `+0xC0` component for `0x07BBC4FF`. The
component table's first slot is `C88600`, whose entire effect is `component +0xA5 = 1` and
`ret 4`. It does not read the passed embedded-wrapper pointer or call another function.

**Observed (static):** the shared component constructor already initializes this byte to `1`.
Consequently this callback is not, by itself, evidence of a fresh state transition, completed
setup, captain selection or input activation. A later reset/consumer could give the byte a
role, but that producer/consumer join remains Unknown in this slice.

## Concrete owner and interface path

The preceding audits establish two independent sides of the link:

```text
C21FED → A04A30(modelInstance, creature)
  → retain/replace creature at instance +0x180 → embeddedObject +0x64
  → same field as wrapper +0x6C, since embeddedObject = wrapper +8

74A990 tail, only when wrapper +0x6C exists:
  owner virtual +0x0C(0x07BBC4FF)
  non-null interface → interface virtual slot 0(wrapper +8)
```

This association and its timing remain conditional: owner retention can occur after some
immediate initialization; async replay and other notification callers are separate. Do not
claim the callback necessarily fires for every newly created model.

For a receiver with the constructed creature primary table `0x0146A080`, the query dispatch is:

```text
primary +0x0C = C02930
  0x07BBC4FF does not match its direct special interface cases
  C029CB → C0B7D0
C0B7D0:
  +0x58 helper ABF690: accepts 0x011C0BA3, not the requested ID
  +0x34 helper CEE3B0: accepts 0x0135B1E8, not the requested ID
  EBX = creature +0xC0
  C0B80B → C41E60(EBX, requestedID)
C41E60:
  special 0x0116DD1B check does not match
  tail-jump C88680, preserving component receiver
C88680:
  accepts 0x01186577 or 0x07BBC4FF, returns its receiver
```

The non-null `C41E60` result exits `C0B7D0` before its later fallback component queries.
This closes the receiver join without assuming that a raw ID occurrence is a decoded xref.
No interface-pointer adjustment beyond `creature +0xC0` is performed on this path.

Construction independently supports that component layout:

```text
C06960 → C1FA30
  component storage = creature +0xC0
  C1FA5A → C439C0 → C897C0
C897C0: base component table 0x014736D8
C439C0 and C1FA30 replace component tables
C06996: final component table 0x01469EC0
```

`0x01469EC0 +0 = C88600`; the base component table also has that first slot. Do not use
component slot `+0x0C = C06560` as the interface query: that slot is an adjustor/destructor
thunk. The outer query path above uses direct helpers and returns the component; the first
slot's signature is one stack argument, not the outer object's AddRef signature.

## Exact callback and constructor default

```text
C88600: mov byte ptr [ECX +0xA5], 1
C88607: ret 4
```

The argument is discarded by callee cleanup and never read. No model pointer is stored,
no service is queried, no state/input/controller is selected and no resource result is checked
inside this callback. For creature component base `+0xC0`, the write is outer byte `+0x165`;
this is not current-creature manager `+0x54`, wrapper control flags or the player-policy scalar.

At `C89918..C8992F`, `C897C0` initializes adjacent component bytes `+0xA4=0`, `+0xA5=1`,
`+0xA6=1`, `+0xA7=0`. Those defaults belong to the shared constructor and are not evidence
that setup completed. Other helper `C886A0` tests **A6/A7**, not A5; field proximity does not
make it a consumer of this callback flag.

Bounded raw field searches in the component/helper range and selected outer-creature ranges
did not establish the A5 reset/consumer. Broader byte candidates included unrelated fields and
branch displacements; they were not promoted to semantic xrefs. Alias-adjusted, packed-field,
indirect and inlined accesses are not exhaustively covered. The correct conclusion is an
unresolved flag role, not a globally unused flag or an `Absent` input path.

## Coverage, guards and next boundary

Decoded slices: `74ACAF..74ACEB`, `C02930..C02A0B` relevant interface cases,
`C0B7D0..C0B85B`, `ABF690..ABF6A2`, `CEE3B0..CEE3C2`, `C41E60..C41E78`,
`C88600..C88628`, `C88680..C886BC`, constructors `C1FA30..C1FADD`,
`C439C0..C43A3D`, `C897C0..C8996F` in focused sections and `C06960` table writes.
Selected query edges, table slots, callback bytes and constructor defaults are pinned in
`tools/verify_startup_state_route.py`.

An exploratory disassembly command with an unresolved address placeholder failed before
reading a slice; it contributes no evidence. Subsequent concrete-address slices above succeeded.

This candidate is resolved as a flag-only notification, not an unclassified activation helper.
The [outer-frame follow-up](startup-gge-scene-pump-outer-frame.md) closes scheduling into
manager `+0x38` as common AppMode service work after mode Update. Next useful work is a
direct input/current-actor consumer. Only pursue A5 further with a decoded reset/consumer lead; do not
expand arbitrary adjacent transform/resource helpers as if they were input activation.
Native delivery, resource success, rendering, input, playable readiness and object-74 delivery
remain Unknown.
