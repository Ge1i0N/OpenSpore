# GGE catalogue producer boundary — manager vectors, refill and persistence

Reviewed **2026-10-17**. GA static authority SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No original runtime capture, `stars.db` corpus or user profile was used.

## Recovered producer chain

The host `build_catalog` helper intentionally accepts already-resolved typed metadata. Static GA
evidence now clarifies exactly what must produce that metadata:

- `B3D3A0` returns global `0x0167EAE4`, the game catalogue/StarManager owner.
- Existing/saved vector is `+0xF0/+0xF4`; `BA6450` exposes candidate vector `+0xDC/+0xE0`.
- `DF95A0` rebuilds the 0xBC record array, calls `DF7CB0` for candidates at
  `entry_begin + saved_count*0xBC`, then `DF7F30` for saved records at the front. It calls
  `DF9200` for per-record `UI/GGE_game_ui_data`, then `DF8300` effects. Thus candidate-first
  call order is **not** presentation order: published order is saved then candidate.
- `DF7CB0` candidate creation queries terrain/property service type `0xE38475D9`, property
  `0x2BD4F5AC` type10, writes raw position/stage5 and terrain key `(property,PNG,0x02E9A2CA)`.
  Its generation/refill route uses `BB61C0`.
- `DF7F30` saved creation uses placeholder `(0x06837044,PNG,0x02605BA3)`, source position/name/
  stage/flags and `.spo` probe via `BB27580`; it maps probe outcomes to neutral statuses. Host does
  not export pointer-bearing `+0xB8` runtime UI objects.
- `BB61C0` removes/refills candidates using count/quota, `BB24D0` eligibility, fallback selection
  and planet work, then appends to `+0xDC`. It is runtime generation, not a static package list.
- `BB8C90` initializes serializer-backed `stars.db`, planet records and refill. `BB4D40` writes
  `stars.db.tmp` and renames it to `stars.db`; OpenSpore performs no such writes.

`BB8C90`'s initialization checklist is now explicit: `BA9D50` gate first; obtain the wide
`PlanetScripts` root through service `0x006B1F20`/slot28; choose a path/config branch from manager
state (`+0x64` and path flag `+0x50`); configure metadata and manager serializers with descriptor
roots `0x01689C50` / `0x0156C668`, tag `0x01A80D26`; obtain the planet-record path and configure
the `planetRecords.pkp` service; then call `BB61C0` candidate refill. The function also initializes
time/header fields and service-owned serializer state. A successful initialization body is not a
proof that `stars.db` was present, usable, complete or post-load fixed up: the absent/branch paths
and several service results remain distinct.

Independent REA/Ghidra corroboration identifies `BB8C90` as a non-contiguous native body
`0x00BB8C90–0x00BB9939` plus `0x00BB9940–0x00BB9ABA` (Ghidra12.1.4, same GA SHA-256). Its
instructions confirm the early `BA9D50` return, two serializer setup passes, reset via `BB6510`,
`PlanetScripts`/`planetRecords.pkp` setup, descriptor roots `0x01689C50`/`0x0156C668` with tag
`0x01A80D26`, and later `BB61C0` refill. They also preserve a branch where no database/resource
result is accepted and initialization continues with defaults; that branch is not a valid Available
catalogue. The original function was not executed by REA.

The surrounding REA call graph narrows the publication boundary further. `B33A00` calls `BB8C90`,
then enumerates the saved vector span `(+0xF4 - +0xF0) / 4` and dispatches each saved entry through
the manager-owned virtual interface; this is a post-initialization saved-entry pass, not proof that
the vector was populated. `BB9AC0` conditionally calls `BB8C90`, invokes `BB61C0`, and selects a
candidate from `+0xDC/+0xE0` only after filtering entries whose `C871D0` result has bit `0x2`; this
selection path is distinct from presentation ordering. The presentation builder `DF95A0` is reached
from `DE32B0` and `DF9740`, clears/reuses its `0xBC` record array, emits candidates through
`DF7CB0`, saved entries through `DF7F30`, and only then attaches `UI/GGE_game_ui_data` through
`DF9200` and applies `DF8300` effects. REA reports unresolved indirect calls separately, so these
edges do not claim a complete runtime caller graph.

Additional REA observations place candidate selection behind the normal simulator/path setup:
`B29600` validates manager/path state, normalizes the selected `.spo`/path input, calls `BB9AC0`,
and maps manager state values to the `BB9C40`/`BB9C70` state interface; `01022470` is a thin
one-time caller of `BB9AC0`. `BB9AC0` conditionally initializes via `BB8C90`, calls `BB61C0`,
and scans the candidate span while skipping entries whose `C871D0` selection result has bit `0x2`.
This is the closest recovered static path to candidate selection, but it still does not expose a
caller-supplied row, a live manager instance, or proof that the underlying resources were available.

REA evidence retained for this pass: `BB8C90` callers
`ev_8c76766c5b7cb4a9bca9b59f5ed2c3cd09b818da7c3da7ce480bf6d3a9e8254b`, `DF95A0` callers
`ev_206838230b3646b4c7e98d29f40871a80c7cbc25ab2ca14be115a1b937dc9d7f`, `DF95A0` value-flow
`ev_3e332a8d59bb373364411dcde09dcf8a42a01ef68db860f6b0c3bdba8a3992f1`, and `BB9AC0` value-flow
`ev_223fe241d822f00099ab909965cd2cd6c8622bba5d1c6fa55f3dc7e82799afbe`. These are shipped-artifact
static observations from REA/Ghidra; unresolved indirect calls, aliasing, persistent state and all
runtime outcomes remain Unknown. The target was not launched.

The retained REA value-flow summaries are complete within their requested views: `DF95A0` reports
32 high-pcode operations, 31 def-use links and 15 effects (`ev_f32527fc7d18bb4ee63ae26ec01340c06360909f2eb66c404b235cd57d29a02a`); `BB9AC0` reports 50 operations,
47 def-use links and 15 effects (`ev_ff9bfadc94685eb96712bc0323a7f1795b76f2c2deb84a2a09faf3934e6791c3`).
These counts describe recovered static data flow, not runtime execution or semantic field types.

REA also confirms the projection seam used after records/UI data exist. `DF6010` iterates the
`0xBC` record array, reads each record's position at `+0x80`, calls `DE3C20`, then invokes the
owning object's virtual slot `+0x64` with the record pointer and the projected values
(`ev_0dd3607a175f4fb9f1f1d193d4a7a379de07ca3f5f15f99f7d3497972d0ac353`). `DE3C20` obtains a
renderer/camera-related service through a virtual slot `+0x58`, transforms the point with
`7C4250`/`805F90`, and computes normalized screen placement (`ev_bab6284c95703cb10880ebb4965a7d5d941375eb9cd7b3975445768f22847836`). The direct caller set for
`DF6010` is empty (`ev_645b43829cc52e50b5780dddd67b6ae73c8fd9a8771113e6ac75e88257129e6b`), which
is consistent with the function being reached through a state-handler/vtable boundary rather than
an ordinary resolved call. Camera ownership, the concrete `+0x64` implementation and runtime
message that triggers this path remain Unknown.

The GGE/UI boundary is separate from record construction. REA confirms that `DF9200` is called only
by `DF95A0` in the resolved direct-caller set (`ev_90969d84c07b8ad3fd8e1a86452d960db52c781831be3c9bfaf698f5a62e4a2c`):
it allocates one `0x90`-byte `UI/GGE_game_ui_data` object per `0xBC` record, stores the record pointer
at object `+0x30` and record `+0xB8`, then performs the stage/icon and save-flag setup. `DF95A0`
itself has only `DE32B0` and `DF9740` in the resolved caller set (`ev_50fb51055abb84f6c4dda34ad2f90309a63e1ef6cc0371560ec69cb6a4a7e818`).
No resolved direct edge from the `GalaxyGameEntry` registrar (`DDDE30`) to `DF95A0` was observed.

The registrar is initialized from `F473F0` (`ev_ab2c2ad005b3f43fead96c8df28456dd217091dd0a08a5cf7a6dfff7aa385558`),
which registers `GalaxyGameEntry` and other drivers. The controller activation path `DEA1B0`
(`ev_83f03f72a363a0e940fa109ec5415508a44114d757b4b7e3e06da26c50f52bbe`) calls `DE5900`, which
loads `UI/GGE/GlobalUI`, while its state/update path calls `DE9F70`, `DE95B0` and `DE6FC0`.
`DE6ED0` creates `Simulator/GGE` objects from those state transitions. This establishes parallel
GGE controller/UI setup and per-record presentation setup, but not the missing runtime message,
phase, camera or manager-instance linkage. REA's unresolved indirect calls remain Unknown.

For a caller-supplied native capture, the minimum useful observation bundle is therefore:
`BA9D50` gate result; manager `+0x64/+0x50` path flags; explicit serializer root/manager/planet
resource outcomes; root/index/context/class resource keys and decoded frame statuses; saved/candidate
vector spans after refill; terrain/name/save/header observations; and whether `BB61C0` actually
appended records. Missing any producer service is **Unknown**, not an empty Available list. The host
snapshot format can carry the final typed rows, but it does not claim to carry these upstream
service provenance facts yet.

## Why this is not attached

The workspace has no original `stars.db` or save corpus, and requirements prohibit scanning user
profiles. Package terrain/property resources are not StarManager vectors or generated positions.
Native manager/service pointers, candidate grid generation, planet producer, save discovery and
post-load fixups remain unavailable. A bounded negative workspace search is not an empty catalogue.

`CatalogInput` therefore remains strict: both availability values must be explicit `Available`.
Unknown does not become empty, and controlled verifier entries do not become production data.
`GGEHostPreview` stays separately labeled controlled and uses three typed entries only for manual UI.

## Required next adapter

An honest producer needs a caller-supplied decoded `stars.db`/manager capture, authorized passive
native manager/service evidence, or a separately justified complete generation/save-discovery
reimplementation. It must preserve saved/candidate order and indices, distinguish missing terrain/
name/save/probe states, publish Unknown when services are unavailable and never write `stars.db` as
a preview/scene side effect.

Evidence remains in `analysis/audit/gge-catalog/` (`DF95A0`, `DF7CB0`, `DF7F30`, `BB61C0`, `BB8C90`,
`BB4D40`, manager getters). Existing `gge_catalog_verify` has 19 grouped controlled checks; this
report adds no live catalogue/native producer claim.

Next: caller-supplied catalogue capture/inspection or explicitly authorized runtime evidence. Until
then the clickable preview validates presentation, not playable GGE data recovery.
