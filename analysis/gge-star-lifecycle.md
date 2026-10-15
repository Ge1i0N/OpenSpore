# GGE star host lifecycle — independent action application, not native mount

Reviewed **2026-10-15**. GA authority SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
This step joins existing host subsets; its retry/independent-action/generation policy is **host
design**, not a newly recovered original state machine. No original runtime execution was observed.

## Implemented outcome

`engine/gge_star_lifecycle.*` owns one `StarUiInstance`, its immutable input snapshot and a persistent
per-action report. `open` instantiates and plans transactionally; `apply_available` applies available
host operations; `close` releases the owned graph/images/report. The instance is exposed read-only
for the existing renderer/picker, not as a native mounted window or pointer-shaped wire identity.

Supported operations are now connected in one host owner:

- Phase-1 visible/enabled/etc flags and local root-extents centering.
- Explicit PNG ImageResource/StageImageTarget through the [existing image executor](gge-image-bindings.md).
- Stage-description caption **reference** binding (not a locale service lookup).
- Entry name, bounded UTF-16 → UTF-8 inline caption.
- Separate `DF9200` configuration flags/caption reference and [Animated Icon](gge-animated-icon.md)
  frame selection plus exact serialized atlas-pixel attachment.
- The [explicit phase-case follow-up](gge-star-transition.md) applies the recovered **visible mask1**,
  name and local rectangle-copy subset of `DF6D80` cases **2..5**. It requires separate current-phase
  observation/choices; opaque services and native phase latch are not executed or synthesized.

Tooltip/userdata, native layering, saved terrain, profile and timestamp/colour services remain
Unsupported (or structurally Missing/Ambiguous). No pointer, name-service answer or date string is
fabricated. A stage-image getter may be Observed as a descriptor without decoded pixels; that is
not a Found resource or drawable readiness.

## Ownership and lifecycle contract

```cpp
os::gge::StarUiLifecycle card;
os::gge::StarLifecycleInputs inputs; // branch observations default Unknown
// Caller supplies real entry/source, explicit observations and any duplicate object choices.
if (card.open(host_id, source, entry, inputs, error)) {
    const auto ticket = card.report().ticket;
    card.apply_available(ticket, explicit_provider_or_null, error);
    // Inspect card.report().actions and both blocked_on fields. A true return is NOT ready.
    // Existing draw_spui_pixel_layer / picking can consume card.instance()->layout().
}
card.close(error);
```

- Input observations and object choices are frozen per successful open. Changed observations
  require a new open; this resets presentation state and issues a new generation ticket, even if
  the caller reuses the same host instance ID. Failed open preserves prior state/ticket.
- Old tickets and closed owners reject passes before provider I/O/mutation. The generation is
  owner-local host identity, not an original StarRecord token, star ID or native phase counter.
- The owner is synchronous/single-threaded. During a provider callback, reentrant open/close/apply
  all fail before mutation. There is no borrowed-window pointer across a provider call and no owner
  replacement while execution is active. This is not an asynchronous cancellation API.
- A null provider reports Unknown; backend Missing/Ambiguous/Unsupported/ReadFailure are distinct.
  Provider exceptions become failed asset observations, not native completion or process crashes.
- A pass memoizes **one result per exact ResourceKey**, including failures, for that pass only.
  The next retry pass can observe a changed backend. No disk/profile search or resource precedence
  is introduced. Decoded images use the existing immutable ownership/size rules; this owner is not
  a globally budgeted asset cache.

The returned instance/report views are borrowed; successful reopen/close invalidates them. The
owner exposes no mutable instance API that could silently invalidate its frozen plan.

## Deliberate independent-action policy

Each pass walks the phase-1 plan in recorded order, **then** separate configuration actions.
It tries each independent presentation action even after another reports Unsupported/Missing.
For example a missing tooltip service does not prevent testing an available entry name or caption.
This is **not** the native ordering between `DF6D80`, `DF9200`, mount/listener operations or optional
control skips. It does not establish that later original actions would execute or be safe after an
earlier failure. No dependency is marked satisfied on the strength of this policy.

Statuses are Pending, Applied, ReferenceBound, Observed, Unchanged, IgnoredOutOfRange, Unknown,
Missing, Ambiguous, Unsupported and Failed. Applied operations, bound references, descriptor
observations and known no-ops are not replayed; unresolved outcomes are retried. Attempts/pass counts
are retained and saturate rather than wrap. Resource readiness is not inferred from a caption ref,
an assigned frame or a successful pass. Original plan resource statuses remain Unknown.

There is deliberately **no phase-complete boolean**. `native_services_executed()` and
`native_mount_complete()` remain false. The normal GGE screen has no automatic caller for this
owner, and MoviePlayer/DBCheck/Intro gates are untouched.

## Identity, centering and text details

Phase-1 choices and configuration choices are validated **separately**. The same duplicated ControlID
`0x026E0002` may explicitly choose regular window55 for its image request and Animated Icon45 for
its frame request; the host neither guesses recursive lookup order nor drops the other candidate.
Choices for the wrong control/object, duplicates/conflicts and out-of-scope controls are rejected.

Centering follows retained `0x806E00` floor/SetLocation behavior against the phase-1 zero-origin
root rectangle. The multiplier at `0x145BE38` is float **0.5**. Host `center_in_root` preserves target
extents, computes floored **local** coordinates and refreshes the inherited graph atomically.
It does not compensate for a nested parent as if the native setter took absolute coordinates.
Finite/positive host geometry guards are not native optional-control behavior; x87 bit-exact
arithmetic for pathological/extreme float rectangles is not claimed.

`set_caption_text` validates before modifying any caption/ref fields. Empty is an observed empty
name and clears an old reference; malformed UTF-16, embedded NUL or >4096 code units fail without
changing the previous text. Valid surrogate pairs are retained exactly in UTF-8. The existing overlay
font is still ASCII-limited; storing Unicode correctly does not claim multilingual font rendering.

Caption reference bindings preserve table/key metadata for the shared painter. `ReferenceBound`
does **not** say the locale table/key exists, loaded, or formatted by native string-builder services.

## Verification

`gge_star_lifecycle_verify`: **232 checks PASS** on both actual 152-object/58-window star layouts.
Controlled metadata/provider behavior is explicitly separate from original save/catalogue evidence.

- Unknown branch/configuration inputs versus observed false, independent action outcomes.
- Real main PNG decode and actual atlas PNG decode reach the same owner's plain-image and frame
  resolvers. Shared keys read once per pass; retry transitions Missing→actual pixels without resetting
  the stage index or replaying successful name/flag/caption work.
- Unsupported services remain visible after later supported actions execute; no native readiness.
- Separate duplicate choices, transactional failed open, same-ID reopen/stale ticket, close/release.
- Regular-window versus Animated Icon choices and unknown/suppressed stage captions stay distinct.
  A clearly controlled window-ControlID variant exercises an available getter → stage-target path;
  it is not presented as a new observed original layout or recovered dynamic lookup behavior.
- Unknown/ambiguous/unsupported/read failure, thrown backend and dishonest Found lacking pixels.
- Provider reentry, template/sibling isolation, exact Cyrillic/emoji UTF conversion, empty/malformed/
  oversized captions, moved-root local centering and root/missing-target rejection.

The [direct streaming provider](gge-streaming-images.md) now reads the original large Graphics package
without a constructed DBPF carrier. Tests also pass that production provider directly to the lifecycle
on both layouts (228→232 checks). The old narrow memory adapter's limits/discovery scope stay unchanged.
This new verifier asserts resolver state, not a new GPU/full-card/galaxy screenshot; prior image/
Animated Icon D3D9 readback tests cover their controlled GPU subsets separately.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_lifecycle_verify
.\build\Debug\gge_star_lifecycle_verify.exe <SPUI-fixtures> .\build\bin\Debug\Data\PatchData.package .\build\bin\Debug\Data\Spore_Graphics.package
python tools/gge_evidence.py --static --lifecycle
```

Evidence: [host/native distinction and centering constant](audit/gge-star-lifecycle/facts.json),
[centering instructions](audit/gge-star-lifecycle/local-centering.asm.txt), existing
[phase-1](gge-star-phase1.md), [images](gge-image-bindings.md), [Animated Icon](gge-animated-icon.md).
This step reuses retained local Ghidra/objdump evidence; it does not introduce new REA Evidence IDs.

Next: scene/active-camera ownership contracts, then real Available catalogue input and per-star
projection/selection integration. Native service
callbacks/listeners and phase ordering remain separate work, not inferred from this host pass.
The [host scene follow-up](gge-star-scene.md) now owns multiple lifecycles, applies guarded pixel
placement and publishes current layers/hits only from explicit Available catalogue/active-camera
observations. It does not turn this independent-action policy into native phase/mount completion.

Final Debug x86 warning-free. The lifecycle checks and all prior phase1/instance/projection/
parser/catalogue/DBCheck/Intro/SPUI/package suites PASS; image80, AnimatedIcon and D3D9 scissor
readbacks PASS. Six Python tests, 55 maintained Markdown link checks and dashboard --check PASS.
Launcher/Main → GGE/visible launcher → CreatureEditor smoke runs exit0, aborts/asserts0, with the
MoviePlayer observation still explicitly Unknown. New launcher capture is RGB pixel-identical to
`analysis/shots/spui_e3launch_uv.png` (diff bbox=None). These smoke runs do not invoke this new owner
automatically, attach a live catalogue or establish native galaxy/scene readiness.
