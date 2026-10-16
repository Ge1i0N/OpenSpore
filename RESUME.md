# RESUME — current hand-off

Last reviewed: **2026-10-15**. Work from the actual `git status` / `git log`, not an old SHA here.
Commit locally; the user pushes through GitHub Desktop. Do not rewrite their changes.

## Goal and rules

Walk GA from Main forward to a fully playable standalone build. Address authority:
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`; beta is semantic reference only.
Use GA disassembly / persistent Ghidra / REA before inferring behavior. Distinguish observed
instructions, shipped data, host policies and missing behavior. Build Debug x86 warning-free;
test with `-w -devDirs`; retain evidence and commit.

## What works now

- Boot / D3D9 / console / consolidated log / capture / fault-tolerance harness; 10/10 boot markers.
- DBPF / RefPack / naming / nested locale packages; 57 packages, 150,945 records in the test corpus.
- SPUI sequential reader: 748/748 exact EOF, 93,726 objects, 22,384 windows, 20,659 Children links.
- Atlas-aware launcher: Image UV/logical size, None-tiling ImageDrawable fixed scale/alignment,
  non-tileable Standard/Radio frame resolver. Screenshot: [current launcher](analysis/shots/spui_e3launch_uv.png).
- Visible `(311,550)` -> CreatureEditor `0x022D9E01`. This is a reconstructed launcher bridge,
  **not** general command dispatch or an implemented editor. Old `(942,261)` selected hidden GGE.
- `Package` RAII fixed FILE leaks; 544 scoped cycles at CRT limit 512 pass.
- `inherited_clip` now drives picking and D3D9 per-window scissor; nested/empty/boundary cases and
  real quad/text/reset pixel readback pass. This aligns our host policy, not full original layout parity.
- Main -> GalaxyGameEntry now presents the code-confirmed default primary layout `GlobalUIGGE-9`.
  **Not the galaxy scene or driver lifecycle**: only 9 serialized windows currently visible.
  `analysis/galaxy-entry.md` and `audit/galaxy-entry/` retain lifecycle slots, constructor defaults,
  property overrides and exact UI-loading path. `mode_ui_verify` tests binding/hash boundaries.
- Handler at `0xDEA4B0` is Intro, not galaxy loading. `analysis/gge-intro.md` traces title,
  MoviePlayer +0x14 (`0xFD7550`), DBCheck and event `0x08085C4C`. Post-title decision/latch subset
  implemented; `gge_intro_verify`: 2040 controlled-signal checks. Runtime uses explicit Unknown
  MoviePlayer observation and never generates DB completion or state10 transitions. Screenshot
   shows this block reason; the phase6 input is a model boundary, not an executed original title.
- DBCheck branch executor `gge_dbcheck.cpp` now exists behind explicit `DbCheckBackend` ports;
  tested on controlled effects + real dialog fixture `0x4B0AC091` / root `0x0431E538` (101 objects,
  24 windows). It is NOT attached to live runtime: Unknown services stay blocked. See
  `analysis/gge-dbcheck-play.md` / evidence `audit/gge-flow/`.
- State2 handler ctor `0xE008B0` is **Play Spore**, name method `0xE00930`. Enter `0xDF9620` rebuilds
  live game records (`0xDF95A0`), then marker effects and per-game GGEGameStarUIv4. Source catalog is
  manager `0xB3D3A0` / second list `0xBA6450`; missing catalog is not an empty-but-ready scene.
- Catalog projection `engine/gge_catalog.*` now builds owned entries from typed metadata: saved
  first then candidates, two source indices, raw world positions, stage/resource/save-status rules.
  `gge_catalog_verify`: 19 grouped controlled-input checks. No live producer or on-disk ABI parser.
  Source fill `0xBB61C0`, stars.db load `0xBB8C90` and save `0xBB4D40` traced and retained;
   `analysis/gge-catalog.md` / `audit/gge-catalog/`. Do not reinterpret 0xBC as a file stride.
- `engine/serialized_frame.*` / `starsdb.*` / `starsdb_links.*`: bounded serializer prefix, six metadata
  fields, variable-width wire references/grid and 14 StarRecord fields; `starsdb_verify`: 240 and
  `starsdb_links_verify`: 362 constructed checks. No real-save corpus, outer-transport guarantee,
  context identity/resource fixups or runtime wiring. `analysis/starsdb.md` / `audit/starsdb/` retain
  focused GA functions, constructor schemas and instructions. `tools/pe.py`
  now rejects BSS and reads crossing a raw section boundary; two independent Python tests cover it.
- `engine/starsdb_context.*` adds explicit DECODED index/class-stream parsing, envelope CRC and
  `(class_id,token)` structural lookup into StarRecord fields. `starsdb_context_verify`: 126 constructed
  checks; `starsdb_context_inspect` read-only CLI smoke PASS. Physical adapter is separate (below);
  no factories/resource fixups/live producer. `analysis/starsdb-context.md` / `audit/starsdb/context/` retain evidence.
  Context outer map is class_id -> nested token, not token -> nested class. CRC covers only
  min(total_record_bytes-8,1000000) bytes starting at token; envelope length INCLUDES its 12-byte header.
- `engine/starsdb_dbpf.*`: single explicit read-only DBPF 3.0/index3 adapter to root/context/classes;
  exact ResourceKey (instance,type,group), duplicates/missing/errors explicit, structural output atomic.
  `starsdb_dbpf_verify`: 266 constructed checks / 270 with optional explicit-path smoke. No real DB
  found under workspace `**/stars.db`; no profile scan. RefPack now bounds every append by declared
  decoded size. `analysis/starsdb-dbpf.md` retains key-order evidence and host version/size policies.
- `observe_post_load` / `starsdb_postload_verify`: **CORRECTED** 0xBBA6E0. Global name pointer initially
  points to BAD_DATA; EQUALITY (not mismatch) triggers regeneration; empty name first generates once.
  15 ordered-call/Unknown/profile checks; no service writes. Previous canonical-name, mismatch-cleanup,
  position-cache and Unknown->Changed interpretations were wrong and removed. `+0x80` purpose remains
  unestablished. See `analysis/starsdb-postload.md` / refreshed `audit/starsdb/postload/schema.json`.
- `engine/save_header.*`: first root frame schema 0x1568B08, initializer 0x131B210. File version IDs
  0x04162D88/89, unsigned major>=33 predicate (0xB27838 CMP/SETAE), NOT StarRecord saved versions.
  `save_header_verify`: 116 constructed checks including DBPF and controlled GGE predicate integration;
  `save_header_inspect` reads caller-selected file only. Timestamp/mode/planet key decoded; map/snapshot/
  root tail remain opaque; missing version fields block probe rather than become false/zero.
  `analysis/save-header.md` / `audit/starsdb/save-header/` retain evidence and planet-name boundary.
- `engine/planet_record.*` walks the real structural prefix (civs/cities/vehicles/tribes/events/nodes)
  before six PlanetRecord fields; city has 28 raw bytes before its frame. Exact counted keys from
  star.mKey+i*0x01000000/type0x05220CB8/group1; star flags0x40 filters planet flags0x100; first selected
  planet name is saved display/save-stem. Planet BAD_DATA generation differs from StarRecord (no
  empty-name branch; only assigns nonempty generation result when !(flags&1)).
  `gge_saved_adapter.*` joins these decoded values + explicit HeaderProbe/terrain observations into
  typed saved metadata, no catalogue availability publication. `planet_record_verify`: 291 constructed
  checks, read-only CLI smoke. No real planet/save corpus or services. `analysis/planet-record.md`.
- `engine/gge_star_present.*`: observed camera +0xC0 matrix -> project via 0x7C4250/0xDE3C20;
  root 0x02E5A4A8 center/saved clamp and >4px deadband via0xDF6110; stage/caption/raw flag plans via
  0xDF9200. `gge_star_present_verify`: 50 checks binding both real GGEGameStarUIv4 (0x21DF01F2),
  each 152 objects/58 windows. Camera/window/root/current/margins/global flag Unknown blocks relevant
  helper; no default identity/cull guessed. NO runtime camera creation/mount/effects, screen unchanged.
  `analysis/gge-star-present.md` / `audit/gge-star-present/` retain evidence.
- `engine/gge_star_instance.*`: owned noncopyable host window graphs, explicit object-index flags/
  caption refs, atomic root/descendant movement, effective ancestor flags and pixel picking. 167
  checks on two real layouts plus controlled graph errors. Duplicate ControlID0x026E0002 maps to
  objects45/55: no first-ID collapse or native dynamic lookup assumption. Shared renderer now has
  `states::draw_spui_pixel_layer` (1:1 pixels, no state/click dispatch); GGE has no live caller yet.
  Native mount remains false: neither DF7690 load result nor DF6D80 return proves service completion; plugins/listeners/
  thumbnail/profile dependencies missing. Scene setup traced DE46D0->DDD920->DDD150 plus separate
  GGE effects world0x02DDA976; exact factories/camera identities not all recovered.
  `analysis/gge-star-instance.md` / `audit/gge-star-mount/` retain 17 functions and corpus identities.
- `engine/gge_star_phase1.*`: ordered phase1 case-body binding plan, 247 checks on both real layouts.
  Missing thumbnail/profile/selected-star branch observations stop with bounded prefix/blocked_on;
  structural Found/Missing/Ambiguous targets retain candidates, explicit choices checked; assets always
  Unknown (no resource read). Image drawable QI0xEF3C47CF differs from Animated Icon0x0106F146.
  **CORRECTION**: DF6D80 normal paths return1 including missing-root skip, not a false/error dependency
  validator. No native operations/phase latch/listeners/readiness or automatic runtime caller.
  `analysis/gge-star-phase1.md` / `audit/gge-star-phase1/` retain 8 functions and return instructions.
- `gge_image_provider.*`: explicit bounded DBPF PNG provider, actual decode/provenance and one image
  request executor; `StarUiInstance` get/set CF3C4EAE fill image with per-drawable `Layout::runtime_images`.
  Shared decoded pixels/atlas logical sizes survive transfer; source/sibling/raw serialized Objects
  untouched. Resolver + painter consume override pixels, not global first-key cache. Unsupported
  getter/classes (Animated Icon separate), Unknown/failure/ambiguous targets preserve old image.
  `gge_image_verify`: 80 checks with actual PatchData PNG record94 128x128, real SPUI layouts and
  controlled red2x2 D3D9/scissor readback. Not a full phase executor/live GGE scene.
  `analysis/gge-image-bindings.md` / `audit/gge-images/` retain setter/getter/interface evidence.
- Animated Icon static selector: StarUiInstance explicit window get/set frame/pixels, Layout map
  runtime_animations separate from fill-drawable overrides; atlas UV/logical size retained.
  CORRECTION QI0106F146 returns object, DF9200 then takes+20C (QI0105A93D itself returns+20C).
  Constructor frame1 ONLY: native normalizer7F5E30 later resets0/connects backend (not executed here).
  Host pixel attach preserves constructor/caller index, not native rebind. Setter invalid indices
  no-op returning true; host Changed/Unchanged/IgnoredOutOfRange.
  Only AutoPlay=false/explicit positive bounded grids/obsolete width0; actual icons45/93 both Image44,
  frames5 rows1 columns5. Actual atlas key B89D2CD2/2F7D0004/31A44893 in Graphics record15591,256x512.
  gge_animated_verify:159 checks (four carrier-open assertions removed), direct original atlas PNG, controlled
  2x3 red/blue GPU readback. Native normalization/playback/live GGE caller unimplemented.
  analysis/gge-animated-icon.md / audit/gge-animated/ retain seven functions and property corpus.
- gge_star_lifecycle.* now owns one immutable input/instance/report snapshot: transactional open,
  generation ticket per successful open (even same host ID), apply_available/close; reentry rejected.
  Independent host action policy (NOT original dependency/order): flags/local-floor centering,
  UTF16 name, locale ref binding, image executor and separate config frame/pixels/flags/caption.
  Applied/ref-bound/observed/no-op actions settle; unresolved errors retry; one exact resource read
  per pass including failures. Tooltip/layer/profile/timestamp/terrain/native phase/listeners remain
  Unsupported or structurally missing. Null provider Unknown; thrown/dishonest backend Failed.
  Separate phase1/config duplicate choices, no fake locale availability or native phase completion.
  gge_star_lifecycle_verify:232 checks, actual PNG/layouts, direct StreamingImageProvider/lifecycle path,
  controlled metadata/getter variant/reentry/UTF/floor/ownership tests. Screen unchanged.
  analysis/gge-star-lifecycle.md / audit/gge-star-lifecycle/ retain policy/native boundaries.
- StreamingImageProvider in gge_image_provider.*: explicit file-backed DBPF3.0/index3, <=4GiB file,
  <=65536 records/4MiB directory/64MiB packed+decoded resource/4096 PNG axis. Exact key+duplicates,
  shared decoder with old <=256MiB memory provider (unchanged), no priority/discovery/profile scans.
  Package::open_bounded checks allocations on same handle; file seek/tell now64-bit; minor version
  retained, mismatched/unknown directory no longer reports parsed. Direct original Graphics atlas
  B89D2CD2/2F7D0004/31A44893 record15591 256x512 now feeds animation/lifecycle without carrier.
  gge_streaming_image_verify:95 checks, all masks/raw/RefPack/errors/bounds/sharing, sparse data+index above
  2GiB, >4GiB rejection,544 successful/failed-reopen ownership cycles. No new scene readiness.
  analysis/gge-streaming-images.md records host limits/direct integration and mutable-source boundary.
- gge_star_scene.*: explicit Available saved+candidate CatalogInput -> bounded owned lifecycles,
  tagged ActiveProjection observation -> project/place/publish layer views + reverse catalogue pick.
  Host camera token not native pointer; ControllerRetained (+198) not substituted for renderer+58.
  Scene apply_available retires projection frame; camera loss retires old draw/pick, per-card zero-w/
  invalid saved clamp failures excluded. Unknown catalogue refresh suspends updates/layers/hits,
  retains old cards for diagnostics ONLY; Available-empty distinct. Unique host IDs, duplicate star
  IDs/source ordinal resets retained, tickets/frame serials/reentry guards,128cards/65536objects/32768windows.
  states::draw_gge_star_scene uses shared pixel-layer hook, no begin/end/clear or automatic caller.
  gge_star_scene_verify:204 checks on actual layouts/PNG with controlled camera/metadata; untouched
  template remains hidden/IgnoreMouse after current phase subset. Picking test clears ONLY root177
  IgnoreMouse in controlled copy, not native show behavior. No new GPU galaxy/native mount readiness.
  analysis/gge-star-scene.md / audit/gge-scene/ retain14 functions, globals and separate camera paths.
- gge_star_transition.*: explicit DF6D80 requested2..5 + observed native UI-data+2C equality guard,
  distinct per-transition duplicate choices; Unknown current -> blocked no-body, equal -> no-op.
  Host lifecycle/scene apply flags(mask1)/entry name/literal local rect only, separate transition
  report. Event436350/436390 and67CA60 service request Unsupported, no native latch or auto input route.
  CORRECTION case4 returns1 WITHOUT assigning+2C; case3 latch before name tail. Source34->target60
  copies RECT(four floats) via95F950/95FDA0, not a layered-object relation; phase1 80DE40 still unsupported.
  F8 callerDF7BF0 saved->3/candidate->2 whenFC null; FC callerDF7AC0 saved->5/candidate->4 only explicit flag.
  gge_star_transition_verify:195 checks, actual untouched-layout panel41 hit with root177 flags19
  unchanged; no controlled IgnoreMouse workaround needed in this new test. Camera/current-phase/
  catalogue observations still controlled, no new GPU native galaxy/live caller.
  analysis/gge-star-transition.md / audit/gge-star-transition/ retain10 functions/corpus/instructions.
- gge_star_input world/UI seam: actual DF6490→7C45E0/698B30 host point-segment nearest entry,
  threshold15, explicit ActiveProjection ray token; UI pick remains current-frame layer reverse pick.
  Handler table +5C DF7BF0 hover/+60 DF7AC0 selected/+64 DF6110 project/+68 DF6010 update retained;
  raw DF7800 event identities not generalized. 29 checks, no F8/FC/CommandID/tooltip/message dispatch.
  analysis/gge-star-input.md / audit/gge-input-seeds.tsv / audit/gge-input/.
- Debug-only `GGEHostPreview` provides the first physically clickable host-card screen:
  source/states/ggehostpreview.*, registered only under OS_ENABLE_DEV_COMMANDS. Explicit three typed
  candidates/positions, actual GGE SPUI/direct Graphics atlas, explicit camera, case2 host transition,
  draw_gge_star_scene and normal input latch. `-state:GGEHostPreview`; `-exec spui:-click,640,360
  -execAfter 10` logs `picked host card 20481 (host only)`. Clean smoke exit0/aborts0. Controlled
   reconstruction only; no live catalogue/native camera/F8/FC/CommandID. analysis/gge-host-preview.md.
- First gameplay step: `engine/game_stages.*` holds the five playable-stage rows
  (mode/tag/registrar/object size/state/next, cross-checked against binary and script);
  `Game_Cell` owns a skeleton driver attached to `CellGame` (enter/tick/exit counter,
  no simulation). Other stages keep the honest not-implemented path.
   `analysis/game-stages.md`; `game_stages_verify` checks table/chain/lifecycle.
- Real-save probe: `tools/real_save_probe.py` replays the pkg validator and the
  strict 3.0/index3 gate against one explicit file. All four beta saves are DBPF 2.0
  (adapter correctly refuses); `EditorOfflineUser.db`'s single record carries the
  documented root triple `(0x01897C18 x3)`. Reports in `analysis/audit/save-probe/`.
  `analysis/real-saves.md`; constructed-input tests pass without game data.
- Fresh GA save (user-created 2026-10-16) confirms the key spaces on real 3.0 data:
  `stars.db` has the exact root + context keys plus five class records; fresh
  `planetRecords.pkp` holds 65 records of type `0x05220CB8`/group 1 on the exact
  `mKey + i·0x01000000` instance stride. Reports in `analysis/audit/save-probe/`.
- Payload decode of the fresh `stars.db` (`tools/starsdb_real_decode.py`): 7/7 records
  decompress to `memSize`; index version 1 / 5 classes / 46,225 with exact count-sum;
  46,225/46,225 envelopes CRC-clean; root frame carries the six metadata IDs and the
  first StarRecord body is a 14-field frame. `analysis/starsdb-real.md`, report in
  `analysis/audit/starsdb-real/`. Field semantics still static-only.
- Successive saves (fresh → Game0.old → Game0): same 5 classes, all decodes clean;
  StarRecords fixed at 42,010 while classes `0x042232A1` (1 → 5) and `0x018EB9D2`
  (+4) and planet rows (65 → 97 → 114) grow with play. Six creature `.spo` are
  3.0-shaped (12–43 records); stage mapping unknown.

## Next, in order

1. Recover needed drawable tiling/outline and Button capture/release/checked behavior from GA.
2. Define actual MoviePlayer/no-movie host contract and DBCheck dependencies (`0x60B5D0`), not
   package-count readiness. Handler Intro update is traced; gate is waiting on missing services.
   The branch executor has been tested but needs actual UI/content-manager/WinProc ports; callback
   `0x60B470` is not established as per-frame and must not be polled into fake completion.
   Continue state2 constructor `0xE008B0` and state10 (`0xDEC320/0xDEC3B0`) return/effect lifecycle.
    Source task: validate the implemented read-only physical DBPF adapter on a real explicitly selected
    database when available; do not scan profiles or use synthetic fixtures as saves. Context/root keys
    (ResourceKey memory order instance,type,group: context `(0x0179D304, index=0x0179D310 or class_id, 0)`;
    root `(0x01897C18,0x01897C18,0x01897C18)`). Key translation established at 0x8DB310; older
    package versions not supported. Continue StarRecord name/resource/property fixups and save metadata,
    then grid/record graph and galaxy/star-grid
    generation, before attaching the typed catalog builder to live UI. Post-load observation subset is
    corrected/tested; actual generation/profile/property ports and save-stem producer remain open.
    Planet structural reader/selection + typed saved adapter are implemented; join context saved-list
    references with explicit planet/header/terrain sources, keeping all missing/unsupported statuses.
    Visible boundary: math/config + owned host geometry/mutation/pick/pixel-layer hook are implemented,
    but not attached to GGE. Phase1 branch/control/resource plan now exists; next image-provider/drawable
    adapter with explicit resolved object targets/actual resource failures is now implemented for PNG
    plain ImageDrawable only (gge_image_provider). The host lifecycle now joins independent image/
    frame/name/flag/centering actions with explicit outcome/retry, not native services/order/listeners.
    Direct production streaming PNG provider and host scene observation/ownership collector are now present.
    Explicit show/phase body subset now implemented with separate observed guard, no native latch;
    world/UI input seam is observation-only. Next: actual raw callback/service route, active/current-phase producer and explicit genuine
    catalogue adapter integration/GPU card validation. Native factory/effects/playback/grid normalization
    remain separate. Plan itself executes no
    services and native return1 is not readiness; image-operation success is not phase completion.
    Continue thumbnail/main-image/plugin ports (DF6D80)
    and scene factory contracts (DDD150 via67DD40/50, render stages67DD10), actual camera acquisition
    from renderer67DCD0(+50 -> returned +38 versus active +58). Controller +198 is not proven the
    same camera as active projection. Effects world separate67DD90. Then source-to-instance lifecycle
    and real draw/selection; no forced Intro/state2 or fabricated source/camera readiness.
    Header word decoder is implemented but filesystem missing/error
    semantics and actual save-name producer are not. Frame/metadata and wire-shape
    decoding are bounded subsets; do not repeat BSS reads of `0x1689C50` or treat context values as
    coordinates.
    Controlled test records
   are not shipped saves or a recovered galaxy. No filesystem writes are authorized by this step.
   The current primary UI is diagnostic; original Intro hides it. Never force hidden branches visible.
3. Advance the first playable stage without claiming full UI or full format parity.

Dynamic sibling Z-order and layout WinProcs are still open. Forward serialized sibling traversal,
ancestor-origin translation, resource override choices and pointer-to-drawable state are host policies.

## Repeatable checks (from repository root)

```powershell
cmake --build build --config Debug --target OpenSpore spui_verify pkg_lifetime_verify overlay_clip_verify mode_ui_verify gge_intro_verify gge_dbcheck_verify gge_catalog_verify starsdb_verify starsdb_inspect starsdb_links_verify starsdb_context_verify starsdb_context_inspect starsdb_dbpf_verify starsdb_dbpf_inspect starsdb_postload_verify save_header_verify save_header_inspect planet_record_verify planet_record_inspect gge_star_present_verify gge_star_instance_verify gge_star_phase1_verify gge_image_verify gge_animated_verify gge_star_lifecycle_verify gge_streaming_image_verify gge_star_scene_verify gge_star_transition_verify gge_star_input_verify game_stages_verify
$fixtures = Join-Path $env:TEMP 'OpenSpore-spui-fixtures'
python tools/spui_objects.py --parse --all --output analysis/audit/spui_objects.json --extract $fixtures
.\build\Debug\spui_verify.exe $fixtures
.\build\Debug\mode_ui_verify.exe
.\build\Debug\gge_intro_verify.exe
.\build\Debug\gge_dbcheck_verify.exe $fixtures
.\build\Debug\gge_catalog_verify.exe
.\build\Debug\starsdb_verify.exe
.\build\Debug\starsdb_links_verify.exe
.\build\Debug\starsdb_context_verify.exe
.\build\Debug\starsdb_dbpf_verify.exe
.\build\Debug\starsdb_postload_verify.exe
.\build\Debug\save_header_verify.exe
.\build\Debug\planet_record_verify.exe
.\build\Debug\gge_star_present_verify.exe $fixtures
.\build\Debug\gge_star_instance_verify.exe $fixtures
.\build\Debug\gge_star_phase1_verify.exe $fixtures
$imageCapture = Join-Path $env:TEMP ('OpenSpore-image-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\gge_image_verify.exe $fixtures .\build\bin\Debug\Data\PatchData.package $imageCapture
$frameCapture = Join-Path $env:TEMP ('OpenSpore-frame-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\gge_animated_verify.exe $fixtures .\build\bin\Debug\Data\Spore_Graphics.package $frameCapture
.\build\Debug\gge_star_lifecycle_verify.exe $fixtures .\build\bin\Debug\Data\PatchData.package .\build\bin\Debug\Data\Spore_Graphics.package
$streamDir = Join-Path $env:TEMP ('OpenSpore-stream-' + [guid]::NewGuid())
.\build\Debug\gge_streaming_image_verify.exe .\build\bin\Debug\Data\Spore_Graphics.package $streamDir
.\build\Debug\gge_star_scene_verify.exe $fixtures .\build\bin\Debug\Data\Spore_Graphics.package
.\build\Debug\gge_star_transition_verify.exe $fixtures
.\build\Debug\game_stages_verify.exe
.\build\Debug\pkg_lifetime_verify.exe .\build\bin\Debug\Data\Locale\en-us\UI.package
$clipCapture = Join-Path $env:TEMP ('OpenSpore-scissor-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\overlay_clip_verify.exe $clipCapture
python tools/pkg_kinds.py
python -m unittest discover -s tools/tests -p 'test_*.py'
python tools/feature_map.py --check
python tools/docs_check.py
```

Expected parser regression: 748 corpus matches / 486 malformed inputs rejected with output unchanged,
plus drawable atlas/state/fixed-scale/alignment checks. Build configuration must remain Win32 Debug.

From `build/bin/Debug`:

```powershell
.\OpenSpore.exe -w -devDirs -state:DevLauncher -quitAfter 30 -capture launcher.bmp -captureAfter 20
.\OpenSpore.exe -w -devDirs -state:DevLauncher -quitAfter 18 -exec spui:-click,311,550
```

`-quitAfter` counts **frames**. Log appends across runs; inspect only the new run when comparing.
Expect exit 0, no aborts/asserts, visible CreatureEditor transition; a warning about unimplemented
editor modes is expected, not proof that the editor works.

## Evidence / documentation

- [Current runtime status](PROGRESS.md) · [verification ledger](VERIFY.md)
- [SPUI framing](analysis/spui-binary.md) · [drawable implementation/evidence](analysis/spui-drawables.md)
- [Modes](analysis/modes.md) · [analysis index](analysis/README.md) · [toolchain](analysis/toolchain-rea.md)
- Progress source: `analysis/features.json`; generated PNG/Markdown/stats: `python tools/feature_map.py`.
  Counts are a bounded checklist, not % of game completion. UI navigation is separate from gameplay.
- [Archived hand-offs](docs/archive/RESUME-2026-10-15.md) are history, not current task instructions.
