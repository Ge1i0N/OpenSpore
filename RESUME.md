# RESUME — read this first

Hand-off snapshot. Rewritten at the end of every session, **before** context compaction.
Last update: 2026-10-14 (session 13, end of day — written immediately before compaction).

---

## HAND-OFF, session 13 end (2026-10-14 ~11:00). Read this block, then just start on step 1 below.

**Everything is committed AND pushed.** The last work commit is `5d652a0` ("The EP1 locale text is a
DBPF *inside* a DBPF…"), and this hand-off edit is committed on top of it — so read `git log -1` for
the real HEAD rather than trusting the sha here. 74 commits at the time of writing, and
`git ls-remote origin main` agreed with local HEAD (the remote `github.com/dpteam/OpenSpore` tracks
each commit automatically), so **nothing is waiting on a push**. No background jobs; the Ghidra
project is unlocked.

### The state in one paragraph
Our engine **draws a real Spore UI screen** from shipped data: `source/engine/spui.cpp` reads the 748
`spui` layouts using the dictionary that EA's own UI Designer shipped inside the game (14 `xml`
resources → 306 `proxy` ids), and the Title state renders the game's windows at the game's own
coordinates with the game's own captions ("Create a New Game", "color picker", "Selected Color",
"Pick A Star For Your New Game"). The layout→text chain is closed: **1,391 of 1,426 caption
references (97.5%) resolve to real strings**, because the EP1 text turned out to live in **nested
packages** (a DBPF inside a DBPF) that nothing had ever opened. `analysis/shots/spui_real.png`.

### What the remaining 1 FAIL and 2 UNKNOWN actually are (all the SAME cause — not wrong claims)
All three are the **vtable-only / uncovered-bytes wall**, not a reversal error:
* `spui.loading_path` FAILs because Ghidra has carved **no function at `0x00810940`** (the spui loader).
  It is reachable only through a vtable, so auto-analysis never reaches it. `0x00810940` is already in
  `tools/gen_seeds.py`'s CITED list; clearing the FAIL needs one seed + one analyze pass (~9 min):
  ```
  $env:GHIDRA_INSTALL_DIR = "C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921"
  refs\ghidra\venv\Scripts\python.exe tools\gen_seeds.py
  refs\ghidra\venv\Scripts\python.exe tools\ghidra_seed.py
  & "$env:GHIDRA_INSTALL_DIR\support\analyzeHeadless.bat" E:\_DPT_Projects_E\_Harness\OpenSpore\refs\ghidra\proj OpenSporeOracle -process SporeApp.exe.unpacked.exe -analysisTimeoutPerFile 7200
  python tools\ghidra_audit.py request
  refs\ghidra\venv\Scripts\python.exe tools\ghidra_export.py
  python tools\ghidra_audit.py verify
  ```
  **Serialize these** — two jobs on the same project produce `LockException` (it cost me three runs).
* `event.dispatcher` and `refpack.decoder` are UNKNOWN because their bodies contain **gaps Ghidra
  never disassembled** (`0x7e71c5..0x7e72be` = 250 B; `0x92c62d..2f` and `0x92c6ed..ef` = 3 B each).
  The auditor now correctly reports UNKNOWN instead of a false FAIL when the body is non-contiguous —
  that is the mechanism working, not a defect.

### Reproduce the whole picture in one command
```
cd build\bin\Debug
.\OpenSpore.exe -headless -devDirs -quitAfter 8 `
  -exec gamestates:-entry -exec spui:-caps -exec txt:0x8B1A2341:0x077427D2
```
**`-quitAfter N` is mandatory for an unattended run** — without it the engine loops forever and a
killed process reports `0xFFFFFFFF`, which looks like a crash but is not (verified: same three lines
below either way, yardstick 10/10 in both).

Expected: `-> Main (explicit -id 0), description: "Spore Launch Screen"`; `captions: 2152 inline,
1426 references (1391 resolve ..., 35 do not)`; `"Accept Reward"`. Yardstick 10/10, exit 0, 0 warnings.

### Numbers that are currently true (all measured, all reproducible)
| thing | value |
|---|---|
| boot markers | 10/10, exit 0, 0 build warnings |
| packages / resources | 57 / 150,945 (unchanged by any of today's refactors) |
| game states recovered | **60 states, 17 modes**, entry `Main -id 0` → `GalaxyGameEntry` |
| locale corpus | **369 tables / 37,608 entries / 14,657 keys** (was 197/18,441/10,246 before nested) |
| spui layouts parsed | **748/748, 22,384 windows, 18,232 with a usable Area, 961,360 anchors** |
| designer schema | **306 ids** (66 Class + 240 Property), 227 present in the binaries |
| our own reversal audit | **29 PASS / 1 FAIL / 2 UNKNOWN / 1 SKIP** over 33 claims / 131 checks |
| Ghidra functions carved | 55,294 (54,611 before seeding; the 7 seeds cascaded to +683) |
| feature map | engine 82.2%, overall 75.7% (261 features: 185 done / 25 partial / 51 todo) |

### THE NEXT STEP, precise and small
**Mount the nested packages for ALL languages and finish the caption chain, then start composing a
real screen.** Concretely, in order:

1. **`text.cpp`'s nested pass currently matches `instance == fnv1_lower(lang)`.** That worked for
   en-us, but verify it for the other 14 languages before trusting it (one line of python, and
   `tools/nested_langs.py` already prints the whole mapping — 21 nested packages, one per language).
   If any language's nested instance is NOT its name hash, the rule needs a fallback.
2. **The 35 unresolved caption references.** Two tables (`mvj` 0x277DEE6A, `legal` 0xADB810C4) are
   absent from our 57 packages even at nested depth. Check whether they are in a package we do not
   have at all (the tree has 57; the retail game ships more) — and if so, record it as a coverage gap,
   not a format gap. `tools/spui_caption_gaps.py` is the classifier.
3. **Then the visible goal: make Title draw a CHOOSABLE screen.** `spui::Reader::best_screen()` picks
   the layout with the most usable Areas. The state script names the real ones
   (`uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e`), so the honest next feature is:
   `-state:Main` → load the layout the state's own `cheat` names → draw THAT, instead of the biggest.
   This is the smallest step that turns "we can render a layout" into "the state machine drives the UI",
   which is the actual Main→playable path.
4. After that: the **`mode` table** (all 60 states log `mode X not executed`), then gameplay.

### Three traps I fell into TODAY — do not repeat them
1. **Double-visit.** The dev search list has both `Data=<exe>\Data` and `cwd=<exe>`, so a recursive
   `find_packages` visits every package TWICE. I hit this twice in one session (`spui` reported 1,349
   layouts where the corpus has 748; `appstates` listed 5 copies of a script that exists 3 times) even
   though the `pkg` verb has carried the `weakly_canonical` dedupe for weeks and MEMORY.md documents it.
   **Any new package scan must dedupe by canonical path.**
2. **Container depth.** "The data does not exist" was wrong THREE times this session, in three
   different shapes: wrong directory (`exe_dir/Locale` vs `Data/Locale`), wrong id field
   (`SporeAppStates` is an INSTANCE, not a group), wrong depth (the EP1 text is a DBPF inside a DBPF).
   **Before concluding data is missing, enumerate: all types, all id fields, all depths.**
3. **A FAIL from a tool is a hypothesis about the TOOL.** 12 of the 13 audit FAILs were bugs in my own
   checker, not in our reversal (`Memory.getInt(a, TRUE)` is big-endian; `JArray("b")` needs the
   `JByte` class; Ghidra prints `JZ` where objdump prints `je`; a function body can be
   non-contiguous, so "absent" meant "not looked at"; a function-pointer handoff is invisible to a
   call-list check). **Read the evidence, never act on the verdict alone.**

### Toolchain, if you need the binary again
* **REA is primary** for interactive questions: `rea decompile <oracle> 0x810940 --provider ghidra --json`.
  It is registered in `C:\Users\DartPower\.config\opencode\opencode.json` with the right environment.
  **`GHIDRA_INSTALL_DIR` must be `C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921`, NOT
  `...\current`** — `current` is a junction and REA's Windows admission rejects reparse points, while
  `rea doctor` still reports healthy. Also needs `REA_GHIDRA_STARTUP_TIMEOUT_MS=5400000` (our import
  takes ~1,200 s vs a 330 s default) and `GHIDRA_HEADLESS_MAXMEM=4G`.
* **Our PyGhidra bridge is the persistent-project path** (`refs/ghidra/proj/OpenSporeOracle`, venv in
  `refs/ghidra/venv`, both gitignored): `tools/ghidra_export.py`, `tools/ghidra_audit.py`
  (request/verify), `tools/ghidra_decompile.py <addr>` (fast, seconds, vs a 20-min REA re-import),
  `tools/gen_seeds.py`, `tools/ghidra_seed.py`. Use it for BATCH audits and for anything needing
  accumulated state, because REA re-imports from scratch every session.
* **Serialize jobs that share the Ghidra project** — I lost three runs to `LockException` by firing
  them concurrently. Identify a process by COMMAND LINE before killing it (I once killed a live
  15-minute REA import because it was the only `java` around).
* Never disassemble the encrypted retail `SporeApp.exe`; the oracle is
  `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`.

### Files this session produced (all committed, all referenced above)
`analysis/spui-schema.md` (the format, with the bit-packing RETRACTION), `analysis/spui-loading.md`
(the disassembly door), `analysis/states.md`, `analysis/text-locale.md`, `analysis/audit-review.md`,
`analysis/toolchain-rea.md`, `analysis/jev-decisions-api.md`, `analysis/claims.json` +
`analysis/audit.md`, `analysis/scripts/` (the recovered ArgScript + designer XML),
`source/engine/{spui,text,appstates,pkg}.*`, `source/engine/spui_schema.inc` (generated),
`tools/{spui_*,nested_*,gen_spui_schema,ghidra_*,find_ep1_locale,what_in_ep1_locale}.py`.

---

## METHOD RULE: go by Main, forward, to a playable build

Disassembly is the first tool. The **path of work** is also fixed (user, session 13): **walk the
original from the entry point (Main) forward**, rebuilding the boot/load chain in order, and at each
step reverse **only as much as is needed to reach the next step**. Discover features/formats as we
go; note the unfinished ones and keep moving (finish one early only if it is the smallest step that
unblocks progress). The goal: the whole path from Main to a fully playable build with all features.
Two "how done" axes, report both: how much of a format is reversed, and how far along Main->playable
we are.

## The Main -> playable path (where we stand)

| step | status | evidence |
|---|---|---|
| entry `0x11e11e1` -> CRT -> `__tmainCRTStartup 0x11e0f21` | ✅ | `analysis/startup.md` |
| WinMain `0xf489e0` -> app_main `0xf48810` -> run `0xf47e90` | ✅ | VERIFY 35-37 |
| AppMode/config object `0x7e8a00` (vtbl `0x1413b50`) + its subsystems | ✅ | VERIFY 37-38 |
| window class `0x849b20`, Create `0x849bf0`, **main loop `0x8482f0`** | ✅ | VERIFY 39-40 |
| config: `Config/*.txt` (Properties/ConfigManager/VideoCards) | ✅ | `source/engine/config.cpp` |
| content load: DBPF -> RefPack -> decode (`pkg:-*`) | ✅ | 150,945 resources |
| draw real content: `gmdl` + `raster` mesh grid | ✅ | `analysis/shots/model_grid.png` |
| resource manager `0x8de4b0`, `trigger` configs | 🚧 | `analysis/content-loading.md` |
| **localized text: `locale` tables (18,441 entries, 10,246 keys)** | ✅ | `source/engine/text.cpp`, `analysis/text-locale.md` |
| **UI/menus: `spui` layouts READ and DRAWN** | ✅ | `source/engine/spui.cpp`, `analysis/spui-schema.md`, `analysis/shots/spui_real.png` — 400 layouts, 11,465 windows, 9,547 with a usable Area, real captions ("Create a New Game", "color picker") |
| states: mechanism + a real `Title` state that transitions to `Loader` | ✅ | `source/states/title.cpp`, `analysis/shots/title_state.png` |
| **states: the game's own state machine (60 states, 17 modes)** | ✅ **RECOVERED FROM DATA** | `analysis/states.md`, `analysis/scripts/SporeAppStates.txt` |
| **states: the game's table DRIVES our engine** | ✅ | `-state:Main` → `Main [0x0]` → `GalaxyGameEntry [0x02ccda9c]`; `source/engine/appstates.cpp` |
| states: `-id` semantics (explicit id REPLACES the name hash; names resolve as STRINGS) | ✅ | disasm `0x7e40d6`/`0x7e40e8`/`0x7e4103`; `analysis/states.md` |
| launch key `-state:Name` (colon, as the original) | ✅ | `cmdline.cpp`; the shipped script's header confirms it **including the colon** |
| **audit of our own reversal, from WinMain forward** | ✅ **29 PASS / 0 FAIL / 3 UNKNOWN / 1 SKIP** | `analysis/audit-review.md`, `analysis/claims.json` (33 claims / 131 checks) |
| gameplay (sim, editors, stages) | ⬜ | feature map `gameplay` = 0% |

**The state machine was DATA ALL ALONG, and I had it wrong for three sessions.** `RESUME.md` said the
state names were "data we do not have" because `SporeAppStates` (`0xD6396913`) appeared nowhere as a
resource GROUP. It is an **INSTANCE** id in group 0 — I had probed one of a DBPF record's three id
fields. The fix was not a cleverer guess but enumeration: dump all 303 `trigger` resources with their
group AND instance names. Result: 8,499 decoded bytes of plain ArgScript → **60 states, 17 modes**,
entry `state Main -id 0` ("Spore Launch Screen") → `gotoState GalaxyGameEntry` (`-id 0x02ccda9c`). The
five stages Cell → Creature → Tribe → Civ → Space are explicit data.

**Built this session, and it was the visible goal:** the Loader/Title screen composes a REAL layout.
`source/engine/spui.{h,cpp}` + the generated `source/engine/spui_schema.inc` (from
`tools/gen_spui_schema.py`, same pattern as `typeids.inc`), driven by `title_render()`. Verified:
400 layouts, 11,465 windows, 9,547 with a usable Area, 472,291 records, real captions on screen.

**Next on the path, in order:**
1. **Raise the layout limit and cover all 748** (`Reader::load(limit)` currently caps at 400).
2. **Extend the schema vocabulary.** A sequential walk still derails in 637/748 files with "id not in
   schema" because the designer XML lists only 227 of the ids the binaries use. The reader is
   anchor-driven so it does not block on this, but a clean sequential parse needs the missing names —
   and the derailing ids are visible in the bytes, so this is bookkeeping, not discovery.
3. ✅ **DONE: the code-18 table reference resolves to text.** A Caption reference is
   `0xFFFF | table:u32 | key:u32` (measured, `tools/spui_caption_ref.py`), and wiring it through
   `engine/text.cpp` resolves **1,391 of 1,426 references (97.5%)**. Getting there needed the EP1 text
   mounted at all: it lives in NESTED packages (`tools/nested_locale.py`), which doubled the corpus to
   **369 tables / 37,608 entries / 14,657 keys**. Live in the engine: `txt:0x8B1A2341:0x077427D2` ->
   "Accept Reward", `txt:0xC4F86AE0:0x07736311` -> "Mission Complete!". 35 references still do not
   resolve (tables `mvj` 0x277DEE6A and `legal` 0xADB810C4 are absent from our 57 packages even
   nested).
4. **The presence mask** (a 10-member / 40-byte struct occupies 10 bytes, so only non-default members
   serialize — still a hypothesis).
5. Then the `mode` table, which is what the 60 states actually ask for (`mode GalaxyGameEntry` is
   logged as NOT EXECUTED today).

**Correction recorded, because it is the transferable part:** this session first concluded from a
uniform mod-4 histogram that `spui` is **bit-packed**. It is not — it is **byte-packed with record
sizes that are not multiples of 4**, which drifts the cursor and produces the same histogram. The
per-id test (zero-free patterns, which padding cannot explain) killed the claim. A distribution that
matches your hypothesis is not evidence for it.
## Audit of our own work (DONE, session 13)

User request: use Ghidra to check **everything, starting from `Main`**, to see whether we started
reversing correctly at all. **Answer: yes.** `analysis/claims.json` makes the whole claim surface
machine-checkable — **30 claims / 104 checks**, each citing the doc line it came from, its severity, and
whether our C++ is load-bearing on it. `tools/ghidra_audit.py` (request/verify) +
`tools/ghidra_export.py` (PyGhidra; Ghidra 12 dropped embedded Jython) diff it against Ghidra's own
analysis of the oracle. Verdicts: **PASS 17, FAIL 1, UNKNOWN 11, SKIP 1.** The first run said FAIL 10,
and **nine of those ten were bugs in my checker, not in our reversal** — read
`analysis/audit-review.md` for the three invisible-API artifacts behind them
(`Memory.getInt(addr, TRUE)` is **big-endian**, `JArray("b")` needs the `JByte` **class**, and objdump-
spelled regexes cannot match Ghidra's printer). The one real correction: the main-loop call site is
`0xf4840b`, not `0xf48405` (`0xf48405` only LOADS the vtable slot) — and that load-then-indirect-call
split is the shape of every virtual dispatch here, which is why the loop was hard to find.

**The structural finding:** seven cited addresses have **no function at all** in Ghidra's 54,611-function
inventory, because they are reachable **only through a vtable**; six open with the MSVC SEH prologue
`mov eax,fs:0x0`, which Ghidra left typed as data. This reframes the rw4/spui/prop blockers — the
recorded "handler slots are map ops" claim was *confirmed* (`direct_callers == 0` PASSED), but the real
wall is that auto-analysis cannot see vtable-only functions. Fix: seeding.

Toolchain decision + the Windows junction trap that cost three imports: **`analysis/toolchain-rea.md`**,
`analysis/text-locale.md`.

## One-line status

**Our engine now draws a REAL Spore UI screen.** `source/engine/spui.cpp` reads the `spui` layouts with
the dictionary that EA's own UI designer shipped inside the game (14 `xml` resources, 306 `proxy` ids),
and the Title state renders the game's windows at the game's own coordinates with the game's own
captions — "Create a New Game", "color picker", "Selected Color" — over the real UI atlas
(`analysis/shots/spui_real.png`). Measured: 400 layouts, 11,465 windows, 9,547 with a usable Area,
472,291 records read, 306 schema ids.

The pipeline is now end-to-end on THREE independent data tracks, all from the shipped packages:
DBPF → RefPack → type/instance naming → `gmdl`+`raster` (a 2x2 model grid), `locale` (18,441 strings,
and the table a layout points at), `trigger` (the 60-state machine, which `-state:Main` now walks),
and `spui` + the designer `xml` (window geometry and captions).
## Upstream knowledge is in the repo, but it is SECONDARY (level 4)

**Disassembly is always the first tool; everything else is a fallback.** `upstream/SporeMaster3/`
(tracked, ~9.8 MB: 21 RE docs, the registries, `resources.csv`, beta `KeyMap.txt`) is another
team's work — consult it only when the disassembly is unclear, and test every claim against the
oracle (level 1) or our 57 packages (level 2) before relying on it. It fixed two of our bugs and
was itself wrong in **three** documented places; its C# source is right where its prose is wrong,
so if consulted at all, read the source. Recorded: MEMORY.md "Upstream knowledge", IDEAS.md §5.

## Yardsticks (report both after every run)

| yardstick | value | how to check |
|---|---|---|
| boot markers OK | **10/10** | grep `yardstick:` in `build/bin/Debug/OpenSpore.log` |
| `N_verified` (DISASM-VERIFIED rows) | **15** of 21 | count `**DISASM-VERIFIED**` in `VERIFY.md` |
| packages **index-parsed** | **57 / 57** (0 kind-unknown, 0 invalid) | `packages:` in the log |
| records parsed | **150,945**, 0 invariant violations | `python tools/pkg_kinds.py` |
| **RefPack decoded exactly** | **30,000 / 30,000**, 0 failures | `python tools/refpack.py --verify --limit 30000` |
| game content decoded | 312 B + 10,191 B Czech text, == `memSize` | `pkg:-decode,0,0` / `,0,2` |
| type names resolved | `trigger`/`prop` for cs-cz NameGeneration | `-exec pkg:-types,0` |
| **resources enumerated** | **150,945**, 10.27 GB, 90 types (86 named) | `inventory:` in the log |
| **resources written to disk** | **2,944 locale + 14,549 png**, 0 failed | `pkg:-unpack,all,<dir>,<type>` |
| **headless unpack** | same result, no window/device, 7.6 s | `-headless -exec pkg:-unpack,...` |
| **gmdl mesh decoded** | positions reproduce the file bbox **exactly (err 0.0000)** on 4 variants | `python tools/gmdl_mesh.py --pkg <i>` |
| **engine draws a real mesh** | 3765 verts / 6000 tris, fps 32.3 | `analysis/shots/mesh_engine.png` |
| **engine draws native raster** | `raster` DXT1/DXT5/raw decoded in C++ and drawn | `analysis/shots/raster_engine.png` |
| **whole game exported** | **5,747 textures→BMP + 4,084 models→OBJ**, all 57 pkgs, 0 failed | `pkg:-unpack,all,<dir>,raster\|gmdl` |
| **real Spore geometry rendered** | limb / creature / plant | `analysis/shots/gmdl_pkg{48,52,54}.png` |
| **feature map** | 212 features, engine 79.1%, overall 70.7% | `python tools/feature_map.py` → `analysis/shots/reverse_map.png` |
| **function coverage (code)** | **29 / 105,239 (0.021%)** | `python tools/functions.py` → `analysis/functions.md` |
| **real game asset rendered** | `vehiclepainticons~/be_basic_shake_05.png` (128×128) | `analysis/shots/hero_real_asset.png` |
| **resource names resolved** | **112,155 / 150,945 (74.3%)** | `inventory:` / `pkg:-path` |
| RW4 header | **confirmed** (28-byte template `0x014f6480` copied by `0x011e3f60`) | `python tools/rw4.py --pkg 45 --rec 1456` |
| state hash | `Loader` = **0xd7d04c04** (FNV-1 + tolower, as the original) | `-exec state:-list` |
| fps | **33.4 / 31.2** | `fps=` in the log |
| build warnings | **0** | `cmake --build build --config Debug` output |
| frames rendered per run | **90** (`-quitAfter 90`), aborts=0 | `shutting down: frames=` |
| in-engine capture | **1280x720, 100% non-black** | `wrote cap.bmp ... nonblack=` |
| fault-tolerance | **5 aborts recovered, 6th → storm exit** | `-exec _abortNow:9` → exit `0x4F53AB02` |

## EA's shipped data is now READ (session 4)

`source/engine/pkg.cpp` opens **all 57 real `.package` files, 0 rejected**, by replaying the
original validator's own clauses (`0x008d90b0`) in its own order with its own reasons. The
loader screen lists them, so the first contact with EA data is visible, not just logged.
**All 57 packages index-parse** (0 kind-unknown, 0 invalid), 150,945 records, 0 violations of
`offset < index_offset` and `offset+packedSize <= index_offset`.

Corrected record model (session 5; the session-4 description of it was wrong in two places):
the index's first dword K is a **shared-field bit mask** (bit0 `type`, bit1 `group`, bit2
`unknown`), so `prefix = 4 + 4*popcount(K)` and `stride = 32 - 4*popcount(K)`; a full record is
32 bytes = `type, group, unknown, instance, offset, packedSize, memSize, compression:u16,
flags:u16`. Observed kinds: K=4 → (8,28), K=6 → (12,24), K=7 → (16,20).
Our earlier `prefix = 4+K / stride = 32-K` fitted K=4 and K=7 but **not K=6** (predicted 10,
measured 12) — that formula left 26 packages unparsed until the bit-mask model fixed it.
The word we used to call a "sentinel" is really `compression:u16 + flags:u16`: 0xFFFF = RefPack
(98,407 records, payload starts `10 FB`, `packed < mem`) and 0x0000 = raw (52,538 records,
never `10 FB`, `packed == mem`) — verified by probing payload bytes across every record.

## First resource read end-to-end

`-exec pkg:-dump,0,3,res0.bin` → 1371 bytes out of `Data\Locale\cs-cz\NameGeneration.package`
(record type `0x024a0e52`, instance `0x65e81724`, offset `0x4c11`, packed 1371 / mem 2120,
RefPack). The payload is a Czech name-generation lexicon. **RefPack decoding itself is not
implemented** — `tools/refpack.py` is explicitly UNVERIFIED and still faults on a real stream,
because its command table was transcribed from a third-party doc instead of from the oracle's
decoder at `0x0092c5b0`. The aimed next step is in IDEAS.md §6.

## Assets are HERE (this changed the roadmap)

User dropped the real game data into `build\bin\Debug\` (the app's own cwd):
- `Data\` (Spore base game): `Spore_Game.package` 20,210,108 · `Spore_Content.package` 995 MB ·
  `Spore_Graphics.package` 943 MB · `Spore_Audio1/2.package` ~1 GB each ·
  `Spore_Pack_03.package` 9.6 MB · `PatchData.package` 80 MB · `Config\` `Locale\`
- `DataEP1\` (Galactic Adventures): `Spore_EP1_Data.package` 715 MB · `Spore_EP1_Content_01/02`
  477/758 MB · `Spore_EP1_Locale_01/02` · `Config\`

Total ~5.7 GB of `.package` files. **`.package`, not `.package.big`** — the user corrected
this: `.big` is just what EA internally called the archives. Consequences:
- `fs.cpp` already finds them (dev mode resolves `Data\`), verified: SHIP mode reports 3 paths.
- The package reader is now the highest-value next reversal, and its format must come from
  **the first bytes of a real file** (authority #2), not from any wiki.
- The game's real states may become reachable once packages parse (VERIFY row 16 unblocks).

## Exact commands

```powershell
# configure + build (x86 Debug, MSVC, icon embedded)
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32 -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug

# run with the PROMPT.md test args, then tail the log while it is alive
$wd="$PWD\build\bin\Debug"
Start-Process "$wd\OpenSpore.exe" -ArgumentList '-w','-devDirs' -WorkingDirectory $wd -PassThru
$fs=[IO.File]::Open("$wd\OpenSpore.log",'Open','Read',[IO.FileShare]::ReadWrite)
$sr=New-Object IO.StreamReader($fs); $sr.ReadToEnd(); $sr.Close(); $fs.Close()

# refresh every analysis artifact
python tools\recon.py ; python tools\dump_strings.py ; python tools\find_plaintext_text.py
python tools\compare_builds.py ; python tools\verify_unpacked.py ; python tools\pdb_topology.py
python tools\arg_vocab.py 0x92b570 --out analysis/arg_vocab.md

# reversal helpers
python tools\xref.py --regex "state" --sec .rdata --limit 14 --no-xref
python tools\xref.py --addr 0x01413730          # who references this string?
python tools\show_region.py 0x013f8dc8 0x200    # hexdump/strings around an address
& 'C:\Users\DartPower\scoop\apps\mingw\current\bin\objdump.exe' -d -M intel `
  --start-address=0x7e40c0 --stop-address=0x7e4200 refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe
```

`refs\` is gitignored (third-party binaries); `build\` too. `analysis\` and `assets\` ARE
tracked — that is where the evidence lives.

## The oracle (do not lose this conclusion)

| file | role |
|---|---|
| `refs\bins\ReleaseGA\SporeApp.exe` | **runtime reference.** `.text` encrypted (entropy 8.000), entry in `.bind`. Never disassemble it. |
| `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` | **reversal oracle (authority #1).** Decrypted `.text`, every other section byte-identical, `.bind` removed. |

Proven by `analysis/unpacked_verification.md`: 9/9 metrics PASS, each measured against the
encrypted `.text` as a **control**. Decisive metric: decrypted code references 7 known
`.rdata` string addresses (`devDirs`, `shipDirs`, `SP_Havok`, `Stack trace is:`, …); the
control references 0/7 — and `.rdata` is untouched by the unpack, so that agreement cannot
be coincidence.

Secondary oracles (names/approach only, **never** addresses/values): the
`FindTheUnencryptedDotText\Also1|Also2|Also3` builds, `SporeCreatureCreatorCSA2024.exe`,
`Proto\GonzagoGL.exe`, and `Beta190208\SporeApp.pdb`. 0/12 match the target's link.

## Target facts (level 1)

GA 3.1.0.29 "SPORE™ Galactic Adventures", x86, base 0x00400000, ASLR+NX, linker 9.0
(MSVCR90). Authenticode valid (EA / DigiCert G4, timestamped 2024). CodeView GUID
`abf55581-6487-4b65-b86f-4631a2bdcb02` age 54 (that PDB we do **not** have).
Engine = **RenderWare on D3D9, statically linked** (single d3d9 import:
`Direct3DCreate9`); field guide §3 applies. `.text` = 0x00401000..0x013cbf2a, 83,114
int3-padded functions.

## What the code looks like

```
source/
  main.cpp                 wWinMain -> diag::early_neutralise() -> app::run
  app.h/.cpp               boot chain with 10 numbered markers (the yardstick), frame loop
  common.h                 u8..u64 typedefs + fnv1a (the original's own state-ID hash)
  engine/log.*             share-readable file + per-site consolidation ledger + LOG_ONCE_*
  engine/diag.*            non-fatal asserts, CRT hook, abort interception, dbghelp stacks,
                           run_guarded() = the fault-tolerance primitive
  engine/cmdline.*         reversed arg contract (devDirs/shipDirs/state/preload/-w)
  engine/fs.*              reversed search paths; shipDirs wins over devDirs
  engine/console.*         command registry + stdin worker thread
  render/device.*          Direct3DCreate9 only; the original's 4 error strings; 800x600 fallback
  render/overlay.*         GDI font atlas -> staging texture -> device UpdateSurface
  states/statemgr.*        FNV ids, no-nesting, onEnter/Exit/Tick/Render/Reload, `state` verb
  states/loader.cpp        our Loader state (progress bar + live device/search-path readout)
```

Everything is ours; nothing is a port of EA code, and no EA asset is loaded. The reversal
facts live in comments as **quoted instructions + addresses**, and in `VERIFY.md`.

## Reversals landed this session (details in `VERIFY.md` rows 1-12)

`AppStateManager` is real and reachable in the **release** build — this is the "типа режимы
запуска" the user remembered:
- state ID = **FNV-1a 32 of the name**, stored at `[state+0x28]`, duplicates rejected
  (`0x007e40f2` → `push 0x811c9dc5` → `call 0x932e50`; `0x007e413b` "A state with this ID
  already exists").
- `can't nest states or transitions` (`0x014135dc`, refs `0x7e35ce`/`0x7e3fde`).
- console verb `state` with `-list^-next^-prev^-reload^`, listing format
  `  %-40.40s [0x%08x]` (`0x007e4f40`..`0x007e501c`).
- launch keys `-state <name>` / `-preload <name>` (`0x007e22a0` / `0x007e2235`).
- states are **declared in `.eBT` script data** (`AppStateManager` sits next to
  `ArgScript/GotoState`, `gotoState`, `transition`, `entry`, `exit`, `endChain`), so the set
  of *game* state names is data we do not have until packages arrive. Hence our own Loader.

Full arg vocabulary is now a file: `analysis/arg_vocab.md` (76 call sites of `0x92b570`,
56 distinct names: `devDirs shipDirs state preload safe noVSync vSync noDev noPlugins
noScripts noSound dumpShaders dumpShadersForPIX dumptelemetry multipleInstances
writeableData userDataDir dataDir dataBaseDir locale baseGame ep1 demo gonzago flock mce
mono stereo pix hostIP clientIP isHost jobPriority jobThreads nodefaultvox nosamplecache
noBackground noFocus noDevEffects noEffectScripts noMaterialScripts showConfigAlerts
silent verboseaudio 4.0 5.1 automation …`).

## Blocked / open

1. **`group` / `unknown` semantics** — the bit-mask model names them but not their meaning; and
   why K=4 keeps `group` per-record while 6/7 hoist it.
2. **Type-id → name** is a *lookup*, not a puzzle: `upstream/SporeMaster3/data/reg_type.txt`
   (593 lines), since type ids are **not** FNV of their name (0/6 tested — IDEAS.md §5.20).
   Our top ids: `0x01A510BB` (98% of the fixed records), `0x2F4E681B` = `rw4` (12,987 records),
   `0x024A0E52` (the Czech lexicon we decoded).
3. **RenderWare bridge** not started. **x64** switch exists in CMake, never compiled.
4. `-w` semantics (VERIFY row 15) and the path-assembly block (row 8) are still DRAFT/PARTIAL.

## Next actions, in order

1. **Startup chain (session 13)** — entry -> **WinMain `0x00f489e0`** -> **app_main `0x00f48810`**
   -> **run `0x00f47e90`** is traced (`analysis/startup.md`, VERIFY rows 35-37). Next: walk the
   **AppMode ctor `0x007e76e0`** (created by `0x007e8a00`, stored at `[app+0x20]`) and the rest of
   `run` after `0x00f481be`, then find where the (text) `trigger` resources and SPUI are loaded.
2. **rw4 mesh parser** — the model subsystem's entry points are known: the rw4 handler
   `0x7633c0` (registered by `0x007635f0` -> `0x6ad090`) dispatches through manager singletons
   (getters `0x67dcd0..0x67df70`, globals `0x15fd8b0..`); the model-manager setter `0x67df40`
   (global `0x15fd8c4`) is called from `0x7a71cb` / `0x7a7efa`. **Follow that chain to the real
   reader.** The magic is memcpy'd, so go by call graph, not by immediate search.
3. **gmdl is DONE for positions and DRAWN** (sessions 12-13): stride 36, position = first float3,
   verified exactly (err 0.0000) on four variants; `source/engine/gmdl.cpp` + `source/render/mesh.cpp`
   draw one on our own screen. UV = 2 floats at offset 16 (N=6). **Remaining: bytes 24-35, the N=5
   layout, and the `prop` second variant** (its `{key,type,count,size,data}` walk consumed exactly
   only 116/4000).
4. `group`/`unknown` field semantics (VERIFY row 20 is the model; the meaning is open).
5. Wire loose directories (unpacked trees) into `fs` as a data source; a separate `OpenSpore.Cli`
   binary would be tidier than `-headless`, but the capability already exists.

Done and off the list: `.package` header + fixed-stride index (session 4: 57 files, 0 invalid,
17,901 records, 0 violations), in-engine frame capture (✅ `GetBackBuffer` →
`GetRenderTargetData` between `EndScene` and `Present`; `analysis/shots/loader_capture.png`
and `loader_packages.png` show readable text), the abort-recovery exercise
(✅ 5 recoveries + storm exit), **`-w`/`-f`/`-r`** (session 12: row 15 DISASM-VERIFIED — the real
handling is in the device fn `0x7e74a0..0x7e7637`; `w` beats `f`; `r` is `%dx%d`), **headless
mode** (session 12, genuinely windowless after the session-13 fix), **drawing a decoded gmdl mesh
in the engine** (session 13), the **first-argument bug** (session 13: `parse_command_line`
dropped the first argument because `lpCmdLine` excludes the program name), **`raster` textures
decoded AND drawn by the engine** (session 13), and the **startup chain traced** (session 13:
entry -> WinMain -> app_main -> run, `analysis/startup.md`).

## Standing rules for whoever continues

- Authority order is in `VERIFY.md`'s header. Quote instructions inline before changing a
  function; if you cannot, label the change a hypothesis.
- Match by **function identity**, not address, when consulting the beta PDB or the other builds.
- Every metric gets a **chance baseline** and a **control**. Three of my own verifier
  revisions failed this way (`IDEAS.md` §5.5-5.7).
- Never call a public locking API while holding the same mutex (`IDEAS.md` §4) — this one
  crashed the first boot and the harness is what diagnosed it.
- No modal dialog may ever block the process. `early_neutralise()` is the first statement of
  `wWinMain` for exactly this reason; keep it there.
- Commit messages end with how the change was verified. Rewrite this file before compaction.
- **Go by Main, forward (session 13).** Disassembly first, and always advance along the original's
  execution from the entry point: reverse only what is needed to reach the next step, note unfinished
  formats and keep moving. The goal is Main -> a fully playable build. Keep the "Main -> playable"
  table above honest.
