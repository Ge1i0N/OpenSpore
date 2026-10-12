# HISTORY — what changed, and how it was verified

Every entry ends with its verification. If a change has no measurable delta it is labelled
**hypothesis**, per the output contract in `ai-brief-and-commands.md`.

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
