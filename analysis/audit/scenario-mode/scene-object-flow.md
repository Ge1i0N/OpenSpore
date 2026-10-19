# ScenarioMode scene-object flow after the pre-play gate

Static analysis of the selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native runtime was launched and no save/resource writer was executed.

This document narrows the boundary after the loading-context phases in
[preplay.md](preplay.md). The recovered calls are object-record synchronization and readiness
bookkeeping. They are not, by themselves, evidence of terrain creation, mounted assets, or a
playable stage.

## `EF25E0`: play-side object-list preparation

`EF25E0` first checks a service-provided byte from `0x015FD918` and only enters its play-side
preparation when ScenarioMode `+0xCC == 2` and that byte is nonzero. In that branch it:

1. releases/compacts the controller's temporary pointer vector at `+0x74/+0x78`;
2. calls `EF18B0` with the Scenario-record range (`+0x2C10/+0x2C14`, count/index at `+0x2C24`)
   and the current mode-side object;
3. walks records with stride `0x238` and calls `EEC950(record + 4)`;
4. for every returned object, calls virtual `+0xAC`, clears bit 0 of the returned object state,
   increments its reference/count field at `+0x40`, and appends it to the temporary vector;
5. calls `F45E70(2, 0, 0)`.

The vector and object references are concrete native bookkeeping. The names/semantics of the
record class, virtual `+0xAC`, and returned object's fields are not promoted beyond what the
instructions show.

After that branch, `EF25E0` performs general mode work (`B3D450`, `B87F10`, `F3C160`). If the
mode-side byte at `ScenarioMode + 0x14` is set, it calls `F40210`. If the mode is play (`+0xCC ==
2`) and `F40210` returns false, it calls `F45E70` once more with the same record-selection
arguments. The edit branch instead reaches `F42660`. This is a retry/synchronization boundary,
not a proof that the retry succeeded.

## `F45E70`: two record synchronization passes

The wrapper is short and explicit:

```text
if param_3 == 0:
    F411A0()
F45AD0(param_1, param_2, 1, 1)
B228F0()
F45AD0(param_1, param_2, 0, 0)
```

For the play preparation above, the caller supplies `F45E70(2, 0, 0)`. The wrapper therefore
does not directly call terrain, stage, or planet construction. `B228F0` is retained as an
unclassified intervening service call.

## `F45AD0`: record/object synchronization and conditional object creation

`F45AD0` walks the Scenario record vector using the native `0x238` stride. For each record it
obtains a data pointer from `ED36D0(record + 4)`, then reads a byte from a selected `0x4E0`-byte
record payload. Its `param_1` branches are:

- **1:** obtain an object through `EEBFF0` when the creation flag is false, otherwise call
  `F3F8C0(data, param_2)`; store the selected record index in record `+0x22C`; retain/replace the
  object in the controller map; apply object flags and virtual `+0xAC` state. A zero payload byte
  can set object `+0x78 = 1`.
- **2:** only process records whose selected byte/flags permit it; obtain or create the object in
  the same way; synchronize flags through `F3D930` and `F3C4B0`.

The exact flag checks are native bit tests: record flags at `record + 0x228` use bits 4, 1 and 2
in the mode-2 path, while the selected object flags use bit `0x2000` in the service object at
`+0x50`. The decompiler's names are not reliable, so this audit keeps them as offsets and bits.

When `param_3 != 0` and the existing object is absent, `F3F8C0` can call `EEDD40`. That is the
strongest recovered object-construction edge in this slice. `EEDD40` is an identity dispatcher
with a shared object/service finalizer, not a proven terrain/world factory; see
[startup-gge-scenario-object-factory-dispatch.md](../../startup-gge-scenario-object-factory-dispatch.md).
`F3F8C0` then writes the resulting object back-links:

```text
object +0x18 = Scenario record pointer
object +0x1C = record identity/value
```

These writes prove association of a native object with a Scenario record. They do not identify
the object as terrain, a planet, an avatar, or a playable stage.

At the end, when the supplied transform differs from the graphics/service defaults,
`F45AD0` calls `F432C0`, which creates/updates a `Simulator/cScenarioMarker` when needed. That is
explicitly a marker path, not terrain construction.

## `F40210`: readiness aggregation, not a loader

`F40210` begins by clearing bit `0x5` in an optional selected service object, then:

1. resolves the selected Scenario record from `+0x2C24` and the `+0x2C10/+0x2C14` range;
2. clears bit `0x5` on every record in the range while walking valid records;
3. calls `F400E0(selectedRecord, &rangeEnd)` when a selected object exists;
4. calls `F400E0(eachRecord, &rangeEnd)` for every valid record;
5. returns the accumulated Boolean result, then calls `F3D7A0` for non-play mode.

`F400E0` calls `F3D620`, obtains the associated service object, runs `F3EFB0`, and sets/clears
record bit `0x10`. It then calls `F3EFB0` on other records when required. A failed result sets
record bit `0x4` and also sets that bit on the related record. `F3EFB0` classifies a record's
resource/identity values and may write a local wait/recovery code to `record +0x230` (observed
values include `0, 1, 8, 9, 10, 0x1E`, among others); the semantic names and service completion
results remain Unknown.

Therefore the `EF25E0 → F40210` path establishes a concrete readiness bookkeeping contract:

```text
record/resource checks → record bits and +0x230 status → Boolean aggregate
```

It does **not** establish that a resource mounted, a terrain was built, or an adventure became
playable. A host implementation must preserve Unknown/Failed rather than treating the Boolean as
a universal world-ready signal.

## Finalizer/lifecycle boundary

The common `EEDD40` tail calls `EED640` for status synthesis when its caller parameter is nonzero,
then reaches `B3D400/B228F0`. `EED640` writes derived predicates/classification and status bits
into a five-word record. `B228F0` walks the owner object list, checks component `0x01186577` and
virtual lifecycle predicates, retains passing objects, and unlinks/releases failing entries while
publishing internal event `0x01A0219E`. Its cleanup helpers release lists and compact vectors.

These are lifecycle/status operations, not proof of resource mounting, terrain creation, stage
activation or playable gameplay. See
[startup-gge-scenario-object-finalizer-lifecycle.md](../../startup-gge-scenario-object-finalizer-lifecycle.md).

`F3D930` provides the adjacent state-sync path: record vectors are copied into ScenarioMode
backing fields, component `0x01186577` setters are invoked, `EED640` status is recomputed, and
owner callback `+0x84` is notified. `B22C50` applies optional component/tag updates and sends a
failed virtual validity predicate to `B22560/B20CC0` release handling. The `B48870` configuration
key `useGameplayPhantom` is gameplay-adjacent policy only, not gameplay readiness. See
[startup-gge-scenario-object-state-sync.md](../../startup-gge-scenario-object-state-sync.md).

The invalidation event `0x01A0219E` reference set includes sends, listener subscription and
removal: `AC1430` subscribes; `B452F0/FDE170` remove subscriptions rather than publishing.
The player consumer gates collectables reconfiguration on payload code `5`, distinct from the
examined cleanup codes `3`/`4`. `B48870`/`B56160` provide a separate
`useGameplayPhantom` and collision/proxy branch with optional `Simulator` allocation. These paths
remain lifecycle/phantom handling, not proof of terrain or playable-stage construction. See
[startup-gge-scenario-event-phantom-boundary.md](../../startup-gge-scenario-event-phantom-boundary.md).

The follow-on `F47240 → AEFB00 → B55750` slice establishes generic allocation, native
proxy/object initialization, bounds preparation and update/attachment cases. It still has no
direct terrain/stage/world factory join. See
[startup-gge-scenario-simulator-proxy-ownership.md](../../startup-gge-scenario-simulator-proxy-ownership.md).

The subsequent `B540D0` query-result loop (interior address `B54180`) reaches
`B53AB0/B538C0 → B55750` through keyed-value and component gates. It establishes table lookup and
existing-object coordinate/vector synchronization, not registration or removal. No direct
world/stage consumer join is established in this slice. See
[startup-gge-scenario-simulator-proxy-registry.md](../../startup-gge-scenario-simulator-proxy-registry.md).

## Remaining world boundary

The next unresolved edges are the successful results and consumers of the `EEDD40` identity
branches and the component virtual methods used by `B228F0`,
`F3D620/F3EFB0`, and the object virtual methods reached through `+0xAC`. The current evidence
does not show a native terrain loader or stage activation in this slice. The separate
`ScenarioPlanet` call remains a lighting-configuration selection as documented in
[preplay.md](preplay.md).

Evidence is retained in [scene-object-evidence.json](scene-object-evidence.json). The static
guard launcher is read-only and does not execute these native calls.

Key retained REA records in that bundle:

- `ev_0bffd816c44156883e19a789b493bdebc96b78dfd8284aa82050de5d91e2a7bb` — `EF25E0` control flow.
- `ev_a3c4487d38167b6f84682d5e1b701922e910b3547f690ee845e44dc5028d31b2` — `F45AD0` record/object pass.
- `ev_b9bddf028422281efd0a971d4ef5cfaf4d7dc2d4f7cc8ddeac16e63d47a065d3` — `F45E70` wrapper.
- `ev_9a77634fae18fc8ae2565c777044f238bcf56992e98a2b61013bcee7f0b019a4` — `F400E0` readiness aggregation.
- `ev_2647893bcb6d0205844608bb394cbbc3da955c56f673380975286bf90a4d0d35` — `F3EFB0` resource/status classification.
- `ev_68188b3dafbeac40777b643df06e96649fea5edaac302a7d24bd0721555ea5af` — `F3D7A0` flag synchronization.
- `ev_9b40ce99cfdc2afadc5fccd943e7cb0182abdc153910de8704afc998bf29506b` — `F3D620` record/object lookup.
- `ev_58b9c3fd574fd0606c38eb63459152334303e2c93e0db4ec8e65ee47f980f8ec` — `F432C0` marker path.
