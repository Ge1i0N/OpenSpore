# GGE DBCheck branches and state 2 “Play Spore”

Reviewed **2026-10-15**. Target: GA-unpacked, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Retained [tables](audit/gge-flow/tables.json), [dialog corpus](audit/gge-flow/db-dialog-corpus.json)
and 14 Ghidra instruction/decompiler extracts in `analysis/audit/gge-flow/`. **Static original
evidence + controlled host tests**, not observation of the original game's execution.

## DBCheck is now an explicit side-effect service subset

`source/engine/gge_dbcheck.{h,cpp}` implements the recovered branch/order contract through an
explicit `DbCheckBackend` port. No methods have default “successful” stub bodies. The adapter is
**not connected to the live UI/content manager yet**: runtime GGE still reports an Unknown movie
observation as described in [gge-intro.md](gge-intro.md). This pass does not advance the real mode.

| Start observation | Original branch | Implemented subset |
|---|---|---|
| Unknown | no corresponding GA value; GA samples a real manager pointer | host blocks with no side effects; Unknown is never cast to null |
| Absent manager | `0x60B654` | emit `0x08085C4C` continuation only |
| Present, manager +0x1C true | `0x60B5E5..0x60B654` | same bypass continuation; not a DB success result |
| Present, manager +0x1C false | `0x60B5EE..0x60B652` | create/load dialog, attach proc and listener |

The named observations `Bypass`/`NeedsDialog` describe only these return-value branches; no stronger
manager semantics are inferred. Runtime “not implemented” is Unknown, not proof of original absence.

### Concrete UI identity

`0x60B340` constructs `{instance=0x4B0AC091, request_tag=0x0510A95B, group=0x40464100}`, calls
`0x8121C0(...,1,0x05B598F7)`, then looks up **root control `0x0431E538`** and attaches a proc
through root vtable +0x104. The callback object constructor `0x60B440` installs `0x13FB320`.
Its +0x14 target returns `0x82` (`0xFE7090`); it is **not the completion callback**. The relevant
callback-shaped method is vtable **+0x48 = `0x60B470`**. Invocation/pump semantics beyond this
body have not been established; it must not be called every frame to manufacture completion.

The actual SPUI is measured in PatchData rec 7381 (other copies, counts and hashes retained by the
corpus tool): **12,173 bytes / 101 objects / 24 windows**. Root object 125 carries `0x0431E538`.
This confirms the ID is a ControlID, not serialized object 0x0431E538. The root and layout are
verified against decoded C++ fixtures, not invented from a hash guess.

### Failure and callback ordering

- Failed UI load: `0x60B340` posts the same continuation; our port contract requires `load_ui(false)`
  to leave no session-owned UI. This is not “database checked successfully”.
- Loaded but root missing: unload, then continuation. The later listener lookup can independently
  miss; original start then returns with no listener and no invented notification.
- First `0x60B470` call: if dialog +0x0C is zero, set it to one and return. Nothing is finished.
- Later call: if manager exists, invoke manager **+0x20**, send continuation, find root, forward
  callback control, detach proc at root +0x108, unload layout. If manager disappeared, do nothing.
- If root disappears after notification, skip forwarding/detach but still unload.

The host session is deliberately one-request/one-shot, matching its current Intro consumer.
Duplicate terminal calls are suppressed, including state being marked before external notification
to prevent duplicate synchronous starts. No claim is made about arbitrary original concurrent
reentry. `cancel()` is an explicit **host ownership policy**: detach/unload owned UI, reset latches,
and emit no continuation. The original singleton/subscription teardown is not reproduced here.
The host marks cancellation inactive before detach/unload; callbacks triggered by teardown are
ignored. A synchronous first callback during proc attachment retains its latch. These are tested
host-safety boundaries, not a claim to have recovered the complete original callback scheduler.

## State 2 is “Play Spore”, not a blank galaxy renderer

Constructor **`0xE008B0`** allocates/initializes the 0xA8-byte handler, installs vtable `0x147EF50`,
sets global `0x16A15FC`. Name method **+0x54 = `0xE00930`** returns wide string **“Play Spore”**
at `0x147EFBC`. Earlier candidate `0xDF5EC0` is actually a boolean query on controller +0xFC, not
the name; analyst seed names are not evidence of role.

| Handler slot | Target | Observed work |
|---:|---|---|
| +0x0C enter | `0xDF9620` | clear return stack, enable GlobalUI, refresh games, effects and zoom controls, post `0x080D7451` with 1 |
| +0x10 exit | `0xDF85B0` | post same message with 0, stop/release star effects, clear selection/timers |
| +0x14 update | `0xDF8700` | update galaxy effects, selection, per-entry UI projection and visibility |
| +0x38 | `0xDF9740` | show control `0x0615A5FF` / GlobalUI and refresh selection when internal state is 2 |
| +0x68 | `0xDF6010` | project positions and update each game UI |

The entry is not implemented in our runtime. `0xDF95A0` rebuilds its game-record array from the
manager reached through `0xB3D3A0`: one existing/saved-game list at manager +0xF0/+0xF4 plus another
list returned by `0xBA6450`. Records have stride **0xBC**, position at **+0x80**, stage at **+0xA8**,
flags at **+0xAC/+0xAD**, per-entry UI pointer at **+0xB8** (roles partly inferred from consumers).

- `0xDF7CB0` fills the second/new-game list, obtaining locations through `0x5C66D0` and other manager
  queries. It is **not** a static SPUI dataset; this pass does not infer locations from artwork.
- `0xDF7F30` fills existing-game entries, names/paths/stages/thumbnail resources and checks `.spo`
  save-related metadata. Missing services cannot safely be replaced with an empty-but-ready list.
- `0xDF8300` creates marker effects for eligible entries, selecting effects including `0xCC1D3EEB`,
  `0xF0EB627C`, `0x2283C798`, and transforms their positions using the GGE scene transform.
- `0xDF9200` creates **0x90-byte `UI/GGE_game_ui_data`** per record, connects each to that record,
  loads the configured `GGEGameStarUIv4` name (`0x16A0D08`), configures stage widgets and save flags.
- `0xDF6010` calls `0xDE3C20` for world-to-screen placement. Original camera/UI transforms, effects
  manager and metadata dependencies are not available in the host yet.

REA static corroboration makes the projection contract more precise: `DF6010` is the `Play Spore`
vtable `0x147EF50` slot `+0x68` method. It reads each record's position from `record +0x80`, calls
`DE3C20`, then forwards the record pointer and projected values through the owning object's virtual
slot `+0x64`, which resolves to `DF6110`; `DE3C20` obtains a renderer/camera-related service
through virtual slot `+0x58` before computing normalized screen placement. The direct-call list is
empty because this is vtable dispatch; the concrete camera owner and runtime trigger remain Unknown.
Instruction-level REA evidence shows the returned `+0x58` result is passed as the `this` object to
`7C4250`; `805F90` supplies viewport state, and `DE3C20` applies the normalized screen transform.
REA distinguishes this projection service from the controller's stored active-camera field:
`DE46D0` obtains controller `+0x198` through `0067DCD0` slot `+0x50`, then returned-object slot
`+0x38`, while `DE3C20` uses the getter's `+0x58` path. `DF6490` also uses `+0x58` for nearest
record selection. Their identity is therefore not assumed equivalent in the host.
REA also separates the UI-bounds service used by `DF6110`: it is acquired through registry lookup
`00810650(0x02E5A4A8,0)`, whose `008102A0` implementation queries interface `0xEEEE8218` and
matches the requested service ID. `DF6110` then uses that service's `+0x34/+0x38` values together
with `805F90` viewport state. The service identity is Unknown; it is not treated as the projection
camera or controller camera.
The registry is shared more broadly: REA reports callers `00810650`, `008113D0`, and `00812590`.
`008113D0`'s no-match branch enumerates providers implementing `0xEEEE8218`, combines their
`+0x38` rectangles, and applies provider `+0x64`; the concrete provider for `0x02E5A4A8` remains
Unknown. This supports a UI/display-service registry interpretation, not a camera identity claim.
The broader REA call chain is `00812100/00811C70 → 00811C20 → 008113D0 → 008102A0`;
`00811C20` invokes the active display/layout object's `+0xD8` for each provider exposing
`0xEEEE8218`. This confirms provider synchronization, but still does not identify which provider
satisfies `0x02E5A4A8`. The owner-switch callers for `00812100` resolve to `008121C0` and
`008122A0`; `00811C70` itself has no resolved direct callers. The remaining gap is the concrete
provider/owner creation path, not the registry matching or synchronization semantics.
REA further shows `008121C0` creates/selects a `UI/LayoutObjects` instance through `00811070`,
copies its caller-supplied three-word key into owner fields, and only then calls `00812100`; the
factory publishes resource key `0x022568E7`. The key's relation to provider ID `0x02E5A4A8` is not
resolved, so the concrete display provider remains Unknown.
REA resolves the lifecycle around that registry: `00812100` replaces the display/layout owner at
`+0x88` through `008106C0` and calls `00811C20`; `00811C70` syncs the providers, invokes owner
callback `+0x8C`, and initializes resource key `0x2FBF2058`. This remains a display/layout path;
no projection-camera identity is inferred from it.
This is static shipped-artifact evidence, not an observation of a running GA process.
The call that forwards each projected record to its concrete owner is a computed `CALL EAX` at
`DF606D`; the reference-only resolver cannot resolve computed calls, but REA static vtable evidence
shows that the `Play Spore` constructor installs vtable `0x147EF50` and slot `+0x64` points to
`DF6110`. `DF6110` checks `record +0xB8` and uses the UI object's bounds to clamp the projected
target. The camera owner behind `DE3C20` slot `+0x58` and runtime trigger remain Unknown; host
selection still must not route through an invented camera/controller path.

This explains why forcing the primary layout's hidden windows visible cannot recreate game choice:
the actual star UI is instantiated **per live game record** and driven by scene projection.

## Verification and next boundary

`gge_dbcheck_verify <SPUI fixtures>` checks the real dialog corpus/root, explicit descriptor,
Unknown vs absent/bypass, load/root failures, independent listener lookup failure, first/second
callback ordering, lost manager/root, cancellation, duplicate terminal calls, and Intro integration
for synchronous bypass and later dialog completion. Side effects use a recorded controlled backend;
there is no live content-manager, message bus or original WinProc pump claim.

```powershell
cmake --build build --config Debug --target OpenSpore gge_dbcheck_verify
.\build\Debug\gge_dbcheck_verify.exe <fixture-directory>
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --flow
python tools/gge_evidence.py --static --flow
```

Next: bind a real host MoviePlayer/no-movie contract and DBCheck backend without treating missing
capability as success. Trace the game-list manager through `0xB3D3A0` and `0xBA6450`, then the
camera/marker dependencies of `0xDF8300/0xDF6010`. No playable stage is completed by this pass.

**Presentation follow-up:** [per-star calculations](gge-star-present.md) now port projection,
centering/saved clamp and stage-configuration plans, verified against the real star-layout controls.
They still require observed camera/window/margins and do not execute per-entry mounting or effects.

**Catalogue follow-up:** [source trace and typed projection](gge-catalog.md) now establish
saved-then-candidate output placement, source index namespaces, record fields and source population
`0xBB61C0`. A bounded field builder is tested on controlled metadata; no live catalogue, stars.db
parser or guessed star positions are introduced.
