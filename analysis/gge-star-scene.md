# GGE scene boundary — owned host projection/layers/picking, not a native galaxy

Reviewed **2026-10-15**. GA-unpacked authority SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static local Ghidra/objdump and shipped UI/PNG observations; no original-runtime camera execution.

## Outcome and remaining boundary

`engine/gge_star_scene.*` now joins explicit Available catalogue metadata, owned
[card lifecycles](gge-star-lifecycle.md), an explicitly supplied active-camera observation, existing
[projection/placement](gge-star-present.md), per-card outcomes and shared layer/pick views.
`states::draw_gge_star_scene` consumes those views through the existing pixel-layer painter.

This is a **host 2D card collection**, not native Simulator/render-stage/effects execution. No code
automatically connects it to the normal GGE screen, discovers saves, generates candidates, creates
a native camera, forces MoviePlayer/DBCheck completion or marks a native scene ready. Explicitly
authorized [real-save corpora](gge-real-save.md) are now available, but these scene verifiers still
use controlled metadata/cameras, not a complete real-save runtime producer.

## Native camera/factory findings

**2026-10-17 identity correction:** [viewer evidence](gge-viewer.md) resolves the generic names in
the original extracts. `67DCD0` returns GameModeManager, `+58` returns cViewer (matrix+C0), and
`+50 -> +38` returns active ICamera through CameraManager. Explicit viewer-matrix composition is
implemented; live GGE camera selection/parameters, actual updates and viewer production remain unknown.

| Question | Static evidence | Status |
|---|---|---|
| GameModeManager source | `0x67DCD0` returns global `0x015FD8B0`; setter `0x67DEC0` | Table01412608 corroborates manager+2C CameraManager / +30 cViewer |
| Viewer projection | `0xDE3C20` calls manager **`+0x58` GetViewer**, passes cViewer to `0x7C4250` | Matrix+C0 belongs to viewer; no native acquisition in host |
| Controller `+0x198` | `0xDE46D0` calls manager **`+0x50 GetCameraManager → +0x38 GetActiveCamera`**, retains/releases ICamera | Different from viewer; Update takes `(delta,viewer)`, concrete GGE camera unknown |
| Scene setup | `DE46D0 → DDD920`: Simulator allocation `0x74`, `DDA8C0`, global `0x16A0B08`, then `DDD150` | Chain retained, not executed; allocation/load result is not host readiness |
| Factory services | `67DD40/67DD50` return globals `015FD8CC/015FD8D0`, setters `67DF50/67DF60` | Request slots/tags observed; concrete factory types/full semantics unresolved |
| Stage registration | `DDD150` uses `67DD10` (global `015FD8C0`) and slot `+0x4C` with stages `0x14/0x1A/0x0D` | Contract boundary, not native render registration execution |
| Effects world | `67DD90` returns `015FD8E8`; `DE46D0` slot `+0x4C` requests key `0x02DDA976`, "GGE effects world", retained at `+0x15C` | No effects-world backend or fake handle |

`DDD150` requests tag `0x07B6877F` via factory67DD50/slot18 into owner+20; tags
`0x07B6877E/0x07D23D15/0x07D23D13` via factory67DD40/slot14 into owner+18/+1C/+14.
Those identities/slots do not by themselves name their concrete classes or prove native mount success.
No claim is made that pointer-looking BSS values were read as live runtime services.

`DE3C20` maps NDC using **window extents only**, not its origin. The host frame/painter/picker use
viewport `(0,0,width,height)` accordingly. Matrix input is memory-order `+0xC0..+0xFC` as already
recovered; no extra transpose, camera default or unproven controller alias is inserted.

## Host input and ownership contract

`StarUiScene::open` accepts a `CatalogInput` and an ordinal-aligned `StarSceneCardInput` vector.
Both saved and candidate list availability must explicitly be **Available**. It calls `build_catalog`,
retains saved-before-candidate order and duplicate star IDs/source indices, and opens one owned
lifecycle per entry. Each card needs its own caller-issued nonzero unique host ID; neither star ID
nor source index is a unique UI identity. Per-card phase/config choices and saved insets stay explicit.

Construction validates all cards before replacing the previous collection. Invalid metadata,
choices/counts/IDs/budgets preserve prior ownership. **Unavailable catalogue observations are a
deliberate exception:** old cards remain diagnostic data, `catalog_status()` becomes Unknown and
the prior frame is retired; asset passes, update, layers and picking remain suspended until a
successful Available reopen. Thus a lost producer never becomes an observed empty galaxy or a
silently usable stale catalogue. Available-empty opens an actual empty collection without stars.

Host allocation policies: **128 cards**, aggregate **65,536 retained objects / 32,768 windows**.
These are conservative presentation bounds, not original galaxy population limits or a total
CPU/GPU asset-memory budget. Decoded image limits/ownership remain those of the
[streaming provider](gge-streaming-images.md); there is no new cross-card global asset cache.

The scene owns its lifecycles and exposes const borrowed card/layout views. Generation tickets
change after each successful open, even if caller IDs repeat; stale scene tickets cannot apply
assets or update a reopened collection. Close retires ownership and all frame views. During a
synchronous asset pass, reentrant open/close/apply/update and layer borrowing are rejected. Card
placement through `StarUiLifecycle::place` has its own active-ticket/busy guard. No native listeners,
asynchronous cancellation contract or thread-safe renderer ownership is inferred from these guards.

## Explicit camera/frame contract

`SceneCameraObservation` defaults to Unknown. Update requires:

1. Source **ActiveProjection**, not ControllerRetained/Unknown.
2. A nonzero **caller-issued observation token**, not a native pointer/service-ready flag.
3. Explicit finite matrix and valid finite window extents.

The tag/token records caller evidence; it does not independently acquire or authenticate the native
camera. In the verifier, ActiveProjection is a deliberately supplied **controlled camera**.
The live native producer and interactive camera controller are still absent.

Every accepted update attempt retires the previous frame and issues a fresh serial. Missing/invalid
camera observations fail globally and leave no old drawable/pickable frame. Valid camera data drives
each entry through `project_star → place_star_ui → owned lifecycle place`. Per-card zero-w/overflow
or missing/invalid saved clamp observations are explicit failures and exclude that card from the
current layers/hits. Other independent cards may still position; this is not a global rollback or
native dependency/culling policy. Old geometry may remain privately owned for retry but cannot be
picked/drawn as the current failed card.

Existing GA subset behavior is retained: saved clamp only, strict **either-axis delta >4px** before
moving, and no new negative-w/NDC clipping/depth sorting. `StarSceneFrameStatus::Projected` means
only that the host computed this observation snapshot, even if some/all cards fail or the catalogue
is empty. It is not image availability, phase completion or scene readiness.

Asset/flag/caption passes retire the frame too, because visibility and geometry may change. Callers
must publish a fresh update before rendering or picking again. This avoids combining a stale hit
snapshot with new action state.

## Draw and picking seam

`layers(frame_serial)` returns only current successfully positioned entries in catalogue order.
Each layout still honors its own inherited visibility/enable/Clip/IgnoreMouse rules. A returned
positioned layout does not assert that any of its hidden controls will actually draw or accept input.
`draw_gge_star_scene(scene, serial)` uses these views with `draw_spui_pixel_layer`; it does not
begin/end/clear a frame, rescale design resolution, load a native scene or attach a GGE caller.

`pick(serial,x,y)` reverses the same layer order and calls the existing per-instance pixel picker.
Its result is a **value** containing scene ticket/frame, host ID, catalogue ordinal, serialized
window object index and observed ControlID/CommandID. It never dispatches that command or changes
the original selected-StarRecord state. Forward catalogue paint/reverse pick are host policy, not
proof of native dynamic Z, camera depth sorting or tooltip lifecycle.

## Verification and actual-asset boundary

`gge_star_scene_verify`: **208 checks PASS**, both real 152-object/58-window layouts and actual
original-package atlas pixels. Metadata, camera matrices and saved insets are controlled inputs,
not a recovered live catalogue/save/native camera.

- Unknown/Available-empty catalogue distinction; unavailable refresh suspends an old frame without
  deleting diagnostic cards or fabricating zero stars.
- Saved/candidate order and reset source indices, duplicate star IDs with distinct host IDs.
- Active-vs-controller camera identity, missing/invalid matrix/window, zero-w per-card failure,
  negative-w preservation and nonzero viewport-origin/extents behavior.
- Shared projection→clamp/strict4px placement→owned graph; fresh frame serials retire stale views.
- Saved missing/inverted clamps fail explicitly; candidates are not arbitrarily screen-clamped.
- Catalogue-order layer/reverse-pick identity, boundary/NaN/disabled input, close/reopen/ticket
  isolation, transactional failed construction and aggregate bounds, provider reentry.
- Direct original StreamingImageProvider atlas data retained in a scene-owned lifecycle; partial
  image/native service availability does not become native-ready.

**Important visibility finding:** the untouched shipped template after the current phase-1 subset
is mostly hidden and its root has IgnoreMouse. The verifier confirms it is **not** made hittable
by scene publication. For overlap/pick assertions only, a clearly controlled copy clears **only
root object177's IgnoreMouse bit**; it does not reveal hidden panels or infer native phase2/show
behavior. The raw serialized Objects and original source remain provenance. The test is not an
original-runtime screenshot, a newly observed native visible card or a playable galaxy.

This verifier asserts host layout/resolver/pick state, not a new GPU scene readback. The renderer seam
is compiled; existing image/frame/scissor GPU suites cover their controlled subsets independently.
Normal Main/GGE still has no automatic caller, active-camera producer or genuine catalogue source.
The [explicit transition follow-up](gge-star-transition.md) now requests native case2..5 visual
subsets on untouched layouts. Its case2 panel hit no longer needs this verifier's controlled
IgnoreMouse fixture; real caller/current-phase observations and automatic dispatch are still absent.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_scene_verify
.\build\Debug\gge_star_scene_verify.exe <SPUI-fixtures> .\build\bin\Debug\Data\Spore_Graphics.package
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --scene
python tools/gge_evidence.py --static --scene
```

Evidence: [facts/boundaries](audit/gge-scene/facts.json), fourteen functions (C/instructions),
[service accessors](audit/gge-scene/service-getters.asm.txt), existing
[mount](gge-star-instance.md) / [projection](gge-star-present.md) / [lifecycle](gge-star-lifecycle.md).
REA MCP tools were absent; existing local Ghidra/objdump state was reused without configuration
changes. These are not REA Evidence IDs or original runtime captures.

Next: concrete active-camera producer/controller and native show/phase lifecycle boundaries, then
explicit real catalogue/source integration and GPU card/selection validation. Native Simulator,
render stages, effects, MoviePlayer/DBCheck completion and gameplay remain separate unresolved work.

Final Debug x86 warning-free. Scene204 and all prior lifecycle232/streaming95/AnimatedIcon159/image80,
phase1/instance/projection/parser/catalogue/DBCheck/Intro/SPUI/package suites PASS; existing controlled
image/frame/scissor GPU readbacks PASS. Six Python tests, 57 maintained Markdown link checks and
generated dashboard --check PASS. Launcher/Main → GGE/visible launcher → CreatureEditor smoke runs
exit0, aborts/asserts0; MoviePlayer observation still Unknown. The new launcher capture is RGB
pixel-identical to `analysis/shots/spui_e3launch_uv.png` (diff bbox=None). These runs preserve the old
full-screen behavior and do not automatically call the new scene renderer or validate a native galaxy.
