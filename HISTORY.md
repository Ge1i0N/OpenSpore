# HISTORY — what changed, and how it was verified

Every entry ends with its verification. If a change has no measurable delta it is labelled
**hypothesis**, per the output contract in `ai-brief-and-commands.md`.

> Chronological record, not current instructions. Historical counts, percentages, hidden GGE
> demos and disproven hypotheses are retained as such. Start with [README](README.md),
> [PROGRESS](PROGRESS.md) and [VERIFY](VERIFY.md) for today's verdict.

## 2026-10-16 — MSVC Win32 Debug builds warning-free, full suite green

User installed BuildTools 17.14 (MSVC 19.44.35229) after two silent CLI failures
(workload checkbox state vs payload confusion; resolved via GUI Modify + Repair
awareness). `cmake -S . -B build -G "Visual Studio 17 2022" -A Win32` configures
clean; all 30 targets build with **0 warnings / 0 errors**. All 29 verify tools
PASS on MSVC binaries — notably `gge_streaming_image_verify` (95) and
`gge_star_instance_verify` (167), which closes both MinGW deviation investigations
(SSE2 FP model and UCRT errno mapping confirmed environmental). Six app smokes
exit 0 with aborts/asserts 0: headless list (10/10 boot), CellGame skeleton +
capture, DevLauncher + capture, click → CreatureEditor, Main → GGE + capture,
host card pick. Python suite 21/21, `feature_map --check`, `docs_check` PASS.

## 2026-10-16 — MinGW x86 logic-verification build, zero source changes

No admin for MSVC, so: user-local WinLibs i686 GCC 16.1.0 + present Ninja, out-of-tree
`build/mingw` (toolchain/shim/project-include, all gitignored; recipe retained in
`analysis/mingw-logic-build.md`). Full tree compiles/links (346 steps): OpenSpore.exe
+ all 29 verify tools. Every check PASSes except `gge_streaming_image_verify`'s
truncate case (EACCES vs expected SHARING_VIOLATION — standalone probes confirm the
file stays protected; UCRT errno mapping, not engine logic). Along the way the MinGW
build caught a real sensitivity: `gge_star_instance_verify` failed under x87 excess
precision and passes with `-mfpmath=sse -msse2 -ffp-contract=off`, independently
confirming `/fp:precise` is load-bearing. App runs: headless 10/10 boot, CellGame
skeleton active + 1280x720 capture, DevLauncher/Main→GGE captures, visible click →
CreatureEditor, GGEHostPreview card pick — all exit 0, aborts/asserts 0.
Verification: previously Python-only claims (boot chain, skeleton, transitions) now
also executed as compiled x86 code. MSVC warning-free build still pending.

## 2026-10-16 — StarRecord values decode clean on 84,020 real records

`--star-values` value-decodes every StarRecord body of `Game0.old` and `Game0` with
`inspect_star_record` shapes: 42,010/42,010 pass, zero failures on both; mKeys all
unique; positions all finite on a thin disk (X ±748.6/686.2, Y ±667.9, Z ±9.2);
all 42,010 named, none empty; planet histogram {0: 41980, 5: 12, 3: 12, 12: 1, 1: 5}
shifts with play, consistent with player-relevant `planetRecords.pkp` growth;
7 distinct species keys; version pair present everywhere but opaque (hundreds of
distinct wide u32 pairs, no semantics claimed); all 14 field IDs present on every
record with zero unknown IDs — the schema is closed on real data.
Verification: 6 constructed-input tests PASS; suite 21/21; `feature_map.py --check`
and `docs_check.py` PASS. Reference resolution and catalogue publication untouched.

## 2026-10-16 — successive saves: galaxy fixed, progress classes grow

The user played on: the slot now spans fresh → `Game0.old` → `Game0` plus six
creature `.spo`. All three `stars.db` decode clean with identical shapes; the galaxy
is fixed at 42,010 StarRecords (same bytes size) while class `0x042232A1` grows
1 → 4 → 5, class `0x018EB9D2` gains 4, root mem grows, and planet rows go 65 → 97 →
114 in the confirmed type/group space. All six `.spo` are DBPF 3.0/index3 kind 4
(12/39/43/40/40/26 records in creation order); their stage mapping is unknown.
Reports `game0-old.json` /
`game0.json` retained; class-ID semantics not decoded (counts only).
Verification: three DECODED_STRUCTURE verdicts, no notes; suite stays 20/20.

## 2026-10-16 — creature saves in creation order: three structural families

Probed the six `.spo` oldest-first (player: stage by stage). Counts 12/39/43/40/40/26
in three type families: minimal (#1, marker class `0xABCDEF00`), editor menagerie
(#2/#3, 31 shared types), snapshot-shaped (#4 `snapshot.spo`, #5 with an identical
40-type set — same DB shape, different creature: a transition auto-save reused seconds
later). #6 embeds StarRecord `0x03E4353C`. Keys only, payloads untouched; stage names
per file confirmed by the player as cell → creature → tribe → civ → space
(`snapshot.spo` = transition auto-save), docs updated accordingly.

## 2026-10-16 — stars.db payload decode on the fresh save

`tools/starsdb_real_decode.py` structurally decodes the fresh `stars.db` read-only:
7/7 records decompress to their index `memSize` (RefPack via the instruction-derived
decoder); context index version 1, 5 classes, total 46,225 with exact count-sum
equality; envelope counts match declarations 5/5 with **zero CRC-32/BZIP2 failures**
across 46,225 envelopes (largest 19,706 bytes, under the cap); root opens with tag
`0x01A80D26` and exactly the six metadata field IDs in documented order; the first
StarRecord body is a 14-field `0x01A80D26` frame ending in the version pair the C++
reader handles. Findings in `analysis/starsdb-real.md`, report retained in
`analysis/audit/starsdb-real/fresh.json`; the "no real-save corpus" caveats in
`starsdb.md`/`starsdb-context.md` corrected to structural-only validation.
Verification: 5 new constructed-input tests PASS (CRC pinned by the published
`123456789 → 0xFC891918` vector); suite 20/20, `feature_map.py --check` and
`docs_check.py` PASS. Field-value decoding and reader execution still need MSVC work.

## 2026-10-16 — fresh GA save confirms the adapter key spaces

The user created a new save in-game; `%APPDATA%/SPORE/Games/Game0/` now holds a real
GA corpus. `stars.db` (2.4 MB) is DBPF 3.0/index3, kind 4, 7 records: the exact root
triple and context index the adapter looks up are both FOUND uniquely, plus five class
records under instance `0x0179D304` — the root → index → per-class shape
`inspect_dbpf_context` consumes. Fresh `planetRecords.pkp` holds 65 records, all type
`0x05220CB8`/group 1 on the exact `mKey + i·0x01000000` instance stride the planet
reader expects. `GGEUserData.dat` repeats the root triple; `PlanetScripts.pld`,
`lastSave.pld` and a creature `.spo` are 3.0-shaped, payloads not decoded. Reports in
`analysis/audit/save-probe/`, findings folded into `analysis/real-saves.md`.
Verification: probe reruns green on all files; `feature_map.py --check` and
`docs_check.py` PASS; Python suite stays 15/15. Reader execution against the real
save still needs an MSVC build; the corpus is two early-game saves, no per-stage saves.

## 2026-10-16 — real-save probe on beta UserData

Staged Steam data reproduces the corpus exactly (`pkg_kinds.py`: 57 files / 150,945
records, RULE HOLDS, 0 violations). New `tools/real_save_probe.py` replays the pkg.cpp
header validator and the strict starsdb 3.0/index3 gate against one explicit file
(read-only, no scans). All four beta saves are DBPF 2.0 and correctly refused; the
single record of `EditorOfflineUser.db` carries the documented root triple
`(0x01897C18 x3)` as raw index dwords (2.0/3.0 layout equivalence not claimed);
`planetRecords.pkp/.pkt` are valid empty DBs; no `stars.db` on either disk. Reports in
`analysis/audit/save-probe/`, findings in `analysis/real-saves.md`.
Verification: 5 new constructed-input tests PASS (15/15 suite green, no game data needed);
`feature_map.py --check` and `docs_check.py` PASS. A kind_info(8) expectation was
disproven during testing (C++ masks to 3 bits, so 8 is accepted there too) and corrected.

## 2026-10-16 — cell stage skeleton driver, data staging, RE skills

First gameplay step, kept out of the GGE files: `source/engine/game_stages.*` holds the five
playable-stage rows (mode/tag/registrar/object size/state/next from analysis/modes.md and the
shipped SporeAppStates script). `Game_Cell` owns a skeleton driver attached to `CellGame`
(enter resets, tick counts while active, exit parks; no simulation, no rendering). The generic
placeholder frame names the skeleton and its tick count instead of NOT EXECUTED. Runtime data
(`Data/`, `DataEP1/` from the Steam install) staged under `build/bin/Debug/`; beta reference
(`SporeApp.exe` + PDB, byte-identical to the documented Beta190208) staged under `refs/`.
Two RE skills vendored into `.opencode/skills/` (`reverse-engineering`, `ghidra-iterative-re`).
Verification: `game_stages_verify` checks table/lookups/chain/lifecycle; new
`tools/tests/test_game_stages_table.py` cross-checks the transcription against modes.md and the
script (4 tests PASS); `feature_map.py --check`, `docs_check.py` and the full Python unittest
suite PASS. C++ compilation still requires MSVC (absent on this machine), so the MSVC
warning-free build and the `-state:CellGame` runtime smoke are pending on a machine with
Visual Studio Build Tools.

## 2026-10-15 — catalogue sources and typed GGE entry projection

After restart, confirmed clean HEAD `0dfdd85` and resumed catalogue work without repeating DBCheck.
Traced manager getter `0xB3D3A0`, +0xDC candidate vector, +0xF0 saved vector, population `0xBB61C0`,
stars.db serializer load `0xBB8C90` and save `0xBB4D40`. Retained 15 functions and record-field
instructions under `analysis/audit/gge-catalog/`; save format remains unimplemented.

`gge_catalog.cpp` projects typed source metadata into owned entries, preserving saved/new placement,
independent source indices, duplicate star IDs, unchanged world positions, flag/stage defaults,
terrain-image load/missing distinctions and two-word save-header predicates. Unknown input lists do not
publish a fake empty-ready catalogue; invalid data leaves output unchanged. 4096 entry/text limits
are host safeguards, not original StarManager quotas. Verification uses 19 grouped controlled-input
checks; no live provider, generated starfield, save I/O or binary 0xBC-file decoder was added.
During final evidence review, the earlier tentative “current save key” interpretation was disproven:
0x145F844/48 are .rdata constants (0x25,3), not runtime identity. Corrected code/tests/docs to neutral
header-word comparison and retained probe 0xB27580; no Current/Other save-identity claim remains.
Final validation: warning-free Debug x86, catalog/DBCheck/Intro/binding/SPUI suites PASS,
544 package lifetime cycles and D3D9 scissor readback PASS; live Main -> GGE -> DevLauncher ->
CreatureEditor exits 0 with aborts/asserts 0. Dashboard 204 done / 35 partial / 53 todo (292 items);
no gameplay-done increment or new galaxy capture is claimed.

## 2026-10-15 — DBCheck branch executor and Play Spore state-2 dependencies

State2 constructor `0xE008B0` resolves to wide name Play Spore through `0xE00930`. Traced enter,
exit, update, game-list rebuild, per-star UI/effects. This is runtime game-record metadata, not a
static layout whose hidden panels should be forced visible. 14 focused function extracts/tables
and DB dialog corpus identities retained under `analysis/audit/gge-flow/`.

`gge_dbcheck.cpp` now implements GA branch/side-effect ordering via explicit backend ports.
Unknown is not original null-manager; bypass continuation and UI failures remain distinguished
from success. `0x60B470` first arms, later calls manager+20 then notification/cleanup; invocation
pump is still unresolved. Tests use real 101-object/24-window SPUI fixture/root and a controlled
side-effect recorder, including cancellation, lost manager/root, failures and Intro integration.
No live backend attached; GGE runtime remains honestly blocked rather than inventing readiness.
Final validation: warning-free Debug x86; DBCheck/Intro/binding/SPUI regressions PASS, 544 package
cycles and D3D9 scissor readback PASS; live Main -> GGE -> DevLauncher -> CreatureEditor exits 0
with aborts/asserts 0. Attachment/teardown callback safety is tested independently. Dashboard:
204 done / 34 partial / 53 todo across 291 tracked items; no gameplay-done item added.

## 2026-10-15 — Intro handler, MoviePlayer/DBCheck gate and explicit missing dependency

Shared GGE handler `0xDEA4B0` is named Intro. Traced enter/update/exit, normal title index-5 exit,
MoviePlayer global/vtable predicate and DBCheck notification paths. Retained 23 Ghidra function
extracts plus bounded assembly/tables under `analysis/audit/gge-intro/`.

`gge_intro.cpp` implements only the post-title phase6/idle predicate and one-shot request latches.
2040 controlled-signal tests cover missing/playing service, other phases, delayed and early events,
duplicate events and host reset. Unknown service is a host safety guard, not an original enum.
Runtime probes the post-title model with explicit Unknown movie observation: it does NOT claim the
title actually ran, does not emit DB completion and does not enter state10. Main's diagnostic GGE
frame now names the missing dependency; 60-frame live run exits 0 with no aborts/asserts.

Important correction: the original Intro hides GlobalUI and zoom UI. Our sparse primary frame is
a diagnostic view, not that visibility lifecycle. `0x08085C4C` can also be posted on absent-manager
or layout-failure branches, so it is a continuation notification, not proof that a DB load succeeded.

## 2026-10-15 — GGE driver trace and default primary UI subset

From registrar `0xDDDE30` / vtable `0x147D2B0`, followed setup/controller/activation/UI loading.
Nine uncarved vtable targets were explicitly seeded into persistent Ghidra. Retained 15 functions,
wide-name constructors and override instructions in `analysis/audit/galaxy-entry/`.
GA defaults are GlobalUIGGE-9, GGEZoomPanel and GGEZoomPanelV2; all resolve to actual SPUI records.

The host now presents only the confirmed **default primary GlobalUI** when the state script selects
mode GalaxyGameEntry. Main -> GGE gets a real-art sparse diagnostic frame instead of a generic
renderer-less placeholder. Only 9 serialized windows visible; hidden panels were not forced on.
Galaxy scene, zoom composition, runtime property overrides, controller lifecycle and input behavior
remain unimplemented, explicitly labelled on screen. Tests cover mode/name/hash boundaries plus
the existing parser/drawable/scissor suite; live Main -> GGE capture exits cleanly.

Also corrected the runtime mode report: missing ASCII/UTF-16 names are bounded search negatives,
not proof of absent code. The launcher ControlID/state shortcut is now explicitly scoped to
DevLauncher/E3LaunchScreen, not silently used as GGE command dispatch.

## 2026-10-15 — atlas-aware launcher and documentation/dashboard refresh

**Continuation after docs:** shared ancestor Clip now drives picking and per-window D3D9 scissor.
The shipped schema's Clip mask is `0x400` and limits children, not the parent itself. Geometry tests
cover nested intersections, own-vs-ancestor flags, empty bounds, half-open edges and invalid cycles.
`overlay_clip_verify` draws through the real D3D9 overlay and verifies captured pixels for a flat quad,
an image quad, queued text before/under a clip, empty/NaN rejection and reset. PASS, warning-free build;
launcher capture and visible CreatureEditor regression remain healthy. This is host-policy consistency,
not a newly proven original WinProc/scissor pipeline. Dashboard updated to 204 done / 31 partial /
53 todo out of the same 288 tracked items.

GA Image UV/logical dimensions, ImageDrawable fixed-axis scale/alignment and Standard/Radio
four/six-frame selection now produce the real-art launcher capture
[spui_e3launch_uv.png](analysis/shots/spui_e3launch_uv.png). Evidence retained under
`analysis/audit/spui-drawables/`; implementation `source/engine/spui_drawable.cpp`.
The old "DialogDrawable wrapper" was a property alias, not an actual intermediate class.

The repeated missing PNGs exposed a host FILE leak, not missing data: `Package` now closes on
destruction. Regression: 544 scoped open/read/destruct cycles at CRT limit 512.
Parser verification remains 748 matches / 486 malformed rejections, plus atlas/state/scale/alignment
checks. Live click `(311,550)` still enters CreatureEditor; the editor itself remains unimplemented.

Documentation refresh makes counts explicit, splits UI navigation from gameplay, removes the claim
that the function-start catalogue measures reversed-code coverage, and preserves old root snapshots
in `docs/archive/`. Dashboard source remains `analysis/features.json`; the new generator emits PNG,
Markdown and stats JSON, validates data and supports a no-write freshness check. Counts are not
directly comparable to old percentages: scope/grouping and the tracked task list changed.

---

## 2026-10-15 — actual SPUI binary reader; 748/748 exact parses and child graphs

Found GA **0x82ce10** by file magic, then read **0x82b3f0** (properties), **0x82b240** (skip/widths),
**0x13005a0** (Children runtime descriptor). Saved instructions/pseudocode with target SHA-256 in
`analysis/audit/spui-binary/`. Old XML reader was a different branch. The vocabulary-wall and inline-
child explanations are superseded by `analysis/spui-binary.md`.

Code 19 is count u16 object indexes; code 20 contains nested structs; 0x5FF5 marks object blocks.
Replaced anchor scans with bounded sequential C++, preserving object/class/property offsets and
unknown ids. Resolve Children by object identity; reject bad references, multiple parents and cycles.
Parent graph/visibility replace size-based paint/pick heuristics.

**Correction:** GGE's parent is hidden; the old flat hit-test activated an invisible widget. Current
windowed test uses visible `(311,550)` -> **CreatureEditor `0x022D9E01`**, exit 0. This is our launcher
bridge; original message dispatch, layout WinProcs, Z order and drawables remain incomplete.

Validation: Python/C++ agree on **748/748** full-byte parses: **93,726 objects, 22,384 windows, 18,232
Areas, 1,183,787 properties, 20,659 links**. C++ verifier rejects **486** malformed inputs atomically
and accepts unknown property ids. Debug x86 build 0 warnings; 57/57 packages / 150,945 resources;
captions 1,384/1,426; boot 10/10. Screenshot is a geometry/text diagnostic, not completed gameplay.

## Session 13 — 2026-10-13: the engine draws real Spore geometry; a first-argument bug fixed; a progress map

### 2026-10-14, the last stretch: THE MENU IS FUNCTIONAL — a click enters a game state
Six commits (`fba967b`→`e55fc41`). The path's menu step went from "we can draw a screen" to "the screen
does something", with every link in the chain justified by data rather than by choice.

1. **`uilayout -load` is EXECUTED** — `fnv1_lower("E3LaunchScreen") == 0xE932BD44` is a real `spui`
   instance, so a state's own words select its screen; `-state:DevLauncher` vs `-state:Title` differ in
   **65.9%** of sampled pixels, i.e. a different SCREEN, not a relabel. (`1995cca`)
2. **Modes answered**: code, not data — 11 of 17 names are standalone strings in the oracle, each
   registered once by a per-class factory; **5 exist nowhere in the 24.9 MB binary**. (`8710631`,
   `analysis/modes.md`)
3. **A pointer resolves to exactly one widget**, same path for a real mouse and for `spui:-click`.
   (`bfb7f4a`)
4. **A widget's control id IS a state id**: 9 real hits of 4,187 control ids × 60 states, **<0.001
   expected**, and the labels spell it out — `0x02CCDA9C` captions **"GGE"**, `0x022D9E01…10` caption
   "Creature/Building/Vehicle/Flora/Terrain Editor". Verified live:
   `click (942,261) -> "GGE" => ENTER STATE 'GalaxyGameEntry'` → `[state] -> GalaxyGameEntry
   [0x02ccda9c]`, the script's own `-id`. (`4cf3825`)

**Two bugs that looked like working code, both found by running the consequence rather than the
intention.** (a) `request_goto()` from outside a transition logged `chain: queued gotoState 'X'` and
never executed — fixed by `Manager::drain_pending()` per frame. (b) The presenter's 260-window cap
BINDS (largest layout has 570), so it drew a subset the hit-test could still select. And a measurement
correction: **vector order is not z-order** — E3LaunchScreen's full-screen root is the LAST record, so
"last drawn is topmost" selected the widget behind everything; fixed as ONE policy (paint
largest-first, hit the smallest containing), with the real parent/child hierarchy named as the single
place to replace it.

Verification: 0 build warnings, yardstick 10/10, exit 0, 57/57 packages, 150,945 resources, 748 layout
resources = 524 distinct / 22,384 windows / 18,232 Areas / 961,360 anchors, locale 265 tables / 23,933
texts / 14,657 keys, captions 1,384 of 1,426, unpack `,locale` 2,944/0 failed, Main/GalaxyGameEntry/
CreatureGame each still capture a frame. Map **82.2% → 82.8%** engine, **75.7% → 76.2%** overall
(273 features, 194 done), and **gameplay is no longer all-todo**.

### 2026-10-14, later: THE STATE MACHINE DRIVES THE UI, and a 64→32 narrowing that had been lying to us

Four commits, all measured. In order of what they cost:

**1. `get(table,key)` was silently ignoring the table** (`fba967b`). `m_by_table` was
`std::map<u32,std::string>` while every access packed `((u64)table << 32) | key`; the narrowing was
implicit, compiled at 0 warnings, and reduced the per-table lookup to a global one — the exact failure
the ambiguous-key refusal exists to prevent, serving other tables' strings to our menu. **The tell was
already printed in a log line I wrote that morning**: "14,657 distinct (table,key) texts" *equal to*
"14,657 distinct keys". Caption resolution went 1,391 → **1,384 of 1,426**: the number fell because it
became true. Verification: the map is now `u64`, the key is spelled by one `pack()`, and the corpus is
reported as distinct pairs, entries, and duplicate-agree/disagree counts (32,038 → 23,933; 8,102 agree,
3 disagree).

**2. `Data/PatchData.package` carries its own nested locale packages** (`fba967b`, `daad387`). The
fourth "the data does not exist" of the session, in a new shape: **wrong scoping of the search**. The
engine had parsed them all along (its nested pass scans by TYPE); my tools filtered on the filename
`*Spore_EP1_Locale*`, so `legal` `0xADB810C4` stayed "missing" for two sessions after it had been read
off disk every boot. `tools/nested_every.py` enumerates by type across all 57 containers: 44 nested
`package` resources, 20 instances shipped in more than one container, en-us in three (PatchData 42
tables, EP1_01 and EP1_02 byte-identical at 65). Verification: `spui -gaps` now splits the 42
unresolved refs into COVERAGE (`mvj`, 14 keys, absent everywhere) and SEMANTIC (4 keys in tables we
do have) — and `legal` is gone from the missing list.

**3. Precedence measured instead of assumed** (`fba967b`, `1995cca`). Both locale and spui had been
letting traversal order decide. Locale: 3 of 23,933 strings disagree, and EP1 winning is provably
correct (it fixes PatchData's "occured"). spui: **748 resources are only 524 distinct instances** —
224 layouts ship twice, 18 with a different length — so `find()` now indexes by instance with an
explicit first-container-wins rule (PatchData overrides base content), and the load line reports
resources, distinct ids, shadows, and differing shadows. The two subsystems' rules still disagree;
that is recorded as an open question with its size (18 layouts, 3 strings) rather than papered over,
because "earliest wins" cannot be the original's rule given the locale evidence.

**4. `uilayout -load` is EXECUTED, not reported unrunnable** (`1995cca`). The recovered 60-state script
contains exactly one UI verb, in `state DevLauncher`:
`cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"`. The name is the pointer —
`fnv1_lower("E3LaunchScreen") == 0xE932BD44`, a real `spui` record — so `spui::Reader::find_by_name()`
resolves it and the new `source/states/uipresent.{h,cpp}` presents that screen. Title lost its private
copy of the drawing code; both go through one path now.

* Verification, live: `[ui] screen: state 'DevLauncher' asked for 'E3LaunchScreen' -> PatchData.package
  rec[1053] inst=0xE932BD44 24814B 68 windows`, preceded by `DevLauncher: 1 \`uilayout -load\` EXECUTED
  by the UI layer`. Screenshot `analysis/shots/spui_e3launch.png`.
* Verification that this is DRIVING and not relabelling: `-state:DevLauncher` (the game's choice) vs
  `-state:Title` (our largest-layout choice, `Spore_EP1_Data rec[4870]`, 570 windows) differ in
  **67,532 of 102,480 sampled pixels = 65.9%**. A different screen, chosen by the data.
* Verification that nothing else broke: 0 build warnings, yardstick 10/10, exit 0, 57/57 packages
  (0 unknown/invalid), 150,945 resources, 748 layouts / 22,384 windows / 18,232 Areas / 961,360
  anchors, locale 265 tables / 23,933 texts / 14,657 keys, unpack `,locale` → 2,944 written / 0 failed,
  `txt:0x8B1A2341:0x077427D2` → "Accept Reward", and Main / CreatureGame / TribalAccessoriesEditor each
  still capture a frame. Feature map 82.2% → **82.4%** engine, 75.7% → **76.0%** overall.
* **Unresolved, stated:** the two trailing ids are not resource ids (absent from all three index fields
  of all 150,945 records) and no registry name hashes to them; the first appears twice in the spui byte
  corpus, the second nowhere. `find_by_name()` does not need them.
* **A side discovery worth its own line:** one name can carry several resource types. `E3LaunchScreen`
  is a `spui` AND a `prop` (`0x00B1B104`); `SpaceGame`/`CreatureGame`/`TribeGame` name a `png`
  (`0x2F7D0004`) as well as their locale table. `tools/id_role.py` answers "what IS this id" by scanning
  all three index fields — role, not search.
* **Testing traps, each paid for:** `pkg:-unpack,all,<dir>` with no type filter is a 10 GB job (timed
  out at 15 min) — the 2,944 yardstick is the `,locale` form; exit code 4 is `render::device().init()`
  refusing under back-to-back unattended launches, correctly handled, not a bug; a grep for
  `ERROR|ASSERT|abort` matches the clean-shutdown line `aborts=0`; and `Get-Content | %{ $_.Line }`
  prints nothing, because plain strings have no `.Line`.

### THE EP1 TEXT IS A DBPF *INSIDE* A DBPF: the layout->text chain closes at 97.5%
Following the caption references to their end produced the session's third "the data does not exist"
correction, and a new shape of the same error: not a wrong directory, not a wrong id field, but a wrong
**CONTAINER DEPTH**.
`tools/spui_caption_ref.py` first established the record shape — code 18 splits on a leading u16 of
`0xFFFF` into 4,207 inline UTF-16 strings and 3,107 references, and a reference is
`0xFFFF | table:u32 | key:u32`, with **1,097 of 1,426** carrying a key that is a real key of the
referenced table. The record is exactly 10 bytes: the next property id follows immediately
(`06 b0 c1 ee` = BackgroundColor), which is what confirms the framing rather than guessing it.
`tools/spui_caption_gaps.py` then classified the 298 that did NOT resolve: **285 (96%) were tables
absent from our tree entirely**, and the registry named them — `EP1_Headers`, `GGETextEP1`,
`EP1_Buttons`, `EP1_Tooltips`, `ScenarioBuildMode`, `legal`, `mvj`. I hypothesised they were in
`DataEP1/Spore_EP1_Locale_*.package`; `tools/find_ep1_locale.py` showed those packages contain **zero**
`locale` resources, which killed that hypothesis but exposed the real answer in the same output: those
packages hold 17 + 4 resources of type **`package` (0x06EFC6AA)**, and `tools/nested_locale.py` proved
each one **is itself a whole DBPF**, together carrying **1,364 `locale` resources**.
The user's memory was right ("вложенные пакеты либо по папкам"), and the wrong half of my own reasoning
was assuming a package index is flat.
`tools/nested_langs.py` settled which nested container is ours: instance **0x7080DB55 ==
fnv1_lower("en-us")** — a nested package is named by the SAME hash rule that names a locale table, so
selecting a language is a lookup, not a guess. Verified from both ends:
`fnv1_lower("EP1_Buttons") == 0x8B1A2341`, the very table the layouts reference.
Engine work: `pkg::Package::open_from_memory()` (a second backing store behind one `read_at()`, with
BOTH open paths running the oracle's validator clauses unchanged), and `text::Tables::parse_package()`
extracted so the flat and nested passes share ONE copy of the grammar — a second copy is exactly how
two views of one dataset start disagreeing.
Measured before/after: locale tables **197 -> 369**, entries **18,441 -> 37,608**, distinct keys
**10,246 -> 14,657**, and layout caption references that resolve **1,139/1,426 (79.8%) ->
1,391/1,426 (97.5%)**. Live in the engine:
`txt:0x8B1A2341:0x077427D2` -> **"Accept Reward"**, `txt:0xAD7B2086:0x0589E51B` -> **"640x480"**,
`txt:0xC4F86AE0:0x07736311` -> **"Mission Complete!"**. 35 references still do not resolve (`mvj`
0x277DEE6A and `legal` 0xADB810C4 are absent from our 57 packages even nested) — recorded, not hidden.
Two process notes worth keeping:
* The refactor first left `read_resource()` on `fseek(m_file, ...)`, so a nested package (m_file ==
  nullptr) hit the UCRT `stream.valid()` assertion and died. **Loud was the right outcome** — had it
  silently returned nothing, the EP1 tables would have looked absent for a fourth reason.
* I re-entered the **double-visit trap** twice in one session (`spui` reported 1,349 layouts where the
  corpus has 748; `appstates` listed 5 copies of a script that exists 3 times), even though the `pkg`
  verb has carried the `weakly_canonical` dedupe since an earlier session and MEMORY.md documents it.
  Both now dedupe. A documented trap does not protect you unless you look it up before writing the loop.
Also added `txt <table>:<key>` (colon form) because our CLI's native separator is `:` and a
space-separated pair cannot survive `-exec` through a shell that eats spaces.

### WE DRAW A REAL SPORE UI SCREEN NOW: 9,547 windows at the game's own coordinates, with its own captions
`source/engine/spui.{h,cpp}` + the generated `source/engine/spui_schema.inc`
(`tools/gen_spui_schema.py`, the same pattern as `typeids.inc`: the registry is DATA, so the table is
generated from the shipped files and never hand-typed). `title_render()` draws it.
Measured: **400 layouts, 11,465 windows, 9,547 with a usable Area, 472,291 records read, 306 schema
ids**, and the captions come out of the layout itself: **"Create a New Game"**, **"color picker"**,
**"Selected Color"**, "Pick A Star For Your New Game" with slots "1".."5" beside it — over the real UI
atlas (`analysis/shots/spui_real.png`). This is the goal the user set for the Loader ("чтобы Лоадер
умел грузить меню"), reached from the game's data rather than from invented chrome.
Two facts the render itself corrected:
* **`Area` is LEFT, TOP, RIGHT, BOTTOM, not x/y/w/h.** `(168,-10,192,14)` is a 24x24 icon and
  `(0,3,181,5)` a 181x2 bar; an x/y/w/h reading cannot produce either. The confirmation is
  quantitative, not aesthetic: switching to LTRB raised the usable-area count from 7,838 to **9,547**,
  because x/y/w/h rejects every rect whose "h" is really a bottom coordinate. Negative left/top are
  legitimate (off-screen anchors for slide-in animation), so a validity test that discards them is
  wrong twice over.
* **The alphas in a layout are meaningful — do not force them opaque.** The first render ORed
  `0xFF000000` into `BackgroundColor` and the screen became opaque grey slabs burying the art: the
  shipped values are `0x007F7F7F` (fully transparent grey) and `0xFFFFFFFF` (opaque white). Use the
  ARGB as stored and outline the box, so a transparent window still reads as the rect the designer placed.
The reader is **anchor-driven, not sequential**, deliberately: a sequential walk derails in 637/748
files on ids the designer XML omits, but every anchor whose id IS known can be read exactly without
interpreting the bytes between two anchors. **Unfinished vocabulary must not block finished structure.**

### THE SPUI DOOR: the format's dictionary was shipped in the game (`spui` is a RECORD stream, not a token stream)
**Heading corrected in-pass: this session first concluded `spui` was a bit-packed TOKEN STREAM, and that
was wrong about the mechanism.** It is byte-packed RECORDS. See the RETRACTION below.
Found by following the DATA rather than the bytes: the recovered state script names the command —
`cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"`. Grepping `.rdata` for the **verb**
`uilayout` produced the whole class cluster in one step, after grepping for the *format* had produced
nothing for sessions.
Chain: factory **`0x00810870`** (gated on request tag `0x0510A95B`, alloc 0x24 under the tag
`"App/SPUILayoutResource"`) → loader **`0x00810940`**, which does
`cmp dword [eax+4], 0x250FE9A2` — **our `spui` type id confirmed in CODE**, not inferred from
`reg_type.txt` + the package index as before — and builds the triple
`UI/UILayoutBinary/SharedPointer` (0x18) + `UI/UILayoutBinary/MemoryStream` (0x24) +
`UI/cSPUILayoutResource/TokenIOContext` (0x38), all three class names present as literal strings →
reader ctor **`0x00903330`**, which installs read/write callbacks `0x925e80`/`0x925eb0` (thin thunks
onto the MemoryStream vtable +0x08/+0x0c) and a **2048-byte** buffer (`mov [edi],0x800`) → the family
is UTFKernel's **`XML/TokenReader`** (`0x0143a450`, used at `0x00903100`, which calls `0x901680`
read-token / `0x9016e0` peek / `0x900e10` error and tests a token flag with `and eax,0x8`).
**Consequence: the 273-byte `spui` sample never aligned to floats or u32s, and `b0 c1 ee 09` recurred
like noise, because those bytes are a token stream (the binary counterpart of an XML layout), not a
struct with unknown field order. There is no stride to find.** So: stop parsing bytes; recover the
token alphabet from those three functions first, then decode one small `spui` and check it against the
407 named screens in `analysis/spui_screens.txt`. The next step is now narrow instead of open-ended.
`analysis/spui-loading.md`, VERIFY rows 46-47, `analysis/claims.json` (`spui.loading_path`, 15 checks).
**TRAP discovered on the way (row 47): a `.rdata` pointer is not always a vtable.** `0x903337` stores
`0x0143a460` into `[esi]` exactly where a ctor stores a vtable — but those bytes are the ASCII
`"UTF/XmlTokenReader/TokenIOContext/mAllocator"`. Read as a vtable it would have invented a fake
12-method class; read as a string it named the reader family for us. The same shape made
`0x013ebcbc`/`0x01418398` in the factory look like vtables (they are not). **Try ASCII before treating
a stored pointer as a vtable.**

### The audit, finished: 29 PASS / 0 FAIL / 3 UNKNOWN / 1 SKIP, and the wall MEASURED
Trajectory: 13 PASS / 10 FAIL (first run) → 17 / 1 (after fixing my checker) → **29 / 0** after seeding
the 7 vtable-only functions and re-analyzing. Seeding those seven entries cascaded to **+683 carved
functions** (54,611 → 54,618 → 55,294): that number IS the vtable-only wall, measured rather than
asserted. **Not one boot-chain reading was wrong.**
Two more checker artifacts, both the same class as the first nine:
(1) **Ghidra prints conditionals by SEMANTIC name, objdump by the historical ALIAS** — our docs say
`je`, Ghidra says `JZ`; a `/je/` regex FAILed against the identical instruction. The auditor now
matches both spellings (`normalize_insns`).
(2) **A function body is not always contiguous, so an ABSENCE is only evidence when the tool actually
examined that ground.** The event dispatcher's body came back as
`[[0x7e7160,0x7e71c4] [0x7e72bf,0x7e7300]]` — a **250-byte hole** Ghidra never disassembled because it
stops at the MSVC padding `8d 49 00` and types those bytes as data — and our documented retry values
(`mov [esp+0x30],0xa` at `0x7e7233`, `mov [esp+0x14],0xfa` at `0x7e7264`) live inside it. So
"immediate nowhere in the function" described Ghidra's coverage, not the binary. The export now carries
`body_ranges`/`body_contiguous`, and the auditor downgrades such a miss to UNKNOWN **naming the gap**
instead of claiming a refutation; `tools/gen_seeds.py` derives `code` seeds for every gap (7 found: the
dispatcher, the RefPack decoder, the mouse pump, `__tmainCRTStartup`, a model-manager handler).
Three walls in the filler were mine, all the same shape — a plausible API that does not exist:
`clearCodeUnits` is on neither `FlatProgramAPI` nor `ProgramDB` (it is
`ghidra.app.cmd.disassemble.ClearCodeUnitsCmd`); stopping the walk at any defined datum can never fill
anything, because **every hole BEGINS at exactly such a mis-typed datum** (the datum is the cause, not
a boundary); and `getFunctionContaining()` reports the enclosing function for an address inside the
hole, so it is useless as a stop test — the next DECODED INSTRUCTION is the real edge.
And a fourth, subtler one, caught only by re-deriving the gaps from CURRENT facts instead of replaying
a saved file: the seed walk reported `0x7e71c5: reached a decoded instruction` and I first read that as
"the re-analysis closed the hole by itself". It did not. `0x7e71c5` holds an instruction that is
**outside** `0x7e7160`'s body ranges, i.e. Ghidra carved that middle region as a SEPARATE function (or
left it unaffiliated). So the dispatcher's bytes ARE disassembled — they just do not belong to the
function my claims cite, which is why `uses_imm` on `0x7e7160` cannot see them. The fix is therefore
**not** seeding at all: it is that the audit must cite both halves (or the gap owner) before calling an
absence a refutation. Re-deriving gaps from live facts is what exposed this; a saved seed file would
have had me seeding holes that were never holes.
Doc correction the audit forced while it was at it: the retry wait helper `0x922080` is
**`SleepEx(ms, bAlertable=1)`** via IAT `0x13cc17c`, not `Sleep` (resolved through the PE import
directory), and the branch re-dispatches event id `0xf62ade`. `startup.md`'s "Sleep(0xfa)" was wrong in
detail and right in substance (10 retries × 250 ms).
Process lesson of the hour: I ran three of these pipelines **in the background simultaneously** and
they fought over the Ghidra project lock, each reporting `LockException` while a *sibling* held it — and
one of them I had already misdiagnosed as a stale lock and cleared. **Serialize the jobs that share one
project**, and wait for the owner to exit (identify by command line, not by process name) instead of
killing it.

### The `-id` semantics, settled: an explicit id REPLACES the name hash, so names resolve as STRINGS
The state-add `0x7e3fb0` branches on whether the ArgScript arg is present:
`7e40c5 push 1/"id"(0x013fc8f0)/call 0x838410` → `7e40d6 je 0x7e40ea` (ABSENT → hash the name) vs
`7e40e8 jmp 0x7e4100` (PRESENT → skip the hash entirely), and **both** converge on
`7e4103 mov [ecx+0x28], eax`, then insert into the `[state+0xb8]` map keyed by that id. So `-id` is not
a parallel field: 19 of the 60 states are addressed by their assigned number and 41 by
`fnv1_lower(name)`.
Our `Manager::find()` had been resolving a name by HASHING it, which made every explicit-id state
unreachable by name — including `Main` (`-id 0`) and all 13 editors — and that contradicted the shipped
script's own header ("you can run with `-state:<stateName>`"). `find()` now compares names as strings
with the engine's own tolower folding, and `find_by_id()` was added for the id namespace the UI layouts
and editor messages actually use. Verified: `-state:Main` → `Main [0x00000000]` → `gotoState
GalaxyGameEntry` → `GalaxyGameEntry [0x02ccda9c]`; `-state:tribalaccessorieseditor` (all lowercase) →
`TribalAccessoriesEditor [0x022d9e06]`.
Sub-trap: the arg is literally `"id"`, **not** `"-id"` — a search for the ASCII `-id` finds only
`inter-ideograph`, the *effect* command's `-id <n:int>` spec at `0x01456d10`, and TLS cipher names. The
leading dash is the ArgScript layer's convention, which is why the first attempt to confirm this by
string search appeared to fail.
Transitions are a **CHAIN, not a stack**: `endChain` (0x014130f8) sits in the same .rdata cluster as
`entry`/`exit`/`onExit`, and `Main`'s body is a bare `gotoState` with no entry block, so a transition
fires while ENTERING a state — but the state-add asserts `can't nest states or transitions`
(0x014135dc at 0x7e3fd7), so a chained `gotoState` must not re-enter the switch. That is
`Manager::request_goto()`: it queues, `goto_name()` drains after the transition, with a 32-hop cap so a
script cycle logs instead of hanging the boot. Measured: the shipped table has no cycle.
Cross-system confirmation, the strongest single measurement in the project: `fnv1_lower(state_name)`
equals a real en-us `locale` **table instance** for exactly four states — `GalaxyGameEntry 0x2B7A0311`,
`SpaceGame 0x2DB6DAD3`, `CreatureGame 0xAD56080C`, `TribeGame 0xF71FA311`. Control: 193 of 125,272
registry names hash to any of the 200 table instances, so the expected accidental rate over 60 states is
~0.09 hits, not 4. One measurement corroborates five independent things at once: the FNV-1
multiply-first direction, the tolower folding, the instance-id naming rule, the recovered state names,
and the locale table indexing.

### A state with no renderer must still present a FRAME
Booting into any game state drew nothing: those states' behaviour is an unimplemented `mode`, so no
`on_render` ran, so the device never presented, so the deferred `-capture` was never serviced — and the
frame loop exited 0 having produced zero pixels. The original's mode decides a state's CONTENT, not
whether the pump runs, so `app.cpp` now draws an honest placeholder for a renderer-less state: the real
name, the real id with whether it was assigned or hashed, the description, the unexecuted mode, the
`gotoState` edges and the entry actions verbatim (`analysis/shots/state_main.png`). Fixing this also
unblocked `-capture` for every future state, which is the only way we get machine evidence of a frame.

### The audit's FIRST run: did we start reversing correctly? YES (17 PASS / 1 FAIL / 11 UNKNOWN / 1 SKIP)
User request: "проверить вообще всё (начиная с мейна), чтобы понимать, всё ли правильно начали
реверсить". Made checkable rather than eyeballed: **`analysis/claims.json`** = 30 claims / 104 checks,
every address assertion from `analysis/startup.md`, `VERIFY.md` 35–41, `MEMORY.md` and
`source/states/statemgr.h`, each citing its doc line + severity + whether our C++ is load-bearing on it.
Bridge: `tools/ghidra_audit.py` (request/verify) + `tools/ghidra_export.py` (PyGhidra driver, because
**Ghidra 12 removed embedded Jython** — `-postScript *.py` fails outright) + `tools/gen_seeds.py` +
`tools/ghidra_seed.py`. Ghidra 12.1.4 analyzed the oracle in 1,197 s and carved 54,611 functions.
Verdict: **PASS 17, FAIL 1, UNKNOWN 11, SKIP 1**. The whole load-bearing chain PASSED independently:
entry → CRT → `__tmainCRTStartup` → WinMain `0xf489e0` → app_main `0xf48810` → run `0xf47e90` → AppMode
`0x7e8a00` → main loop `0x8482f0`, including both self-written TRAP notes in NEGATIVE form (the W-vs-A
`PeekMessage` split; `screenWindowed` being a RenderWare metric).
**The first run reported FAIL 10, and NINE were bugs in MY CHECKER, not in our reversal.** Reading the
evidence instead of acting on the verdict is what caught that. `analysis/audit-review.md`.

### The one real doc error: the main-loop call site is 0xf4840b, not 0xf48405
`startup.md` cited `0xf48405` as the call into the loop. It only LOADS the vtable slot; the call is the
next instruction: `f48405 mov eax,[edx+0x84]` / `f4840b call eax`. That split is the shape of EVERY
virtual dispatch in this binary, and it is precisely why the main loop was hard to find — a text search
for `call [reg+0x84]` matches nothing. Pinned by two `insn_at` checks so it cannot regress. Also
corrected: window vtable `0x141cc10` **slot 0 is the deleting destructor `0x849e80`** (the ctor
`0x849b20` is real but is not a vtable slot — my category error); `.data 0x153f858..` is a table of
**pointers** into `.rdata`; the FNV seed `0x811c9dc5` is a **parameter pushed by the caller** at
`0x7e40f2` (the hasher holds only the prime); `EventLoadScreenBufferPercentage` is at `0x0148227c`
(`0x01550498` is `EAReportUploader` — kept as a negative control); `UIWorld_Loading` is **UTF-16LE** at
`0x01418274`.

### THE STRUCTURAL FINDING: Ghidra cannot see vtable-only functions (this reframes rw4/spui/prop)
Seven cited addresses had **no function at all** in Ghidra's inventory: `0x7e3fb0` (statemgr add-state),
`0x7e7160` (event dispatcher), `0x7eaae0` (AppMode +0x20), `0x849bf0` (Window::Create), `0x7c7f50` /
`0x7c90f0` (config executors), `0x7e40f2` (mid-function). `tools/gen_seeds.py` walked each back to the
preceding `int3` gap and proved **six are real entries** — never reached because they are callable ONLY
through a vtable, and six of seven open with the MSVC SEH prologue `mov eax,fs:0x0`, which Ghidra left
typed as data (`0x7e7160` became the datum `?? 64h`) while the listing shows all three of our documented
hashed event ids in plain sight (`cmp eax,0x255abf5` / `0xf62add` / `0x212d3e7`). The recorded rw4
finding was **confirmed** by the audit (`direct_callers == 0` PASSED outright); the deeper reason we
kept failing is now named, and the fix is seeding in both toolchains
(`analysis/audit/seeds.tsv` = 21 candidates; `analysis/audit/seeds_rea.tsv` = the 7 uncarved ones only,
because a seed overlapping an already-decoded instruction fails).

### Three invisible-API artifacts, i.e. how a checker manufactures confident wrong answers
(1) **`Memory.getInt(addr, TRUE)` is BIG-ENDIAN**, not "ignore alignment" — it reversed every vtable
word and produced six bogus FAILs (`0x007e6850` came back as `0x50687e00`, the same four bytes
permuted). (2) **`JArray("b")` raises `ClassNotFoundException('b')`** — the element type must be the
`JByte` CLASS; the swallowed exception made four perfectly good strings report "no readable string".
(3) **regexes written in objdump's spelling cannot match Ghidra's printer** (`MOV EAX,dword ptr
[EDX + 0x84]`). Also hit: `ProgramDB` has no `getEntrypoint()`; `Listing`/`FunctionManager`/
`ReferenceManager` are instance methods on `Program`, not statics; `GhidraFile` has no `getPath()`;
`DecompInterface` has no `setAutoUpdate()`; `DataDB` has no `isString()` (so every string was silently
dropped); `GhidraProgramUtilities` has no `analyzeProgram()`; `FlatProgramAPI.createFunction` takes only
`(Address, String)`; `GhidraFile.save` only `(TaskMonitor)`; and **`analyzeAll` needs an enclosing
transaction** or every analyzer worker dies with `db.NoTransactionException`.

### The state machine was DATA ALL ALONG — I had it wrong for three sessions
`RESUME.md`/`MEMORY.md` recorded the game's state names as "data we do not have", because `SporeAppStates`
(`0xD6396913`) appeared nowhere as a resource GROUP. **It is an INSTANCE id in group 0** — I had probed
one of a DBPF record's three id fields. The fix was not a cleverer guess but enumeration: dump all 303
`trigger` resources with their group AND instance names. That immediately surfaced `SporeAppStates`,
`CSAAppStates`, `Startup`, `UIMachineScenarioMode`, `TriggerConfigScenarioMode`, `OptionProperties`,
`ConfigManager` — several of which we had also written off.
`analysis/scripts/SporeAppStates.txt` = 8,499 decoded bytes of plain ArgScript, extracted verbatim
(also `Startup.txt` 15,269 B, `UIMachineScenarioMode.txt`, `TriggerConfigScenarioMode.txt`). Recovered:
**60 states, 17 modes**. Entry `state Main -id 0` description "Spore Launch Screen" → `gotoState
GalaxyGameEntry` (`-id 0x02ccda9c`); `-demo` enters `Demo`, `-preload` enters `Preload`. The five stages
Cell → Creature → Tribe → Civ → Space are explicit data, each with `FromLaunchScreen`/`FromLoadGame`/
`FromLevelEditor` entry and inter-stage messages (`kMessageCellToCreatureTransition 0x02A4f8f0`). 13
editors share `mode Editors` and switch by `kMessageSwitchEditor 0x022d308b` + `(hash(<Setup>))`. `-id`
is EXPLICITLY assigned for UI-addressed states (the script's own comment: "assign manual IDs just so I
can fill in the UI layout stuff more easily") — same rule as `Properties.txt`: take the id from the
file, never hash it. The user's `-state:SpaceAccessoriesEditor` is confirmed by the shipped header
**INCLUDING THE COLON**; the exact state is `TribalAccessoriesEditor -id 0x022d9e06`, and
`SpaceAccessoriesEditorSetup` is the setup RESOURCE. `analysis/states.md`.
It also names the door into `spui`: `cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"` — the
SPUI entry point is the **`uilayout`** verb with `-load <name> <id1> <id2>`.

### TRAP: scoop's `current` is a JUNCTION and REA refuses it (three imports lost)
`C:\Users\DartPower\scoop\apps\ghidra\current` is `<JUNCTION>` (`ReparsePoint`). REA's Windows native
admission opens every path component **without following a reparse point** and rejects the request:
`Reparse path component is unsupported (tag 2684354563) … Ghidra target snapshot failed admission`.
`GHIDRA_INSTALL_DIR` must be the real versioned dir `…\apps\ghidra\12.1.4-20260921`. The trap is that
**`rea doctor --provider ghidra` reports `healthy: true` with the junction path** — doctor validates the
installation, while path admission happens later, at operation time. Also required for this binary:
`REA_GHIDRA_STARTUP_TIMEOUT_MS` far above the 330,000 ms default (our import took 1,197 s) and
`GHIDRA_HEADLESS_MAXMEM` above the 2G default; the MCP client's per-call timeout is a SEPARATE deadline.

### Two process mistakes of mine, recorded because they are the transferable part
(1) I **killed REA's working JVM**. A `java` process was alive while our own seed run had crashed; I
assumed it was a leftover holding the project lock and stopped it — it was the REA import, 15 minutes
in. **Identify a process by its command line before killing it** (`Get-CimInstance Win32_Process
-Filter "Name='java.exe'"`, match the project path), not by its name. (2) I **hand-rolled what REA
already provides**: I wrote `tools/ghidra_seed.py` against the raw PyGhidra API and hit four signature
walls, when REA exposes the identical capability as the documented `REA_GHIDRA_SEED_FILE`. **Check
whether the tool has a first-class option before implementing the feature underneath it.**

### Toolchain: REA is PRIMARY, our PyGhidra bridge is the persistent-project fallback
REA 6.4.0 installed globally and registered as an MCP server for OpenCode
(`C:\Users\DartPower\.config\opencode\opencode.json`, with a `.rea.backup` snapshot setup took first);
`rea doctor --provider ghidra --json` → healthy (Ghidra 12.1.4, native `decompile.exe` win_x86_64, JDK
21, Windows native authority). Primary because its evidence contract is stricter than what I would
write: `facet_availability` per facet, and the rule that **"an empty call list with partial or unknown
coverage is not proof that the function has no calls"** — exactly the shape of our
`direct_callers == 0` claim. The bridge stays for two measured reasons: REA uses an ephemeral
`-readOnly -deleteProject` session per run and **cannot accumulate** seeds/annotations (~20 min
re-import each time) while our project retains them; and 104 checks in one batch pass is an audit, not
an investigation. `analysis/toolchain-rea.md`.

### Localized text: the game's own strings reach our Title screen
`source/engine/text.cpp` parses the `locale` resources (type `0x02FAC0B6`). Format measured on the
shipped bytes: UTF-8 **with BOM**, records separated by **CRLF**, grammar `0x<8hex> [~markup~] <text>`.
en-us: 200 resources → **197 parse, 18,441 entries, 10,246 distinct keys, 684 ambiguous, 5,434
comment lines, 17 continuations, 0 unterminated markup tokens**.
The load-bearing finding is that **the key space is per table**: 0 of 200 tables repeat a key
internally, but across tables the low band collides (36.2% ambiguous below `0x1000`, 0.5–0.7% above
`0x10000`). So `get()` REFUSES an ambiguous key rather than returning whichever table was parsed
first; `txt 0x00000011` prints the collision set and asks for a table instance. UI labels live in the
high band, which is why the menu works at all. `analysis/text-locale.md`.
The Title screen now resolves its five rows by LABEL (`find_label`) and prints label + key on screen:
`New Game 0x05B08767`, `Load 0x0045B9EB`, `Options 0x00010009`, `Credits 0x0473EF30`,
`Exit 0x045BEDD8` (`analysis/shots/title_state.png`). `Load Game` does not exist as a string — the
game says `Load` — so the screen falls back through candidates instead of inventing a label.
New `txt` console verb: `-list | -label <text> | -ambig | <0xKEY> | <tableINST> <0xKEY>`.
Verified: headless boot exit 0, `locale 'en-US': 197/200 tables parsed (3 no-grammar, 0 decode
failures), 18441 entries, 10246 distinct keys, 684 ambiguous`; `txt:0x05B08767 -> New Game`;
`txt:0x045BEDD8 -> Exit`; `txt -label Options -> 8 hit(s)`; `txt:0x00000011 ->` refused as ambiguous.
Build: 0 warnings (two C4840/C4477 from a ternary feeding `std::string` to `%s` were fixed).

### TRAP found and corrected by disassembly: the locale search path was dead
The first `text.cpp` run logged "no Locale/en-US package" — and the cause was OURS, not the data.
`fs.cpp` added `exe_dir/Locale/<lang>`, which matches nothing on disk (the shipped layout is
`Data/Locale/<lang>/`). The oracle settles how the original builds the path: at
`0x7e863c..0x7e8661` it loads the wide key `'Locale/'` (`0x01413cc4`), walks it to its NUL, and
**appends** it to a base-path object (`call 0x429760`), then appends the language — so `Locale/`
hangs off a DATA ROOT, not off the executable directory. The same cluster treats `'BaseData/'`
(`0x01413cb0`) / `'UserData/'` (`0x01413c8c`) identically (the VERIFY row 2 tie). `fs::init` now
attaches `Locale/<lang>` to every data root, case-insensitively.
**This is the session's most transferable lesson: a search path that yields "no data" is itself a bug
to disassemble, not a fact to accept.** Two of our "the data does not exist" conclusions (locale text,
and the still-outstanding state names) are of this class.

### Ghidra joined the toolchain, to audit every claim we have made
User request: check everything, starting from `Main`, to see whether we started reversing correctly at
all. Rather than eyeball Ghidra output, the whole claim surface was made machine-checkable:
`analysis/claims.json` records **30 claims / 98 checks** (every address assertion from
`analysis/startup.md`, `VERIFY.md` rows 35–41, `MEMORY.md` and `source/states/statemgr.h`, each with
the doc line it came from and a severity). `tools/ghidra_audit.py` turns that into a request,
`tools/ghidra_export.py` (PyGhidra) extracts Ghidra's OWN facts + pseudocode, and `verify` diffs them.
Verdicts are PASS/FAIL/UNKNOWN/SKIP with two hard rules: **UNKNOWN is not PASS** and **SKIP is not
PASS**, so the audit can never quietly bless a claim Ghidra could not answer. Data-derived claims (the
gmdl stride, measured on 11,041 resources) are recorded as SKIP rather than dropped, so the audit
states what it does NOT cover. Immediates are bucketed BY MNEMONIC so `pushes_imm: 640` cannot be
satisfied by an unrelated `cmp`. New `pseudo_regex`/`pseudo_not_regex` check kinds exist for ORDERING
claims — the FNV-1-vs-1a direction (`(h*prime)^c` vs `(h^c)*prime`) is invisible in a flat listing and
obvious in a decompiler's expression tree, and it is the single most load-bearing arithmetic fact in
the project (every group/instance/state id depends on it).
Setup facts that cost a round: Ghidra 12 **removed embedded Jython**, so `-postScript *.py` fails with
"Ghidra was not started with PyGhidra"; and **`-analysisTimeoutPerFile 0` means time out IMMEDIATELY**,
not "no limit" — the first run analyzed nothing and still exited 0, which looked like success.
Ghidra 12.1.4 installed via scoop; its 543 MB download failed twice under scoop's downloader and
completed under `aria2c` into scoop's cache. Project + venv live under gitignored `refs/ghidra/`.

### Config scripts read end-to-end: three files, variables resolved
`source/engine/config.cpp` now loads ALL THREE `Config/*.txt`: `Properties.txt` (63 option
name->id), `ConfigManager.txt` (211 assignments; `$variable` references are resolved against the
`set` table - e.g. `setOption OptionVideoRes $Medium` -> `2`, last assignment wins), and
`VideoCards.txt` (**4 vendors, 616 cards**). Log: `[config] config: 63 properties, 83 variables, 4
vendors, 616 cards`. The `cfg` verb exposes `-list/-props/-vars/-cards/<name>`. So the game's whole
configuration surface is readable by our engine.

### States: a real `Title` state, and the original's `-state:Name` colon form
The user's detail: in Spore states are launched as `-state:SpaceAccessoriesEditor` (COLON, not `=`).
Our parser now splits on whichever of `:`/`=` comes first (so a Windows path in a value keeps its
colons) - `-state:Title` verified to work. Measured: `cmdline: -w -devdirs -state=Title`,
`registered 'Loader' id=0xd7d04c04`, `registered 'Title' id=0xdce0e62d`, `-> Title`,
`[10/10] state.goto OK current='Title'`, fps 34, exit 0.
`source/states/title.cpp` adds the first UI state: it loads a real `png` UI atlas as its backdrop
(`graphics_atlas~/GlobalUICell-00.png`, 512x512), draws a menu list over it, and **auto-transitions
to Loader after 4 s** - a real state-machine transition on our renderer
(`analysis/shots/title_state.png`). The state names are NOT the `spui` screen names (checked: the
`spui` list has `ScenarioEditMode*`/`PlayMode_Panels`, no `*AccessoriesEditor`), so the state set
remains data we have not located (`SporeAppStates`/`appmodes` groups are absent from our packages).

### UI: the engine draws a real Spore UI atlas (first piece of the menu chrome)
The Loader now loads a real UI backdrop - a large `png` whose name looks like UI chrome
(`bckgd`/`launchscreen`/`newgameflow`/`globalui`) - and draws it behind everything (stretched),
then the model grid and text on top. Measured: `graphics_atlas~/GlobalUICell-00.png` (512x512) - the
game's own UI atlas (buttons, arrows, sliders, checkboxes, toggles). Capture:
`analysis/shots/ui_backdrop.png`. This is our first step along the Main->playable path into UI; the
next is the `spui` layout parser so real screens can be composed from these atlases.

### UI (SPUI): the menus are data-driven screens, and we have their names
Disassembled the UI path: menus are `spui` resources (type 0x250fe9a2, **524 screens, 407 named** via
FNV-1-lower over `reg_file.txt`) - including `TitleScreen`, `LaunchScreen`, `GameLaunchScreen`,
`SporeTitleSeq`, `CSATitleScreen`, `GameSettings*`, `GlobalUIOptions-2`, `MessageBox`,
`NetworkLoginDialog`. Related types `layout_bin` (0x050f4731) / `layout` (0xefbda3ff) are declared but
only `spui` ships. A screen is requested by id via `0x0080fff0` ({spui,layout_bin,layout} ->
`[mgr]->vtable+0x24`). The `UTFWin/cSPUI*Win` window classes (`.rdata 0x1416c40..`) are registered from
the cluster `0x8009f0..0x800b60`. Full name list `analysis/spui_screens.txt`, notes `analysis/ui.md`.

### The Loader shows FOUR real models at once (2x2 grid, each textured)
Instead of cycling one model, the Loader loads up to four real gmdl models with DISTINCT instance ids
(so the grid shows four different models) and draws them in a 2x2 grid, each with its paired
texture, with a caption. Captured: `analysis/shots/model_grid.png` - a pink creature,
`floraModels_LOD2~/FE_volcano1_Sp2`, `floraModels_LOD3~/FE_volcano2_Mp2`, and a purple creature
(`0x067b0e97`). 10/10 markers, fps 33, exit 0. The old single-model cycling is recorded above.

### The Loader now cycles through several real models, each with its own texture
Instead of showing one model, the Loader collects up to 12 real gmdl models that parse and have a
matched texture, then cycles every 3 seconds (`loader_tick` -> `load_model`), swapping both the mesh
and its paired texture. Measured over a 10 s run: `0x067b0fbc.gmdl`, `floraModels_LOD2~/FE_volcano1_Sp2`,
`floraModels_LOD3~/FE_volcano1_Sp2`, ... - each with `flora_editorTextureDiffuse~/FE_...`. The capture
`analysis/shots/model_cycle.png` shows a plant model with its correct flora texture. 10/10 markers.

### Model gallery: a spread of exported models rendered
Rendered a spread of exported gmdl models into one montage - `analysis/shots/model_gallery.png` shows
a building, machinery, a plant, a vehicle and a spacecraft: real Spore geometry decoded from the
shipped packages and exported as OBJ. (Built with `tools/render_obj.py` + a small tiling script.)

### Model <-> texture binding by instance id (100% match)
The Loader now binds a model's OWN texture: a `raster` whose instance id equals the gmdl's instance
id. Measured: **865 of 865** gmdl instances in `Spore_Content` have a matching raster instance, so
the binding is exact (not a heuristic). The Loader decodes the mesh first, then looks up the raster
with the same instance. Result: `analysis/shots/model_matched_texture.png` shows the Spore creature
textured with its **correct** palette (pink body, dark head, white eyes/feet) - previously the
texture was picked by a density heuristic and was the wrong one (green/purple). Log:
`model texture: 0x40622900/0x067b0fbc.raster (512x512 DXT5) matched by instance 0x067b0fbc` for the
mesh `0x40627100/0x067b0fbc.gmdl`. 10/10 markers, exit 0.

### gmdl: the vertex stride is variable (vertex_bytes/vertex_count) - 3.4x more models
Found and fixed the real bug behind the low parse rate: the vertex stride is NOT a constant 36, it is
`vertex_bytes / vertex_count` and it varies with the declaration count N. Measured on the exact
variants: **N=3 -> 24, N=4 -> 28, N=6 -> 36**, with **position = the first float3 at every stride**.
Computing the stride instead of hardcoding 36 raised the exact-parse rate from **491 to 1,747** of
4,209 in `Spore_Content` (x3.6) and game-wide from **1,776 to 6,066 OBJ** (x3.4), 0 failed. The
validation tolerance was then made RELATIVE (5% of the bbox diagonal - the header bbox is a rounded
summary), lifting `Spore_Content` 1,747 -> **2,016** and game-wide 6,066 -> **6,979 OBJ (63%)**. A
block-start shift search (0..40 bytes) always found shift 0 best, so the block start was already
right; the remaining rejects have errors of 7-16 (a genuinely different layout). Rendered samples of
the new variants are real, coherent geometry (`analysis/shots/gmdl_fixed_*.png`). Recorded in MEMORY.

### gmdl: the layout is a SUBSET, and the parser now validates (491/4209 exact)
While quality-checking the OBJ export I found a real bug: the parser accepted gmdl variants whose
attribute layout we have NOT derived, producing garbage meshes (one had 2 vertices and 13,162 faces).
Root cause: our position formula (`voff + 0x1C + (N-1)*12`, stride 36, position = first float3) only
fits ONE layout variant. The vertex-declaration count `decl_n` is **3, 4, 5 or 6** across the game
(Spore_Content: {3:1179, 4:483, 5:3, 6:551}), and only **491 of 4,209** reproduce the file's own
header bbox (median error 10.9 - clearly wrong for the rest). Fix: `gmdl::parse` now computes the
bbox of the extracted positions and returns `ok=false` unless it matches the header bbox
(`err < 1e-3`) - the exactness test that solved gmdl, applied as a guard. Measured: `pkg:-unpack
...,gmdl` on Spore_Content now writes **491 OBJ + 3,718 raw .gmdl** (was 2,216 OBJ, many garbage).
The Loader mesh is unaffected (10/10 markers, textured mesh still drawn). Recorded in MEMORY.

### gmsh: reads as a vec3 triangle list, but not a simple array
`gmsh` (0x01c135da, 2,535) has a stable 24-byte header on 2534/2535 payloads, then vec3 triples
(positions). But `len % 12` is spread across every residue, so it is NOT `header + N*12` - there is
variable-length data (index/attribute sections) after the floats. Loaded via the resource manager at
`0x402301`. Layout not derived; recorded so the next attempt reads the reader.

### A progress map like the decomp/recomp projects
`analysis/features.json` is the single source of truth for what is reversed/implemented/verified;
`tools/feature_map.py` renders it to `analysis/shots/reverse_map.png` (a grid of green/amber/grey
squares grouped by subsystem, with per-category percentages and an overall bar) **and** to the text
list `analysis/features.md`. Both come from the same JSON, so they cannot drift apart. Current
numbers: **212 features, engine 78.5%, overall 70.0%** (harness 93%, startup 96%, launch args 97%,
packages 91%, gmdl 85%, rw4 53%, render 79%, formats 10%, gameplay 0%, platform 25%). The map is
embedded in `README.md`; PROMPT/RESUME/PROGRESS/IDEAS link to it. To grow it: edit the JSON, run
`python tools/feature_map.py`.

### A function map (the honest counterweight to the feature map)
The feature map measures the scope we chose; it can look high. So `tools/functions.py` adds a HARD
number: candidate function starts in the oracle's own `.text` (union of E8/E9 rel32 targets, .rdata/
.data pointers into .text, and prologues `55 8B EC` / `8B FF`), versus the functions we have actually
reversed (`analysis/functions_named.json`). Result: **105,239 candidates, 29 reversed, 0.021%**. The
map now draws this as a second gauge. The **beta PDB is not the denominator**: it is a different link
(0/12 addresses match — `analysis/build_identity.md`), so it can supply names only where function
identity matches, not addresses. Recorded in `analysis/functions.md`.

### The beta PDB is NOT stripped — 136k function names harvested
The PDB has no standard `/GLOBAL`/`/PUBLIC` streams (which is why the earlier reader called it
stripped), but it is full of **MSVC-mangled symbol names**: 194,027 unique mangled runs, **136,433**
unique function/method name paths. `tools/pdb_symbols.py` reverses the `@`-separated path into
`Namespace::Class::Method` and buckets by subsystem: **SP 63,246, EA 24,214, eastl 9,874, rw 6,582**
(RenderWare!), `nSPCreatureAnim` 2,423, `nSPSkinner` 1,561. So a PDB function map IS possible — as a
**name** inventory (addresses do not map). It is what makes the disassembly readable, and it tells us
the scale of the RenderWare subsystem (`rw`: 6,582 functions). Output: `analysis/pdb_symbols.md` +
`analysis/functions.md` (which now shows both the oracle count and this shape).

### Can addresses be guessed from the PDB? Partially — and the limits are now proven
The user asked whether a script could guess all addresses from the PDB. Answer: **a full map is
impossible**, and the three reasons are verified, not assumed: (1) there is no beta **binary**, only
the PDB, so there is no code to fingerprint; (2) no reference build matches the oracle (all `Also*`
have different CodeView GUIDs); (3) the GA build is **/GR- (RTTI off)** — only 2 type-descriptor names
exist — and assert function-name strings are absent. A PARTIAL guesser works and is built:
`tools/name_guess.py` attributes each `.rdata` string referenced by `push imm32`/`mov r32,imm32` to
its enclosing function, then matches distinctive strings to PDB name tokens. Result: **220 candidate
address->name guesses, 39 HIGH-confidence** (e.g. `0x007f6160 SPUICreateAnimationTimerRamp`,
`0x0089f060 EA::Allocator::StackAllocatorCoreAllocate`, `0x00845710
EA::ArgScript::cParser::CommandExists`, `0x00a73c10 SP::EffectsManager`, `0x00ea66b0
SP::cPlanetModel::IsUnderWater`). Refined this session: the guesser uses STRICT function starts
(prologues + `E8` targets only — dropping .rdata/.data pointers, which are often jump-table case
labels and produced mid-function addresses), reports the exact push site, and documents two honest
limits: it keys on self-describing functions so it barely overlaps our hand-reversed workhorse
routines, and when one string matches several PDB names it identifies the FAMILY reliably but the
member shown is arbitrary (verified: `0x0092c540` pushes `Refpack` -> `EA::Refpack::*`). All rows are
guesses to check by disassembly; output `analysis/name_guesses.md`. Also: `tools/pdb_symbols.py`
shows the PDB is NOT stripped — 136,433 unique function/method name paths (SP 63,246, EA 24,214,
eastl 9,874, rw 6,582, nSPCreatureAnim 2,423). The feature map gained a `tooling` category (89%).

### The mesh is now TEXTURED (real UVs + real texture)
`gmdl.cpp` now also reads the vertex UV (2 floats at offset 16, the N=6 layout), `render/mesh.cpp`
carries UVs in its FVF and applies a bound texture (MODULATE with the fixed-function lighting), and
the Loader passes the decoded `raster` texture to the mesh draw. Result: `analysis/shots/mesh_textured.png`
shows the Spore creature **textured** with the game's own `raster` (its green/purple palette), next to
the texture itself - real geometry AND real texture, both decoded from the packages, on our renderer.
fps 32.7, exit 0, 10/10 markers.

### The unpacker exports solved formats as standard files
`pkg:-unpack` now converts the formats we solved into standard files instead of dumping their raw
bytes: **`raster` -> BMP** (via `raster::decode`) and **`gmdl` -> OBJ** (via `gmdl::parse`). Measured
on `Spore_Content`: 2,770 BMP (e.g. `building_editorTextureDiffuse~/Elevated Hall.bmp`, 512x512 32bpp,
`BM` signature) and OBJ models. Since a model's texture is the `raster` with the SAME instance id
(865/865 match), the gmdl export now also writes the **paired `.bmp` + a `.mtl`** and references them
(`mtllib`/`usemtl`/`map_Kd`) - so exported models are self-contained and textured: on Spore_Content,
2,016 OBJ + 2,016 MTL + 2,016 BMP. `gmdl` records whose layout we have not derived fall back to raw
bytes.

**Whole game (all 57 packages):** `raster` -> **5,747 BMP**, `gmdl` -> **1,776 OBJ** (bbox-validated;
the rest fall back to raw bytes), **0 failed** on both. An earlier run reported 4,084 OBJ, but that was
BEFORE the bbox guard below - many were garbage meshes.

### Asset format survey: blockdata solved as a vec3/bbox array; gmsh is mesh-like
Went looking for a tractable mass format. **`blockdata` (0x5caa5e28, 3,141)** is an array of `vec3`
(every payload size is a multiple of 12 bytes) whose **first two `vec3` are a min/max bounding box**
(`min <= max` verified on samples). **`gmsh` (0x01c135da, 2,535)** starts with big-endian-looking
counts then float32 runs - mesh-like (cf. `gmdl`), layout not derived. Added `analysis/formats.md`, a
per-type status table (png/raster/gmdl/locale SOLVED; trigger/prop/blockdata/rw4 PARTIAL; gmsh/audio/
animation TODO).

### Content loading: the resource manager and the Config/*.txt scripts
Found the layer above DBPF: the resource-manager getter `0x8de4b0` (global `0x1667aa0`) and two
query entry points (`0x686450`, `0x847780`) taking a `{group, type}` descriptor. The game's configs
are `trigger` resources (type `0x024a0e52`) referenced at three sites. Resolving the group ids with
FNV-1-lower over `reg_file.txt` gave real names: `0x658b0ab2`=`properties`, `0x9a06678c`=
`ConfigManager`, `0xd6396913`=`SporeAppStates` — so `run` loads the state list from group
`SporeAppStates` (at `0xf48505`).

Decisively, those configs are **loose text files**: `Data/Config/Properties.txt`,
`ConfigManager.txt`, `VideoCards.txt`. `Properties.txt` is the game's own option table
(`property <Name> <0xid> <type>`, 47 options); `ConfigManager.txt` is an imperative script
(`set/setOption/if/elseif/endif/trace/include/alert/...`). The option ids are a MIX — only 2 are
FNV-1-lower of the name, the rest explicitly assigned — so the file is authoritative. This answers
"how does the game read its config/state text": loose `Config/*.txt` scripts plus `trigger`
resources, via the resource manager. New: `analysis/content-loading.md`, `analysis/config-scripts.md`.

**Wired into OUR engine:** `source/engine/config.cpp` reads `Config/Properties.txt` and
`ConfigManager.txt` through the fs search paths at boot (log `[config] config: 63 properties, 83
variables from Config/*.txt`), and a `cfg` console verb exposes it (`cfg -list|-props|-vars|<name>`).
The option ids come in three forms — explicit hex, decimal, and `(hash(Name))` (whose inner name can
differ from the property name, e.g. `AudioMasterVolume` -> `hash(mastervolume)` = `0xaded51d5`); the
C++ reader resolves all three. Conditionals are recorded, not evaluated (they are hardware-dependent).
Measured: `cfg AudioMasterVolume` -> `property 0xaded51d5 float`; `cfg -props` lists all 63; windowed
run still 10/10 markers, raster + mesh drawn, fps 32, exit 0.

### The MAIN LOOP is found: 0x008482f0
After ruling out run, every AppMode vtable slot, and a direct pump call, the loop was found by a
different route: the engine imports **`TranslateMessage` (IAT 0x13cc5cc)** and **`DispatchMessageW`
(IAT 0x13cc630)**, and both are called from ONE function — **`0x8482f0`** (window-class vtable
`.rdata 0x141cc10`, slot **`+0x84`**). It is a classic message loop: `mov edi, ds:0x13cc62c`
(**PeekMessageW**), then `PeekMessageW(&msg,0,0,0,PM_REMOVE)` -> `cmp [msg.message],0x12` (WM_QUIT) ->
dispatch to the window event handlers `[esi+0x5c/0x60/0x64]` -> `TranslateMessage` ->
`DispatchMessageW` -> repeat until the queue is empty. `run` calls it at **`0xf48405`**
(`call [0x67dcb0()]->vtable+0x84`; `0x67dcb0` returns the window object from global `0x15fd89c`).

Trap recorded: the engine has TWO PeekMessage users — the main loop uses `PeekMessageW` through a
register (`edi`), the mouse helper `0x84a050` uses `PeekMessageA` directly (window slot `+0x88`).
That is why a search for direct callers of the pump (and a `ff 15 <IAT>` search) both missed the
loop. The window class is now fully pinned: ctor `0x849b20`, Create `0x849bf0`, main loop `+0x84`,
mouse pump `+0x88`, dtor `0x849e80` (VERIFY rows 39-40).

### The original's startup chain, traced from the entry point
The user asked how the game actually boots (the "start from main" approach). Traced it end to end:
PE entry `0x011e11e1` -> CRT init -> `__tmainCRTStartup 0x011e0f21` -> **WinMain `0x00f489e0`** (the
call at `0x11e105c`, hInstance 0x400000) -> **app_main `0x00f48810`** -> **run `0x00f47e90`**.
app_main reads `-multipleInstances` and otherwise enforces one instance with a named object
`Global\SporeEditorOrGame` (`Another instance of the game is already running`), then allocates the
App object and calls run. run does an early-init check, reads `-demo`, allocates a 328-byte App
object, sets the **filesystem mode from `-devDirs`/`-shipDirs`** (`0x687bd0`), creates the app-mode /
state-manager host via **CreateAppMode `0x007e8a00`** (496-byte object, ctor `0x007e76e0`) into
`[app+0x20]`, reads `-baseGame`, and enters the subsystem setup + main loop. Full doc:
`analysis/startup.md`; VERIFY rows 35-37. Next: walk the AppMode ctor `0x007e76e0` and the rest of
run, then find where the (text) `trigger` resources and SPUI are loaded.

### raster (RenderWare textures) — header solved, DXT decodes, and the ENGINE draws it
`raster` (type 0x2f4e681c, 5,931 resources) is the engine's texture format. A fast scan (each
package read once) over 1,200 payloads gave a stable header: `{u32 1, u32 dim, u32 dim, u32 code,
u32 8, char[4] tag, u32 base_bytes}`, where `tag` is `DXT5` (59%), `DXT1` (10%), or raw BGRA
(0x15). `tools/raster_decode.py` decodes the base level: `Spore_Content` rec[125] is a 256x256
**DXT1 normal map** (`analysis/shots/raster_dxt1.png` — the purple/blue sculpted relief), a clear,
correct decode; DXT5 alpha atlases and raw BGRA also decode. (The first scan attempt never finished
because it re-read the whole package for every record; the rewrite reads each package once.)

**Wired into the engine:** `source/engine/raster.cpp` is the C++ port of the decoder; the Loader now
decodes a `raster` and draws it with `create_texture_bgra`, preferring it over the `png` path.
Measured: `showing real raster 0x40642901/0x067a07e7.raster (512x512 DXT5)` and
`analysis/shots/raster_engine.png` show the game's OWN texture format rendered by our own renderer,
beside the gmdl mesh. A demo-only picker skips `raw` textures and normal/specular/mask names, because
this game stores normal maps uncompressed (raw) and they would just look flat purple. Open: the
`code` field's meaning and the mip layout.

### Our renderer now draws a decoded gmdl mesh
The solved gmdl geometry (session 12) is now on OUR screen: `source/engine/gmdl.cpp` is the C++
port of the parser, `source/render/mesh.cpp` uploads positions+indices, computes smooth normals and
draws them with a fixed-function orbit camera, and the Loader draws one mesh into its own viewport.
No D3DX (the matrices are built by hand, since the DX SDK is not installed). Measured:
`showing real mesh ...gmdl (3765 verts, 6000 tris) from Spore_Content.package`, and the in-engine
capture `analysis/shots/mesh_engine.png` shows a real Spore creature - head, eyes and limbs -
shaded and rotating, next to the real decoded texture and the full inventory. fps 32.3, exit 0.

Two implementation notes that are easy to get wrong: the mesh lives OUTSIDE the `Loader` struct
because Loader is reset by copy-assignment and would double-free the D3D pointers; and the mesh
draw sets `COLOROP = SELECTARG1(DIFFUSE)` because its FVF has no texture coordinates and would
otherwise modulate with whatever texture the overlay left bound.

### A real bug: the FIRST command-line argument was silently dropped
Chasing why `-headless` "did nothing" exposed a long-standing defect. `wWinMain` receives
`lpCmdLine`, which **excludes** the program name, but `parse_command_line` erased the first token
anyway - so the first argument was always lost. It hid because `-w` was always first and windowed
is the default; the `-f`/`-r` tests had `-headless` first, so `-f` was actually the *second*
argument and worked. Consequence: **session 12's "headless" run was not headless** - it created a
window; the flag was the first argument and got dropped. Fixed with an explicit `has_program_name`
parameter (false for lpCmdLine, true for `GetCommandLineW()`). Re-verified: `-headless -devDirs`
now logs `headless: no window` / `headless: no device`, and a genuinely windowless unpack writes
2,944 files / 27,882,571 bytes in 8.3 s. `-f -r 1024x768` as the *first* arguments now give
`windowed=0 ... 1024x768`.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
Windowed: `-w -devDirs -capture mesh.bmp -captureAfter 45 -quitAfter 60` → exit 0, `cmdline: -w ...`
(`-w` now parsed as the first argument), `showing real mesh ... (3765 verts, 6000 tris)`, 10/10
markers, `fps=32.3`, capture 3,686,454 bytes → `analysis/shots/mesh_engine.png`.
Headless (genuinely): `-headless -devDirs -exec pkg:-unpack,all,<dir>,locale -quitAfter 8` →
`headless: no window`, `headless: no device`, `unpack: 57 pkgs, 2944 written, 0 failed, 27882571
bytes`, 2,944 files on disk in 8.3 s.
gmdl attributes: `python tools/gmdl_attribs.py --pkg 48/52/54` → channels 4,5 (byte offset 16) are
in [0,1] on **100%** of vertices on all three N=6 variants → UV. Channel 3 is always 0; bytes 24-35
are still open; N=5 lays out differently.
Progress map: `python tools/feature_map.py` → `analysis/shots/reverse_map.png` +
`analysis/features.md`, 212 features, engine 78.5%, overall 70.0%.

---

### RenderWare 4 (header confirmed, object table not reversed)
Following the RW-4 finding from session 7, the header is now pinned down: a **28-byte template**
at `.rdata 0x014f6480` (`\x89RW4` `w32\0` `\r\n\x1a\n` `00 20 04 00` `"454\0"` `"000\0"` + zero)
is copied verbatim by the constructor at `0x011e3f60` (`mov ecx,7 / mov esi,0x14f6480 / rep movs`).
The writer caller (`0x006c24b3`) pushes the sentinel `0xcafed00d`. Added `tools/rw4.py`, which
validates and dumps that header and stops there on purpose.

Still NOT reversed: the object/chunk table after the header, the `+0x0c` flags, and whether
`"454"` is a version or a build tag. Recorded as VERIFY rows 31-32 with row 32 explicitly
"NOT REVERSED" so nothing is assumed. This is the honest boundary: the format's surface is
confirmed, its body is not.

---

## Session 12 — 2026-10-13: headless mode (unpacker without a window); gmdl vertex layout ruled out

### Headless mode (finishes the unpacker wishlist item)
`-headless` (aliases `-noWindow`) skips the window, the D3D9 device and the font atlas, but runs
everything else: logging, the harness, the search paths, the naming registry, the console, and the
Loader state's package scan. Console commands still execute, so the inspector and unpacker work on a
machine with no display (or in CI). Measured:
```
OpenSpore.exe -headless -devDirs -exec pkg:-unpack,all,<dir>,locale -quitAfter 8
  -> 57 pkgs, 2944 written, 148001 skipped, 0 failed, 27,882,571 bytes  (2944 files on disk)
```
That is **bit-for-bit the same result as the windowed run** (2,944 files, 27,882,571 bytes), in
7.6 s, with no window created. Windowed mode re-checked afterwards: window + device + atlas + the
real asset still render, capture still 3,686,454 bytes, exit 0.

> **CORRECTION (session 13).** This "headless" run was **not** headless: `-headless` was the first
> argument and the command-line parser was dropping the first argument, so a window was created. The
> flag and the parser were both fixed in session 13; the genuine windowless run is
> `headless: no window` / `headless: no device`, 2,944 files in 8.3 s. See session 13 above.

### gmdl sub-mesh descriptor CONFIRMED (the "variable header" mystery solved)
Session 11 disproved the fixed triple. This session found what the triple really is by looking at
**four different variants side by side** instead of one: a genuine sub-mesh descriptor
`{format=4, index_count, stride=16, index_bytes=2*index_count}`, but it sits after a **variable**
header, a `u32 count` and a `float32 bbox[7]`, plus a `u32 submesh_count`. So its offset moves per
file. Measured: `Spore_Content` rec[12] bbox@0x48 desc@0x68; `EP1_Content_01` rec[0] bbox@0x60
desc@0x80; `EP1_Content_02` rec[1] bbox@0x0c, **submesh_count=2**, desc@0x2c (and a second descriptor
at 0x339b4); `EP1_Data` rec[579] bbox@0x18 desc@0x38. In every case `submesh_count` is at
`bbox+0x1c` and the descriptor at `bbox+0x20`. `tools/gmdl_mesh.py` was rewritten to FIND the
descriptor by self-consistency (format 4, stride 16, `index_bytes == 2*index_count`, indices fit)
rather than assume an offset, and it now parses all four, including the two-sub-mesh file whose
second sub-mesh indexes into the same vertex buffer (first index 24353).

### gmdl meshes SOLVED — the stride is 36, and real Spore geometry came out
The earlier "vertex layout ruled out" section above is superseded by this. Looking at FOUR variants
side by side (instead of scanning one) exposed a **vertex-block header** after the indices, identical
in all of them: `u32 1, u32 N, 0, 2, 0, 0x000C0000`, then `(N-1)` 12-byte declaration entries, then
`u32 0, u32 vertex_count, u32 vertex_bytes`. `N` is 5 or 6; `vertex_count == max_index+1`; and
`vertex_bytes == vertex_count * 36` — **36, not the descriptor's 16** (that field is a reserved code;
chasing it as the stride was the trap). The vertex count field sits at `voff + 0x1C + (N-1)*12`
(0x58 for N=6, 0x4C for N=5). Reading each vertex as nine floats with **position = the first float3**,
the extracted positions reproduce the file's own header bbox **exactly, err 0.0000**, on all four:
`Spore_Content` rec[12] (vc 3765), `EP1_Content_01` rec[0] (16551), `EP1_Content_02` rec[1] (28880,
two sub-meshes), `EP1_Data` rec[579] (2357).

Exported to OBJ and rendered with a tiny dependency-free software rasteriser
(`tools/render_obj.py`): `analysis/shots/gmdl_pkg48.png` is a limb/organ, `gmdl_pkg52.png` a rounded
creature with appendages, `gmdl_pkg54.png` a branching plant. That is **real Spore geometry, decoded
from the shipped bytes**. Still open: what the six non-position floats mean (declared by N, not yet
identified).

### gmdl vertex layout ruled out (a second disproven hypothesis, with the test that killed it)
Last session established gmdl's header is not a fixed triple. This session tested the NEXT guess -
that the vertex block is a plain array of vec3 with a fixed stride. The test is exact and uses the
file's own header: read the block as N-byte vertices and compare the computed bounding box with the
header bbox at `+0x48`. Result: **no stride matches** (12/16/20/24/28/32 all fail; the Z range alone
coincidentally lines up at stride 12, which is exactly the kind of single-field coincidence that
must not be trusted). Conclusion, recorded in MEMORY: **vertices are interleaved** with other
attributes, so the header bbox is a mesh summary and the attribute layout must come from the loader's
disassembly. Byte-scanning cannot resolve it.

### `-w` / `-f` / `-r` — VERIFY row 15 upgraded from DRAFT to DISASM-VERIFIED
Row 15 had said "our `-w` is our own convention" and pointed at `screenWindowed` 0x014111d0. That was
wrong: `screenWindowed` is a **RenderWare device metric** (it sits in a `{name,index}` table at
0x1411440, index 14, next to `screenWidth`/`screenHeight`/`screenBPP`), not a command-line flag. The
real handling is in the device-creation function `0x7e74a0..0x7e7637`: it fills a config struct with
the 800×600 default (the known `push 0x320 / push 0x258`), overrides width/height if `r` (0x1413ac4)
is present by parsing it with `%dx%d` (0x1413ab8, import 0x13cc4e0), then looks up `f` (0x1413ab0)
and `w` (0x1413ab4) through the same arg helper `0x92b570` and sets a windowed byte — `f`→0, `w`→1,
with `f` tested first so **`w` wins**. The sole `GetCommandLineA` consumer `0x00e20b5a` converts the
ANSI command line to UTF-16 (`0x93c510`), which is why the arg helper compares UTF-16 names.

Why the old census missed them: `tools/arg_vocab.py` required names ≥2 characters, so the
single-letter flags `w`/`f`/`r` were silently dropped. It now takes `min_len=1` at a verified helper
call site; the vocabulary grew from 56 to **60 names** and 72→76 decodable call sites.
`cmdline.cpp` now implements `-f` and `-r WxH` (with `w` beating `f`), and all four cases were
measured against the log (see below).

Also this session: the model-manager globals' getter/setter pairs were located
(`0x67dcd0..0x67df70` are one-line getters/setters for `0x15fd8b0..0x15fd8d0`), and the setter used
by the rw4 handler (`0x67df40`, manager `0x15fd8c4`) is called from `0x7a71cb` / `0x7a7efa` — so the
model subsystem's entry points are known and the parser is reachable from there next time.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
Headless: `OpenSpore.exe -headless -devDirs -exec pkg:-unpack,all,<dir>,locale -quitAfter 8` →
exit 0, `unpack: 57 pkgs, 2944 written, 0 failed, 27882571 bytes`, 2,944 files on disk.
Windowed regression: `-w -devDirs -capture hl.bmp -captureAfter 20 -quitAfter 35` → exit 0,
`window ... icon=embedded`, `device windowed 1280x720`, `font atlas ready`, `showing real asset
vehiclepainticons~/be_basic_shake_05.png`, capture 3,686,454 bytes.
gmdl stride test: the bbox-match search reported no candidate; recorded with the method.
gmdl descriptor: `python tools/gmdl_mesh.py --pkg 48/52/53/54` parses all four variants, finding the
self-consistent descriptor(s) (`Spore_Content` desc@0x68 18000 idx; `EP1_Content_01` desc@0x80 64752
idx; `EP1_Content_02` **two** descriptors @0x2c/0x339b4; `EP1_Data` desc@0x38 8814 idx), with
`submesh_count` at `bbox+0x1c` and the descriptor at `bbox+0x20` in every case.
gmdl mesh SOLVED: `python tools/gmdl_mesh.py --pkg 48 --obj ...` → **bbox check OK (err 0.0000)** for
all four (vc 3765/16551/28880/2357, stride 36, `vertex_bytes == vc*36`). Rendered via
`python tools/render_obj.py <obj> <png>` → `analysis/shots/gmdl_pkg{48,52,54}.png` show a limb, a
creature and a plant.
`-w`/`-f`/`-r` (headless, log `flags:`): `-f -r 1024x768` → `windowed=0 … 1024x768`;
`-w -r 800x600` → `windowed=1 … 800x600`; `-f -w -r 640x480` → `windowed=1` (**w wins**);
`-r=1920x1080` → `windowed=1 … 1920x1080`.
`tools/arg_vocab.py 0x92b570 --out analysis/arg_vocab.md` → 76 call sites, 76 decodable,
60 distinct names (now includes `w`, `f`, `r`).

---

## Session 11 — 2026-10-13: rw4 = models; gmdl explored and a single-file fit disproven

Two things: identified what the model resources are, and tested a hypothesis honestly into the
ground.

### rw4 = models (meshes)
User confirmation, and it matches the data: 12,987 `rw4` + 11,041 `gmdl` are the geometry the game
draws. The registration is now pinned: `0x007635f0` pushes handler `0x7633c0` + type `0x2f4e681b`
into the register function `0x6ad090` (a hash-map insert keyed by type). The handler itself only
dispatches through manager singletons (getters at `0x67dd20`/`0x67dd30` returning globals
`0x15fd8c4`/`0x15fd8c8`), so the actual mesh parser is further in - a virtual-dispatch chain, next
session's work. New tool `tools/dump_u32.py` reads those registration tables.

### gmdl explored — and a clean-looking find disproven
`gmdl` (0x00E6BCE5) looked far more tractable than rw4: payloads visibly contain float32 vertex
data and u16 index runs. On `Spore_Content.package` rec[12] the structure was *beautiful*: a float
bounding box at `+0x48`, `+0x6c = 18,000`, `+0x74 = 36,000 = 2*18,000`, and a u16 array at `+0x78`
with exactly 18,000 entries all in range (max 3,764). It looked like the format.

**It is not.** `tools/gmdl_header_check.py` tested the invariant across 400 sampled gmdl resources:
it holds in **18 (4.5%)**. The three fields take wildly different values elsewhere (`+0x70` is 16 in
the fitting cases but `0x1B684E2F`, 1,245,202, 1,376,276, ... in others), so `+0x6C` is a variable
**tagged** structure, not a fixed triple. Recorded as IDEAS §5.25 with the numbers.

This is the method working as intended: the same broad-consistency test that validated RefPack
(30,000/30,000) is what refuted this claim (18/400) before it reached the codebase. A fit on one
file is not a format.

Still solid from the exploration: every gmdl starts with dword `8`; payloads mix float32 and u16
data; and the header contains repeated `{0x1B684E2F, <value>, 0x00616041}` groups, where
`0x1B684E2F` is the **rw4 type id byte-swapped** - consistent with RefPack's big-endian integers,
so gmdl likely carries big-endian tags.

### How this session was verified
`python tools/gmdl_mesh.py --pkg 48` → the bbox, `index count 18000`, `index byte size 36000
(== count*2 ? True)`, `indices @0x78..0x8d18: 18000 u16, max=3764` (a real structure on ONE file).
`python tools/gmdl_header_check.py --limit 300` → `+0x74 == 2 * +0x6c : 13 (4.3%)`, and across 400
sampled records 18 fit - the disproof.
`tools/dump_u32.py` + `tools/find_imm.py 0x2f4e681b` → the model-type registration at `0x007635f0`
-> `0x6ad090`. Build unchanged (tools only).

---

## Session 10 — 2026-10-13: a real Spore asset rendered by our own engine

The pipeline now ends in pixels. `source/render/imgfile.*` decodes a PNG **from memory** via GDI+
(no temp file) into tight BGRA, `Device::create_texture_bgra` uploads it as A8R8G8B8, and
`Overlay::image` draws it. The Loader picks one real `png` resource, decodes it and shows it with
its real name.

Measured: `showing real asset vehiclepainticons~/be_basic_shake_05.png (128x128, 24685 decoded
bytes) from PatchData.package`, `uploaded 128x128 BGRA texture`, and the captured frame
(`analysis/shots/hero_real_asset.png`) shows that blue vehicle-paint scale pattern rendered by our
renderer, next to the full inventory readout. That is DBPF index → RefPack → type/instance naming
→ PNG decode → D3D9 texture → draw, all in our own code.

Also recorded from the user: **`rw4` resources are MODELS (meshes)** — which raises their priority,
since 12,987 of them plus 11,041 `gmdl` are the geometry the game draws.

New tool `tools/dump_u32.py` reads registration tables, and it found the model-type wiring:
`0x007635f0` pushes handler `0x7633c0` and type `0x2f4e681b` into the register function `0x6ad090`;
the handler itself only dispatches through manager virtuals, so the mesh parser is further in.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings (one real portability fix: GDI+ needs
`objidl.h` + `propidl.h` included *before* `gdiplus.h`, else `GdiplusHeaders.h` fails on
`IStream`/`PROPID`).
`OpenSpore.exe -w -devDirs -capture asset.bmp -captureAfter 25 -quitAfter 40` → exit code 0,
`GDI+ up`, `uploaded 128x128 BGRA texture`, `showing real asset vehiclepainticons~/be_basic_shake_05.png`,
`fps=33.5`; the captured frame was converted and **viewed** — the asset renders.

---

## Session 9 — 2026-10-13: the unpacker works on real data (all 57 packages)

Turned the reversed pipeline into a usable tool: `pkg:-unpack <pkgIndex|all> <outdir> [type]`
writes every resource to disk under its real name, decoded — PROMPT.md's "standalone unpacker"
wish, and it exercises the whole chain at once (DBPF index → RefPack → type/instance naming).

Measured on the shipped game:
```
pkg:-unpack,all,<dir>,locale  -> 57 pkgs, 2,944 written, 0 failed, 27,882,571 bytes
pkg:-unpack,all,<dir>,png     -> 57 pkgs, 14,549 written, 0 failed, 254,822,851 bytes
```
The 14,549 PNGs are real images (200/200 sampled carry the PNG signature), one committed as
proof: `analysis/shots/unpacked_png_sample.png` is `GGENewGameFlowUI-00.png`, a UI atlas with the
game's stage icons (fire/tribe, paw/creature, globe/space, rocket) and planetary panoramas.

### A real bug found by counting
First `all` run reported 2,944 written but produced only **206 files** — the flat tree overwrote
itself, because the same group/instance recurs across packages (every locale has its own
`Text/...`; `Spore_Content` repeats base-game names). Fixed by namespacing under `parent_stem`
(`cs-cz_Text/`); a bare package stem was not enough because 17 locales each own a `Text.package`.
After the fix, file count == written count exactly (2,944 / 2,944). Counting files rather than
trusting the log is the check.

### Measurements
* Written: 2,944 (locale) and 14,549 (png), **0 failures**.
* PNG signature: 200/200 valid. Build: 0 errors, 0 warnings.

### How this session was verified
`cmake --build` → 0/0.
`OpenSpore.exe -w -devDirs -exec pkg:-unpack,all,<dir>,png -quitAfter 20` →
`unpack: 57 pkgs, 14549 written, 136396 skipped, 0 failed, 254822851 bytes`; on-disk file count
14,549; 200/200 sampled PNG headers valid; `analysis/shots/unpacked_png_sample.png` renders as a
real UI atlas (viewed).

---

## Session 9 (prep) — RW4 reader search: what was ruled out

Before switching to the unpacker, the RW4 reader was hunted without success, and the **negative
results are recorded** so the next attempt does not repeat them:
* `tools/find_imm.py 0x34575289` and `--ascii w32` → **0** hits; raw byte search of `\x89RW4`,
  `w32\0` and the `\r\n\x1a\n` marker in `.text` → **0** hits. The magic is `memcpy`'d from the
  `.rdata` template (`mov esi,0x14f6480 / rep movs`), so it is never an inline immediate.
* `tools/find_imm.py 0xcafed00d` → 2 hits, **both inside the writer** (`0x006c233d`); it is not
  shared with the read path.
* `tools/callers.py 0x11e3f60` (header constructor) → 2 callers; `tools/callers.py 0x004021a0`
  (the rw4 type accessor) → 3 callers, all in one function (`0x0040246b`). Neither is the reader.
* New tool `tools/dump_u32.py` reads registration tables: the rw4 type id appears in `.rdata`
  `0x0140e028`/`0x0140e050` and in `.data` as `(0x2f4e681b, 0x079a8ee7, <handler>)` registration
  entries at `0x015777e8…0x0157780c` and `0x015ad308…0x015ad320`.
Conclusion: the reader is reached through the **resource-type dispatch** (a registration table),
not through the magic — so the next move is the dispatcher, and `tools/dump_u32.py` +
`tools/callers.py` are the tools for it.

---

## Session 8 — 2026-10-13: RW4 header pinned down (object table still open)

Continues the RW4 work from session 7. The header is now proven, not observed:
- A **28-byte template** at `.rdata 0x014f6480` is copied verbatim by the constructor at
  `0x011e3f60`: `mov ecx,7 / mov esi,0x14f6480 / rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]`.
  Fields: `+0x00` `0x34575289` ("\x89RW4"), `+0x04` `"w32\0"`, `+0x08` `0x0A1A0A0D` ("\r\n\x1a\n"),
  `+0x0c` `0x00042000`, `+0x10` `"454\0"`, `+0x14` `"000\0"`, `+0x18` zero.
- The constructor then fills `[obj+0x1c/0x20/0x24/0x30/0x34/0x38]` and three sub-arrays at
  `0x44` (8 dwords), `0x64` (8 dwords), `0x84` (4 dwords).
- The **writer** caller is `0x006c233d` (calls the constructor at `0x006c24b3`) and pushes the
  sentinel `0xcafed00d`.
- The rw4 type id `0x2F4E681B` reaches a resource accessor at `0x004021b3`.

Added `tools/rw4.py`: validates + dumps that 28-byte header for any decoded rw4 resource, and
stops there on purpose. Verified on `PatchData.package` rec 1456 → `valid RW4 header`,
`platform 'w32'`, `version '454'`, `flags_0c 0x00042000`.

**Explicitly NOT reversed** (VERIFY row 32, marked so nothing is assumed): the object/chunk table
after the header, the `+0x0c` flags, and whether `"454"` is a version or a build tag. The reader
that parses these streams has not been located; the next move is to find it in `.text` and read it
instruction by instruction, the way RefPack was done - not to import a third-party RW4 description.

### How this session was verified
`python tools/rw4.py --pkg 45 --rec 1456` → `VERDICT: valid RW4 header` with the fields above.
`tools/find_imm.py 0x34575289` → 0 raw occurrences of the magic as an immediate (it is copied by
memcpy from the template, not compared inline - which is *why* a raw immediate search finds no
reader: the reader compares via the template, so it must be found by call graph, not by magic).
`tools/callers.py 0x11e3f60` → 2 callers, one at `0x006c24b3` (the writer). Build unchanged
(no engine code touched this session); tool run only.

Reader search, so the next session does not repeat it: `tools/find_imm.py 0x34575289` and
`--ascii w32` → 0 hits; raw byte search of `\x89RW4` / `w32\0` / the marker in `.text` → 0 hits;
`tools/find_imm.py 0xcafed00d` → 2 hits, both inside the writer; `tools/callers.py 0x11e3f60` → 2
callers; `tools/callers.py 0x004021a0` (the rw4 type accessor) → 3 callers, all in one function
(`0x0040246b`). None of these is the stream reader, so the reader is reached another way - walk
from the resource loader that dispatches on type `0x2F4E681B`, not from the magic. This is a
**negative result worth keeping**: it rules out the obvious searches.

---

## Session 7 — 2026-10-13: the whole game enumerated and named

Started from: session 6b (RefPack decoded, type ids named).
Ended with: **a global inventory of the entire game — 150,945 resources, 10.27 GB decoded,
112,155 resource names resolved** — shown on the Loader screen, plus a real bug fixed in kind-7
type reading.

### The result
`python tools/resource_inventory.py` and the in-engine `inventory` readout both report:
```
150,945 resources across 57 packages, 10.27 GB decoded
instance names resolved: 112,155/150,945 (74.3%)
distinct types: 90; named: 86; unnamed ids: 4
```
Top types by count: `prop 30,645 · snr 29,400 · soundProp 19,286 · png 14,549 · rw4 12,987 ·
gmdl 11,041 · raster 5,931 · animation 4,997 · blockdata 3,141 · locale 2,944 · bem 2,621 ·
pollen_metadata 2,598 · summary 2,590 · gmsh 2,535`. That is the shape of the whole game, from
its own indexes, with no wiki involved.

### How names resolve (and what that proves)
Group/instance ids are 32-bit hashes; they resolve because the game's hash is **FNV-1 with
tolower** — the same function identified by disassembly at `0x00932e50` and used for state ids.
Verified on cs-cz NameGeneration: **14/14** instance ids resolve (`LexiconPeople_CS`,
`LexiconCities_CS`, `LexiconPlanets_CS`, ...). This is the second independent confirmation of the
FNV variant, now on the resource side rather than the state side.

Added `source/engine/naming.*`: loads `reg_file.txt`/`reg_type.txt` at runtime (walks up from
cwd, so the built exe finds the repo copy), never guesses — an unknown id stays hex. New verb
`pkg:-path <pkg> <rec>` prints `group/instance.type` (e.g. `LexiconPlanets_CS.trigger`), and
`pkg:-rec` shows the instance name inline.

### A real bug found and fixed
`kind` 7 masks out BOTH `type` and `group`, so a kind-7 record's word[0] is the **instance**, not
the type. Our `Record::type()` read `raw[i_type]` unconditionally, so for every kind-7 package
(type ~2,900+2,590+... of them) `pkg:-types` printed names computed from the *instance* value -
wrong names, silently. Now `type()`/`group()`/`unknown()` fall back to the index prefix when the
field is shared. Same correction applied to `tools/pkg_kinds.py` (kind 7's dict no longer claims
a `typeid` word at 0). This is exactly the class of bug the field guide warns about: a wrong
field interpreted confidently produces plausible-looking output.

### Measurements
* Resources enumerated: 150,945 (each verified against `offset+packedSize <= index_offset`).
* Decoded bytes: 10,273,231,284 (10.27 GB) - the sum of every record's `memSize`.
* Names resolved: 112,155 (74.3%). Unresolved stay as hex ids; 4 type ids are still unnamed.
* fps: 33.3 with the inventory on screen.
* Build: 0 errors, 0 warnings.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
`python tools/resource_inventory.py --top 32` → the table above.
`OpenSpore.exe -w -devDirs -capture hero2.bmp -captureAfter 30 -quitAfter 50` →
`inventory: 150945 resources, 10273231284 decoded bytes, 112155 instances named`,
`packages: 57 index-parsed, 0 kind-unknown, 0 invalid`, `fps=33.3`, exit code 0;
`analysis/shots/hero_inventory.png` shows the inventory on the Loader screen.
`-exec pkg:-path,0,2` → `LexiconPlanets_CS.trigger`; `-exec pkg:-rec,0,2` →
`type=0x024a0e52 (trigger) inst=0x35e6c1a9 (LexiconPlanets_CS)`.

### RenderWare 4 (started, not finished)
The rw4 resources (0x2F4E681B, 12,987 of them) are **RenderWare 4**, not the RW 3.x chunk model
the field guide's §3 playbook assumes: a decoded rw4 payload starts `89 52 57 34 77 33 32 00
0d 0a 1a 0a` ("\x89RW4w32\r\n\x1a\n") with version `454`. There is an embedded source path
`\Spore\SporeEP1ML\Core\RenderWare\platform\src\graphics\core\device\target\dx9\rwgdrvgraph.cpp`,
and the only RW text in the binary is a `vb` console command. So RW4 needs its own format work
(its stream reader), which is the next large item; `tools/rw_scan.py` is in place to walk it.

---

## Session 6b — type ids get NAMES (the decoded data becomes readable)

RefPack gave us bytes; this gives them meaning. Added `tools/gen_typeids.py`, which turns
`upstream/SporeMaster3/data/reg_type.txt` into `source/engine/typeids.inc` (450 ids, sorted,
binary-searched, 139 duplicate-id aliases skipped). Wired in:
- `pkg:-rec` now prints `type=0x024a0e52 (trigger)` instead of a bare number;
- new `pkg:-types <i>` prints what a package contains, by name, sorted by count.

First real payload identified: `Data\Locale\cs-cz\NameGeneration.package` holds
**13 x `trigger` (0x024A0E52)** plus **1 x `prop` (0x00B1B104)** — so the Czech given-name and
toponym lists we decoded are `trigger` resources. That is the chain working end to end:
DBPF index -> RefPack decode -> type name.

Verified: clean build (0 errors, 0 warnings);
`-exec pkg:-types,0` -> `NameGeneration.package: 14 records, 64320 decoded bytes, 13 refpack`
followed by `trigger 0x024a0e52 x13` / `prop 0x00b1b104 x1`; `pkg:-rec` shows the name inline.
(The registry is a lookup, not a hash — type ids are assigned, confirmed again here.)

---

## Session 6 — 2026-10-13 morning: RefPack solved, first real game content decoded

Started from: session 5 (57/57 indexes parsed, 150,945 records, but nothing decoded; RefPack
explicitly UNVERIFIED).
Ended with: **RefPack fully reversed from the instructions, verified on 30,000 real records,
integrated in C++, and the first real Spore content decoded end-to-end.**

### The two facts that solved it (both instruction-level, both easy to get wrong again)
1. **Copy-loop count = register value + 1.** The copy loops test the counter BEFORE decrementing:
   `mov eax,esi / dec esi / test eax,eax / jne` runs N+1 times for esi=N, while the *literal*
   loops test after (`dec esi / test esi,esi / jne`) and run exactly N. So the copy lengths are
   `+3/+4/+5`, not `+2/+3/+4`. My first port used the raw value and failed **250/250** streams;
   with +1 it is exact on **30,000/30,000**. This is the kind of thing that looks right when you
   read the code and is wrong when you run it - the exactness test is what caught it.
2. **Two valid signatures.** The detector masks: `bswap(word) & 0x1FFF == 0x10FB`. Top bits are
   flags, so `0x10FB` AND `0x50FB` pass. A strict `data[0]==0x10` check rejected 50 of the
   first 20,000 real records, every one a `0x50FB`.
(The header size is BIG-endian - the upstream doc's "little-endian" is wrong, third time that
document misled us and the third time the disassembly corrected it.)

### Added
* `source/engine/refpack.h/.cpp` - the C++ decoder, with the instructions quoted inline.
* `Package::read_resource_decoded()` - reads a record and decompresses it when the index says
  RefPack; `out.size()` == `memSize` on success.
* `pkg:-decode <pkg> <rec> <out>` console verb (reports bytes, mem/packed, % printable).
* `tools/refpack.py` rewritten around the disassembly (no third-party table), with
  `--verify` that decodes real records and requires an exact length match.
* `tools/rp_trace.py` (per-command trace), `tools/rp_probe.py` (variant comparison),
  `tools/rp_rules.py` (rule search), `tools/rp_brute.py` (enumeration) - the instrument set
  used to localize the discrepancy rather than guess.

### First real game content decoded
`pkg:-decode,0,0` → 312 B, `pkg:-decode,0,2` → 10,191 B, both exactly `memSize`, from
`Data\Locale\cs-cz\NameGeneration.package`: UTF-8 (BOM `EF BB BF`) Czech given names
(`Ábel, Adam, Bára, Běla, Emil, Gábi, …`) and toponyms (`Abélard, Aberdeen, Adrásteia,
Agamemnón, …`). The loader can now read the game's own text.

### Measurements
* RefPack: **30,000 records decoded exactly, 0 failures** (was: 0 decoded, decoder faulted).
* Records indexed: 150,945 (unchanged); of them 98,407 are RefPack and now decodable.
* Build: 0 errors, 0 warnings.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
`python tools/refpack.py --verify --limit 30000` → `records decoded successfully: 30,000`,
`FAILED: 0`, `VERDICT: PASS`.
`OpenSpore.exe -w -devDirs -exec pkg:-decode,0,0,dec0.bin -exec pkg:-decode,0,2,dec2.bin
 -quitAfter 12` → `decoded 312 bytes (mem=312 packed=253 refpack=1, 94% printable)` and
`decoded 10191 bytes (mem=10191 packed=6367 refpack=1, 89% printable)`, both files on disk with
exactly those sizes, exit code 0, and their contents are the Czech lexicon above.

---

## Session 5 — 2026-10-13 00:00: upstream RE knowledge adopted, tested, and two of our bugs fixed

Started from: session 4 (57 packages opening, but only 31 with a parsed index; `sentinel` and
`[1]` unexplained; FNV variant unverified).
Ended with: **all 57 packages' indexes parsed**, a corrected hash, a corrected index model, the
first README, and our own upstream copy.

### The user pointed us at `E:\_DPT_Projects_E\_Harness\SporeMaster3`
Another team's from-scratch RE of the same game. Per the authority order that is **level 4**
(hints and names, never values), so nothing was copied blind: every claim was tested against
either the oracle's disassembly (level 1) or our 57 shipped packages (level 2) first.

Copied into the repo (tracked, ~9.8 MB, `upstream/SporeMaster3/`): 21 format docs, the four
registries (`reg_type` 593 lines, `reg_file` 2.8 MB, `reg_property`, `reg_simulator`),
`resources.csv` (6,005 resource rows) and the beta `KeyMap.txt`. Deliberately NOT copied:
their 20 MB of hash-guesser dictionaries — tooling, not format knowledge.

### What their model got right (adopted, and it fixed our code)
* **Index dword[0] is a shared-field bit mask**, not a kind selector: bit0 `type`, bit1
  `group`, bit2 `unknown` are hoisted into the prefix when set. Therefore
  `prefix = 4 + 4*popcount(K)`, `stride = 32 - 4*popcount(K)`, and a full record is 32 bytes.
  Our previous `prefix = 4+K / stride = 32-K` fitted K=4 and K=7 **but not K=6** (it predicted
  10 where the measured prefix is 12). This is the correction that made 26 more packages parse.
* **The last u32 is `compression:u16 + flags:u16`**, not a magic sentinel. Split, the two
  observed values explain themselves completely, and we verified the split by probing the
  payload's first bytes at every record offset across all 57 packages:
  `0xFFFF` → starts `10 FB` in 100% of 98,407 samples with `packed < mem`;
  `0x0000` → starts `10 FB` in 0% of 52,538 samples with `packed == mem`.
  So `0x80000000` on the size word is a validity bit and **compression is what toggles** —
  which also disproves our own session-4 note that the resource we dumped was plaintext.
* Their `resources.csv` column model (`type,group,instance,compressed,packedSize,memSize`)
  matched our records exactly once the u32 was split.

### What they got wrong (NOT adopted, recorded in IDEAS.md §5)
* **"RefPack size is little-endian"** — it is BIG-endian. `10 fb 00 08 48` → BE = 2120 = that
  record's `memSize`; LE = 4,720,640 for a 1371-byte payload. Confirmed independently in the
  oracle: `0x0092ccdc` reads the length through the BE helper `0x0092c4e0`.
* **"A registry line without a hash means ID = FNV(name)"** — false for type ids: 6/6 pairs
  from `reg_type.txt` (`rw4`=0x2f4e681b, `prop`, `soundProp`, `gmdl`, `plt`,
  `creature_traits`) fail to match FNV-1 or FNV-1a in raw/lower/upper. Type ids are assigned.
  (Their own hash-guesser doc admits ~9% Top-1 accuracy, so name-by-hash is weak; KeyMap is
  the real answer.)

### Two real bugs of ours, found while checking them
1. **Our FNV was the wrong variant.** `common.h` implemented FNV-1a with uppercase folding.
   The oracle's hash at `0x00932e50` is `imul edi,edi,0x1000193` **then** `xor edi,eax` — FNV-1
   — and arg3 selects folding via the CRT: mode 0 raw, 1 `tolower` (IAT slot 0x013cc2ec),
   2 `toupper` (0x013cc500), both verified with pefile. The state-registration site
   `0x007e40f2` pushes `(name, 0x811c9dc5, 1)`, so a game state id is **FNV-1 + tolower**.
   `Loader` was `0x69cdcb22`, now `0xd7d04c04`. Silent by construction: ids only ever compare
   to other ids, so our own `state -list` and `goto` kept working throughout.
2. **`pkg:-list` said 109 while the Loader said 57** for the same tree. The session-4 dedupe
   lived only in the Loader. Moved into one `all_packages()` helper shared by every `pkg:`
   subcommand. Two views of one dataset disagreeing by 2x is exactly the kind of thing a
   yardstick is supposed to catch; now both say 57.

### Also this session
* `tools/refpack.py` written, and left explicitly **UNVERIFIED**: its command table came from
  the third-party doc, and it still faults on a real stream (offset 522 at output length 20).
  The verified next step is recorded in IDEAS.md §6 — disassemble `0x0092c5b0`, not guess again.
* RefPack entry points confirmed in the oracle: `0x0092ccdc` (header: tests bit 0x8000 to pick
  a 3- or 4-byte BE length), `0x0092cd40` (detector: `bswap(word) & 0x1FFF == 0x10FB`),
  `0x0092c4e0` (BE reader, exactly one caller: `0x92cd05`).
* **First README.md**, with `analysis/shots/hero.png` (in-engine capture of the Loader listing
  all 57 real packages) as the proof-of-concept image.
* `IDEAS.md` restructured after a bad edit of mine lost the §5 heading and put items 16-21
  inside §4; §6 (open questions, precisely stated) added so the next disasm run is aimed.

### Measurements
* Packages with a **parsed index**: 31 → **57 / 57** (0 kind-unknown, 0 invalid).
* Records: 17,901 → **150,945**, still 0 bounds violations, 0 `usize<size` violations.
* State id `Loader`: `0x69cdcb22` (wrong variant) → **`0xd7d04c04`** (matches the original).
* `pkg:-list` and the Loader agree on **57** (was 109 vs 57).
* Build: 0 errors, 0 warnings. Exit code 0 on every run this session.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
`OpenSpore.exe -w -devDirs -exec pkg:-list -exec pkg:-hdr,0 -exec pkg:-rec,0,3
 -exec pkg:-dump,0,3,res0.bin -quitAfter 12` →
`packages: 57 index-parsed, 0 kind-unknown, 0 invalid`, `-> packages (57):`,
`rec[3]: type=0x024a0e52 inst=0x65e81724 off=0x00004c11 packed=1371 mem=2120 REFPACK
flags=0x0001`, `dumped 1371 bytes -> res0.bin` (file present, 1371 B), `clean exit code=0`.
`-exec state:-list` → `Loader [0xd7d04c04] <- current`.
Record census: `python tools/pkg_kinds.py` → kinds {6:30, 7:18, 4:9}, 150,945 records,
0 formula / 0 bounds / 0 usize violations, sentinels {131071: 98407, 65536: 52538}.
Upstream claims: the BE/LE and FNV tests are reproducible with the snippets in MEMORY.md
"Upstream knowledge" and IDEAS.md §5 items 19-20.

---

## Session 4 — 2026-10-12 late night: first contact with EA's shipped data

Started from: session 3b (assets arrived, DBPF reader located by disasm).
Ended with: **109 real `.package` files opened and validated, 0 rejected**, and the
record layout measured over 17,901 records. Plus a 7× fps regression found and fixed.

### Added
| file | job |
|---|---|
| `source/engine/pkg.h/.cpp` | DBPF reader: header replay of the validator's own clauses, fixed-stride index parse, per-record bounds clamp |
| `tools/pkg_record_test.py` | test the 24-byte record hypothesis across every fixed-stride file |
| `tools/log_dupes.py` | count duplicate package opens in a run log (the tool that caught the double-scan) |
| `analysis/shots/loader_packages.png` | loader screen listing the real packages |

### Reversal landed (VERIFY rows 18-21, MEMORY "DBPF package format")
- Header = 0x60 bytes; fields `+0x00` magic, `+0x04` version ≤ 3, `+0x24` count < 0x07FFFFFF,
  `+0x2c` index size, `+0x3c` non-zero, `+0x40` index offset — all quoted from the validator
  at `0x008d90b0`, and the reader **replays the same clauses in the same order with the same
  rejection reasons** rather than inventing its own.
- Record = 6 u32: `[2]`=offset, `[3]`=size|0x80000000 confirmed with **zero violations** in
  17,901 records; `[0]`=type id (16 distinct, one dominating at 98%), `[1]`=instance id,
  `[4]` ≥ masked `[3]` in all records (probable uncompressed size), `[5]` ∈ {0x10000, 0x1FFFF}.
- 26/57 files are var-width and fit `idx_size = 16 + 20*count` exactly. **Reported as
  VAR-WIDTH, never guessed**: the parser for them is the next disasm target (the index
  container at `[pkg+0x2d0]`, vtable+0x24/0x28/0x38/0x40, fed from `0x008d9a90`).
- The header loader's failure branch (`0x8d9053` → `0x008dd820`, 17-byte header, magic
  `{0x7f,0x9d,0x88,0xec}`) is a *fallback* parser, dead for all our files. Recorded so nobody
  mistakes it for the main path.

### Bugs found and fixed (each by measurement, not by reading)
1. **Double scan**: the dev search list contains both `Data=<exe>\Data` and `cwd=<exe>`, so
   the recursive scan opened every file twice — 62 "opened" lines for 31 distinct files.
   Fixed by deduping on `weakly_canonical`. After: **31 fixed + 26 var-width = 57**, which
   matches `tools/pkg_records.py` offline over the same tree. Two independent routes to the
   same number, which is the confirmation we want rather than a log that merely looks right.
2. **fps 30 → 4** when the loader began drawing the 57-line package list: `flush_text` issued
   one `DrawPrimitiveUP` per quad, i.e. ~3400 draw calls/frame. Fixed by emitting 6 verts per
   glyph (triangle list) and flushing the whole batch in ONE call; vertex buffer 1024 → 4096.
   After: **33.4 / 31.2 fps** with the same package list on screen.
3. **Ledger swallowed the package listing** (all 109 lines share one call site → "x9, further
   repeats suppressed"). Those lines are one-per-run, so they use `LOG_ONCE_*`.
4. **`C4018` signed/unsigned** on `it.depth() > (unsigned)max_depth` — the cast was the bug,
   not the comparison.

### False alarm worth recording
`exit=0xFFFFFFFF` appeared once after the dedupe run. The log said `clean exit code=0`.
Cause: reading `$p.ExitCode` in a race with `WaitForExit`, not an app bug. Using
`[void]$p.WaitForExit(40000)` first gives `code=0` consistently. Harness lesson: a PowerShell
race looks exactly like a crash, so check the log before believing the exit code.

### Yardsticks
- Packages opened: **109 lines → 57 distinct files, 0 invalid** (was: unmeasurable, no reader).
- Records parsed: **17,901** across 31 files, 0 invariant violations.
- fps: 30 → 4 (regression) → **33.4** (fixed, with more on screen than before).
- Boot markers: still **10/10**. Build warnings: **0**.

### How this session was verified
`cmake --build build --config Debug` → 0 errors, 0 warnings.
`OpenSpore.exe -w -devDirs -capture cap.bmp -captureAfter 40 -quitAfter 90` →
`packages: 31 fixed-stride, 26 var-width, 0 invalid`, `yardstick: 10/10 markers OK`,
`fps=33.4 frames=34 aborts=0`, `fps=31.2 frames=66 aborts=0`, `clean exit code=0`;
`cap.bmp` 3,686,454 B, `nonblack=9502/9502 (100.0%)`; `tools/log_dupes.py` → 0 duplicates.
Record census: `python tools/pkg_record_test.py` → 31 files, 17,901 records,
`[2] offset >= index_offset: 0 (OK)`, `[2]+[3] beyond index_offset: 0 (OK)`.

---

## Session 3b — 2026-10-12 night: machine evidence, fault tolerance proven, assets arrived

### Added
- **In-engine frame capture** (`Device::capture_frame` + `request_capture`): `GetBackBuffer`
  → `GetRenderTargetData` → SYSTEMMEM → `LockRect` → 32bpp BMP, executed between `EndScene`
  and `Present`. Launched by `-capture <path> -captureAfter <n>` or the `capture` verb. Logs
  pixel coverage so a blank frame is detectable without looking at it.
- **Unattended harness**: `-exec <cmds>` (repeatable, `;`-separated), `-execAfter <n>`,
  `-quitAfter <n>`, and `name:arg` so no shell quoting is involved. `Args::all()`.
- **Fault-tolerance test**: `_abortNow <n>` arms one abort per frame *inside* `run_guarded`.
- **Atlas health check**: ink coverage logged, error if 0% or >90%.

### Fixed (each found by running, not by reading)
- MSVC `signal()` resets to SIG_DFL per invocation → the 2nd abort exited `3` unhandled.
  Re-armed inside the handler. (`MEMORY.md` session-3b trap 1.)
- Capture hung the process when called with the scene open → moved between EndScene/Present.
- `GetFrontBufferData` is invalid under `D3DSWAPEFFECT_DISCARD` → route 2 (`GetBackBuffer`).
- `ETO_OPAQUE` with a white background made the font atlas solid → black background; text was
  white bars in two consecutive captures before this was understood.
- Glyph advance ≠ cell width (16×16 cell, 9.6 px advance) → overlap; now cell = font metric
  (7×15) and advance = cell width, POINT filtering.
- Command names not lower-cased at registration → `logLevel`/`_abortNow` unreachable.
- Log fully buffered after the last forced flush → a healthy frame loop looked hung; now
  flush every line.
- Guessed APIs that do not exist: `GetTextMetricW` (→ `GetTextMetricsW`), `ETO_CLAMPED`,
  `IDirect3DSurface9::GetSurfaceDesc` (→ `GetDesc`).

### Measurements (all from runs, not reasoning)
- Capture: `cap.bmp` 3,686,454 B = 1280×720×4+54, `nonblack=9502/9502 (100.0%)`, and
  `analysis/shots/loader_capture.png` shows readable text (title, mode line, `state: Loader`,
  search paths, 28% bar, `frames: 20 t=0.70s log lines: 32`). **This replaces the
  human-only "я видел полоски загрузки" evidence.**
- Fault tolerance: `-exec _abortNow:9` → hits #1..#5 intercepted inside `run_guarded` with
  symbolised stacks, loop kept counting frames; hit #6 within 1 s → storm exit `0x4F53AB02`.
  Single abort → `frames=40 aborts=1`, `clean exit code=0`.
- Regression A (`-w -devDirs -capture -quitAfter 50`): `yardstick: 10/10 markers OK`,
  `frames=50 aborts=0 asserts=0`, exit 0.
- Regression C (`-devDirs -shipDirs -exec 'state -list'`): `useDevDirs=0`,
  `search mode=SHIP, 3 paths`, `Current state: 'Loader'`.
- Build: 0 errors, 0 warnings.

### Assets arrived (changes the roadmap)
User placed the real game data in `build\bin\Debug\`: `Data\` (base game, 7 `.package`,
incl. `Spore_Game.package` 20,210,108 B) and `DataEP1\` (GA, 5 `.package` + `Config`),
~5.7 GB total. Correction recorded: the extension is **`.package`**, not `.package.big`
(`.big` is only EA's internal name for the archives). `fs.cpp` already resolves them.
Top priority is now the `.package` header reversal, from the file's own bytes (authority #2)
cross-checked against the original's reader (authority #1).

### Decision recorded: do not install the June-2010 DX SDK
The user offered to install it. Answer: no. The original imports exactly 9 `d3dx9_27`
functions, 7 of which are trivial matrix/vector math and 2 are shader-compile helpers now
served by `d3dcompiler` (already present). We call no D3DX function; the atlas uses a
staging texture + device `UpdateSurface`. Installing an old SDK adds header friction with
MSVC 19.44 for no gain, and the planned internal RHI wrapper (`IDEAS.md` §1) is where any
D3DX-shaped need gets absorbed.

---

## Session 3 — 2026-10-12 evening: first booting executable (Loader state)

Started from: session-2 state (oracle verified, reversal docs written, no build system).
Ended with: `build/bin/Debug/OpenSpore.exe` booting 10/10 markers, window + icon + D3D9
device + font atlas + console + AppStateManager-analog + Loader state, `-w -devDirs`.

### Added
| file | job |
|---|---|
| `CMakeLists.txt` | x86 Debug build, icon+version resource generated into `build/generated`, explicit source list (no globs), no D3DX |
| `source/main.cpp` | `wWinMain` → `diag::early_neutralise()` **first**, then `app::run` |
| `source/app.h/.cpp` | boot chain with numbered markers (the yardstick), window proc, frame loop wrapped in `diag::run_guarded` |
| `source/common.h` | `fnv1a`/`fnv1a_upper` — the original's own state-ID hash |
| `source/engine/log.h/.cpp` | share-readable log + per-site consolidation ledger + `write_once` bypass |
| `source/engine/diag.h/.cpp` | non-fatal asserts, CRT report hook, abort interception, dbghelp stacks, `run_guarded` |
| `source/engine/cmdline.h/.cpp` | the reversed arg contract (`devDirs`/`shipDirs`/`state`/`preload`/`-w`) |
| `source/engine/fs.h/.cpp` | the reversed search paths (`Data/BaseData/Locale/Config/UserData/cwd`), shipDirs wins |
| `source/engine/console.h/.cpp` | command registry, stdin worker thread, `help`/`logLevel`/`echo`/`quit` |
| `source/render/device.h/.cpp` | `Direct3DCreate9` only, original's 4 error strings, 800×600 fallback |
| `source/render/overlay.h/.cpp` | GDI font atlas → staging texture → `IDirect3DDevice9::UpdateSurface` |
| `source/states/statemgr.h/.cpp` | FNV-id states, no-nesting guard, `onEnter/onExit/onTick/onRender/onReload`, `state` verb |
| `source/states/loader.cpp` | our Loader state (progress bar + live device/search-path readout) |
| `assets/OpenSpore.ico` | user's icon, **tracked** (refs/ is gitignored) |
| `tools/shot.ps1` | focus-stealing-free screenshot via `PrintWindow(PW_CLIENTONLY\|PW_RENDERFULLCONTENT)` |
| `tools/xref.py` | string → who-references-it (the workhorse that found AppStateManager) |
| `tools/arg_vocab.py` | enumerate every name pushed to the arg helper: 76 sites → 56 names |
| `PROGRESS.md`, `VERIFY.md`, `IDEAS.md`, `HISTORY.md` | the six-doc set, filled in |

### Reversals landed (details + quoted instructions in `VERIFY.md`)
- `AppStateManager` is real and reachable in the **release** build: `state` console verb with
  `-list^-next^-prev^-reload^`, state IDs = FNV-1a of the name, duplicate-ID rejection,
  "can't nest states or transitions", `-state`/`-preload` launch keys.
- The full arg vocabulary is now a file, not a guess: `analysis/arg_vocab.md`.
- `-w` is **not** in that vocabulary → our windowed flag remains our own convention (row 15,
  DRAFT). Recorded so nobody later "discovers" it and trusts it.

### Bugs found and fixed this session (each with its measurable delta)
1. **UCRT assert in `setvbuf`** — `setvbuf(f, nullptr, _IOLBF, 0)`: NULL buffer means "CRT
   allocates of THIS size", and 0 trips `setvbuf.cpp:57: 2 <= buffer_size_in_bytes`. Symptom:
   modal dialog, exit `0xC0000409` (-1073740791), **empty log**. Fix: explicit 64 KiB +
   `_IOFBF` fallback. Delta: log went from 0 bytes to a full boot trace.
2. **Harness installed too late** — `log::init()` ran before `diag::install()`, so bug 1 had
   nothing to catch it. Fix: `diag::install()` first, plus `early_neutralise()` as the very
   first statement of `wWinMain` (covers CRT startup asserts).
3. **`std::mutex` re-lock in `console::init`** — held `g_mux` while calling `reg()`, which
   locks it again → `future_error` (0xE06D7363). Caught **live by our own harness**: 16-frame
   symbolised stack with file:line, exit `0x4F530000^code`. Fix: clear under the lock, release,
   then register. Standing rule written into `IDEAS.md` §4.
4. **`LockRect` 0x8876086C on the font atlas** — a `D3DPOOL_DEFAULT` texture without
   `D3DUSAGE_DYNAMIC` is not lockable. Fix: SYSTEMMEM staging texture + device `UpdateSurface`
   (signature verified against `shared\d3d9.h:464`; there is no texture-level `UpdateSurface`
   and no `UpdateSourceRect` in this SDK at all — two earlier guesses were wrong).
5. **`Clear()` before `BeginScene()`** — D3D9 silently drops it. Fix: `begin_frame()` returns
   bool, scene state tracked in `m_in_scene`, `end_frame()` only Presents if the scene opened.
6. **`sf.Size` / 7-arg `StackWalk64` / `PDWORD64` displacement / `_fsopen_s` / `NTAPI` report
   hook / `SIZE_MINIMIZE` / `#ifdef` inside macro args / `SearchPath` colliding with the
   `SearchPathW` macro / `/OPT:NO`** — a batch of API-shape mistakes, all compiler-caught, all
   fixed and noted in code comments where they were non-obvious.
7. **Ledger swallowed boot markers** — all 10 markers share one call site, so marker 9 printed
   "x9, further repeats suppressed". Fix: `LOG_ONCE_*` for one-per-run lines. Delta: 10/10
   markers now appear in the log.
8. **`shot.ps1` sampled the bitmap after `Dispose()`** and called `.Dispose()` on a
   `[System.Drawing.Color]` (a struct, no such method). Fixed ordering + removed the call.

### Known-unverified after this session
- **Machine evidence for the loader screen**: `tools/shot.ps1` returns an all-black client
  area (1 colour × 1600 samples) — `PrintWindow` does not capture D3D9 content. The screen is
  human-confirmed only ("О да я видел полоски загрузки"). Next action 1 in `VERIFY.md`.
- **`diag::run_guarded` has never fired.** The abort-recovery path is built but unexercised,
  so "dynamic fault tolerance" is currently a claim, not a result. Next action 2.
- x64 build switch exists in CMake and has never been compiled.
- `-w` semantics (row 15) and the path-assembly block (row 8) are DRAFT/PARTIAL.

### Yardsticks
- `N_verified` (DISASM-VERIFIED rows in `VERIFY.md`): 0 → **3** (rows 1–12 are verified; the
  count of *newly* verified reversal facts this session is 10 rows).
- Boot markers: n/a → **10/10**.
- Build warnings: many → **0**.

### How this session was verified
- `cmake --build build --config Debug` → `-> E:\...\build\bin\Debug\OpenSpore.exe`, no
  `error C`, no `error LNK`, no `warning C`.
- Ran `OpenSpore.exe -w -devDirs` from `build/bin/Debug`; log tail read while alive via
  `[IO.FileOpen]::ReadShare`-style access (`[IO.FileShare]::ReadWrite`).
- Log contains: `yardstick: 10/10 markers OK`, `flags: windowed=1 devDirs=1 shipDirs=0
  useDevDirs=1`, `window 010C0D18 'OpenSpore - Loader' 1296x759 (client 1280x720) windowed=1
  icon=embedded`, `device windowed 1280x720 vsync=1 (desktop 1692x919 fmt=22)`,
  `font atlas ready: cell=16 cols=16 glyphs=95`, `registered 'Loader' id=0x69cdcb22`,
  `-> Loader [0x69cdcb22]`, `enter Loader state`.
- Crash harness proved by an actual crash (bug 3) producing a symbolised stack, not by a test.

---

## Session 2 — 2026-10-12 afternoon: the unencrypted `.text` oracle

### Added
`tools/find_plaintext_text.py`, `tools/compare_builds.py`, `tools/verify_unpacked.py`,
`tools/pdb.py`, `tools/pdb_topology.py`, `tools/show_region.py`, `tools/strings_scan.py`,
`analysis/{plaintext_text,build_identity,unpacked_verification,pdb_topology}.md`.

### Results
- Found plaintext-`.text` builds among the user's new `refs/bins/FindTheUnencryptedDotText`
  and `refs/bins/Proto` drops, then proved that **none** of them is the GA target's link
  (0/12 on CodeView/`.rdata`/`.data`).
- The user then supplied `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, which **is** the
  same link: every section byte-identical except `.text` (decrypted) and the removed `.bind`
  (the EncryptPE loader that held the original entry point 0x01d0e310).
- `verify_unpacked.py` 9/9 PASS with the encrypted `.text` as control; decisive metric is the
  cross-section one (7/7 known `.rdata` strings referenced by decrypted code, 0/7 by control).
- Beta PDB: MSF directory validated (2683 streams / 130338 pages, two-level), but it has **no**
  info magic and **no** named-stream map → stripped/nonstandard, names/topology only.
- Three wrong invariants of my own verifier were recorded as disproven (int3 run *start*
  alignment, raw E8/E9 as a branch census, `FF 25` as the only import form) plus the
  missing-chance-baseline class.

### How it was verified
`python tools/verify_unpacked.py` → all metrics PASS; `analysis/unpacked_verification.md`
written. `python tools/compare_builds.py` → `analysis/build_identity.md`.

---

## Session 1 — 2026-10-12 morning: recon, no code

### Added
`tools/pe.py`, `tools/dump_strings.py`, `tools/recon.py`, `analysis/recon.md`,
`analysis/strings/{rdata,data,const,tls}.txt` (33,793 strings).

### Results
- Target identified from its own bytes: GA, x86, **3.1.0.29**, "SPORE™ Galactic Adventures",
  "Maxis, a division of Electronic Arts Inc.", 22 RT_VERSION blocks, Authenticode **valid**
  (EA, DigiCert G4, timestamped 2024), PE timestamp 2024-09-30, linker 9.0 (MSVCR90).
- CodeView: GUID `abf55581-6487-4b65-b86f-4631a2bdcb02` age 54,
  `c:\BuildAgent\max-spore001-spore\CMBuild\SporeEP1_RL\out\Win32\Release\SporeApp.pdb`
  (PDB not in our possession).
- `.text` entropy 8.000 → encrypted; entry point inside `.bind`.
- Import census → D3D9 (`Direct3DCreate9` only), d3dx9_27 (9 fns), DSOUND #1/#9, DINPUT8,
  WSOCK32/WS2_32, steam_api (3 fns), dbghelp Sym*, IMM32, USP10, GDI video-RAM probe.
- Engine identified as RenderWare on D3D9 → field guide §3 applies.
- `.rdata`/`.data` are plaintext even in the encrypted build, so strings/tables were
  recoverable; `devDirs`/`shipDirs` located (after fixing an alignment-drifting decoder that
  had produced a phantom hit inside a float pool).

### How it was verified
`python tools/recon.py`, `python tools/dump_strings.py`, `python tools/show_region.py
0x013f8dc8` — outputs committed as `analysis/*.md`.
