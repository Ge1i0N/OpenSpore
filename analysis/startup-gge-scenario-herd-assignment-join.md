# Scenario marker-record to current-creature assignment: conditional join

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only short `objdump` slices and PE-byte inspection; no Ghidra or native execution.
Continuation of [assignment caller flags](startup-gge-current-creature-assignment-callers.md).

## Result: a static conditional join, not an observed spawn

```text
F45E70 → F45AD0 → F432C0 (only if its transform/control gates pass)
  → allocate marker RECORD, owner +0x8C, identity/value -2
  → F3F8C0(candidate == owner +0x8C)
  → EEDD40(..., fifth argument = 1)
  → normalized tuple identity 0x2B978C46, resource lookup non-null
  → B23920(..., fourth argument = 1)
  → cHerd +0x84 = 1
  → cHerd virtual +0x54 = C6E690
  → if current creature is null: C09B40(..., fifth argument = 1, sixth = 0)
  → if creature acquisition succeeds: B1FCA0, manager +0x54 = creature
  → C03920 setup, whose success and rollback remain separate
```

This connects a Scenario-side producer to one of the previously identified positive assignment
callers. The actual selected resource/key, successful runtime factories, creature setup, world
attachment, captain identity and input activation remain Unknown. The record's `-2` sentinel and
`mOwnedByAvatar` metadata do not alone identify a player-controlled captain.

## `C6E690` is a `cHerd` virtual, not an unbound address family

The `.text` relative-edge scan found no direct call/jump to `C6E690`; a raw pointer scan found
`0x01471BE4`. Constructor `C6C2B0` installs primary vtable `0x01471B90` and secondary table
`0x01471B80` at object `+4`. The primary table has `+0x54 = C6E690` and `+0x0C = C69F50`.

Registered class `0x01BE418E` uses allocator `B1EE70`, size `0x220`, allocation label
`Simulator/cHerd`, then tail-jumps to `C6C2B0`. Registration anchors are `B251FD..B25219`;
the adjacent label/interface registration carries `cHerd` and `0x52AA6117`.
`C69F50` accepts `0x52AA6117` and delegates other requests to `B183A0`, which accepts requested
interface `0x017F243B`. This is compatible with `B20BF0`'s service request. As in the previous
[factory audit](startup-gge-scenarioplay-player-factory-dispatch.md), registration and allocation
are conditional, not proof of runtime selection or success.

The constructor instruction at `C6C308` initializes herd byte `+0x84` to zero. Static metadata initialization
`134D450..134D4B2` associates offset `0x84` with ASCII `mOwnedByAvatar`, property ID
`0x021E6DAE`, and Boolean serialization helpers `AC8690/AC86C0`. The raw data destination is
zero in the file; these are initializer instructions, not a claim that metadata is populated at
runtime. The name is evidence for an ownership role, not proof of captain identity/control.

## Concrete role writer and indirect call: `B23920`

Six caller arguments are consumed by this `thiscall` routine:

```text
B23920(position, resourceContext, count, ownedRole, extraContext, makeNest)
```

These labels describe observed use; only the `mOwnedByAvatar` role has a pinned binary name.
`B23933..B23938` requests class `0x01BE418E` through `B20BF0`. It writes count at `+0xF4`,
resource context at `+0xA4`, position at `+0x34`, extra context at `+0x15C`, and the low byte
of the **fourth** argument at `+0x84` (`B23970..B23977`). When the sixth argument is nonzero,
it also obtains an object through `B20DD0` and retains it at `+0x160`.

`B239B6..B239BD` then invokes the returned object's virtual `+0x54`, unconditionally with
respect to the role and sixth-argument branch. If the selected factory returned the constructed
`cHerd`, this dispatch is `C6E690`. That routine checks role `+0x84 != 0` and current-reference
`B1FD00 == null` before `C6E861 → C09B40(position, resourceContext, 1, herd, 1, 0)`.

`B23920` dereferences the factory result before any null guard in the examined prefix; it must
not be modeled as a checked successful factory transaction. Its later Boolean argument also
controls manager `+0x58`, which is **not** the current-creature reference `+0x54`.

## Scenario producer: `F432C0 → F3F8C0 → EEDD40`

In the no-existing-`+0x8C` branch, `F432C0` allocates `0x234` bytes labelled
`Simulator/cScenarioMarker`, initializes through `F2A960`, stores the pointer at owner `+0x8C`,
and writes its first word to `0xFFFFFFFE`. `F2A960` initializes vectors, a local range and status
fields; this is a Scenario **record**, distinct from the native object returned by the dispatcher.

The routine copies the source transform to the record. It validates the three-word resource key
at owner data `+0x130` through a service call; on failed validation it uses `F42EB0` to replace the
key. That helper builds query criteria and either copies a result tuple or returns an all-zero
tuple; neither selected resource identity nor runtime availability was observed here.

At `F43412..F43426`, it calls `F3F8C0` with candidate = owner `+0x8C`, resource key at data
`+0x130`, and the other stack argument zero. Crucially, `F3F8C0` does **not** skip construction
when candidate equals owner `+0x8C`. Instead it records equality with `sete` and skips only the
special `F237B0` branch. In its fallback it passes that equality Boolean as **EEDD40 argument
five** (`F3F935..F3F950`). This corrects the earlier no-replacement interpretation.

Within `EEDD40`, normalized tuple identity `0x2B978C46` selects `EEE0B9`. A non-null resource
context from `401090/4DF5C0` is required. `EEE0D4..EEE0EC` calls:

```text
B23920(position, resolvedResourceContext, 1, EEDD40.arg5, 11, 0)
```

Thus the marker-record equality Boolean becomes the herd ownership byte and can reach current
creature assignment **inside `B23920`**, before it returns. Afterwards `EEE124..EEE137` reads
the manager's current creature. When argument five is nonzero it skips the separate `EEE142`
creature creation call and uses that existing reference; when zero it calls `C09B40` with
assignment **zero**. The earlier direct-call audit was correct about `EEE142` but did not cover
the earlier indirect herd initialization producer.

`F45E70` calls `F45AD0` first with final controls `(1,1)` and then `(0,0)`. The first pass can
reach `F432C0` when its final control and transform-difference tests permit it. Existing-marker
updates follow another branch; this report does not claim every synchronization pass spawns.
No failure check proves that the nested assignment and setup completed successfully.

## Separate correction: pre-getter call in `F1F060` is string work

`67DE00` returns global `0x015FD8F8`. Its constructed manager table `0x01403D5C` has
`+0x20 = 695CB0`, which delegates to manager `+0x4C`, virtual `+0x10`. Initialization
`696569..696581` installs the `6B6CC0` object at that field, labelled `App/cStringManager`.
Its table `0x01409C68 +0x10 = 641760` returns field `+0x28`, constructed by `6B6420`.

The resulting table `0x01400FB0 +0x34 = 6B58E0` is exactly the virtual slot used at `F1F132`.
`6B58E0` copies a UTF-16 string into one of four local string slots; `F1F060` supplies index
zero and the value from `F261E0`. This closes that candidate as string setup under the examined
construction chain, not creature/resource creation. The following `B1FD00` remains an existing
current-creature read, not an assignment.

## Coverage and remaining work

Evidence: decoded focused slices at `B1EE70..B1EE97`, `B251D9..B25224`,
`C6C2B0..C6C353`, `C69F50..C69F73`, `B183A0..B183C6`, `B23920..B23A15`,
`C6E820..C6E86F`, `134D450..134D4B3`, `F432C0..F435A0`, `F3F8C0..F3F961`,
`EEDF87..EEDFA8`, `EEE0B9..EEE195`, and the string-manager construction/dispatch slices.
Selected byte, slot, label and call anchors are pinned in `tools/verify_startup_state_route.py`.
Raw scans were candidate discovery only; indirect/inlined/alias writes are not exhaustively covered.

The [avatar-asset/profile follow-up](startup-gge-avatar-asset-profile-setup.md) identifies data
`+0x130` as `mAvatarAsset`, bounds fallback/normalization producers, establishes that `C03920`
returns profile-pointer presence rather than model success, and follows conditional cleanup to
the current-reference clear. Actual selected captain key and concrete model-provider success
remain unresolved. Do not
equate the ownership byte, reference assignment, object flags, or source record with a completed
captain spawn. Native delivery, resources, world/stage activation, input, gameplay ticks and
object-74 `Play Scenario 0x070D7509` join remain Unknown.
