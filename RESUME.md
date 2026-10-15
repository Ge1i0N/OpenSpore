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
    Then prioritize the visible boundary: 0xDF9200 per-entry GGEGameStarUIv4; 0xDF6010 -> 0xDE3C20
    world-to-screen; 0xDF8300 marker effects, without forcing Intro/state2 or fake source readiness.
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
cmake --build build --config Debug --target OpenSpore spui_verify pkg_lifetime_verify overlay_clip_verify mode_ui_verify gge_intro_verify gge_dbcheck_verify gge_catalog_verify starsdb_verify starsdb_inspect starsdb_links_verify starsdb_context_verify starsdb_context_inspect starsdb_dbpf_verify starsdb_dbpf_inspect starsdb_postload_verify save_header_verify save_header_inspect planet_record_verify planet_record_inspect
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
