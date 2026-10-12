# VERIFY — feature × address × disassembly status

**Authority order** (from `ai-brief-and-commands.md`, restated here because this file is the
tie-breaker): 1. disassembly of the original binary (+ PDB, + its `.data`) · 2. its shipped
data files · 3. other reimplementations of the same *engine* (approach only) · 4. wikis /
SDK headers (names and types only) · 5. our own reasoning.

**Oracle**: `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` — the decrypted dump of the GA
build. Proven in `analysis/unpacked_verification.md` (9/9 metrics against the encrypted
control). All addresses below are **VA in that dump's layout** (image base 0x00400000).
The shipping file `ReleaseGA\SporeApp.exe` is the *runtime* reference only; its `.text` is
encrypted and must never be disassembled.

Status values: `DISASM-VERIFIED` (we read the instructions and quote them) ·
`DISASM-PARTIAL` (read, but a branch/behaviour is unconfirmed) · `DRAFT` (our interpretation,
no disasm yet) · `UNVERIFIED-DRAFT` (a drafted body, by definition not a reversal).

| # | Feature | Original address(es) | Key strings (VA) | Status | Our implementation |
|---|---|---|---|---|---|
| 1 | Arg lookup helper (`this, name, …` → index or -1) | `0x0092b570` | — | **DISASM-VERIFIED** | `engine/cmdline.cpp` (contract, not code) |
| 2 | devDirs / shipDirs precedence → `[cfgObj+0x159]` useDevDirs | `0x007e7f27`, `0x007e8365`, `0x007e838e` | `devDirs` 0x013f8dc8, `shipDirs` 0x013f8db4 | **DISASM-VERIFIED** | `Args::use_dev_dirs()` = `dev_dirs && !ship_dirs`; shipDirs wins |
| 3 | Startup state selection (`-state`, `-preload`) | `0x007e2235` (preload), `0x007e22a0` (state) | `state` 0x01413588, `preload` 0x014135a4 | **DISASM-PARTIAL** (the goto call is via vtable `[esi+0x38]`/`[esi+0x28]`, not resolved) | `app.cpp` honours `-state <name>` |
| 4 | State identity = FNV-1a 32 of the name; duplicate rejected | `0x007e40f2`→`0x00932e50`; store `[ecx+0x28]`; dup probe `0x007e4132` | `A state with this ID already exists` 0x01413600 | **DISASM-VERIFIED** | `common.h::fnv1a_upper`, `Manager::reg` |
| 5 | No nesting of states/transitions | `0x007e35ce`, `0x007e3fde` | `can't nest states or transitions` 0x014135dc | **DISASM-VERIFIED** | `Manager::m_in_transition` guard |
| 6 | `state` console verb + `-list^-next^-prev^-reload^`, `[%-40.40s [0x%08x]]` | `0x007e4f40` (ctor of the verb), dispatch `0x007e501c` | 0x014136dc / 0x0141371c / 0x01413730 / 0x01413748 / 0x01413758 / 0x01413760 / 0x01413774 / 0x0141377c / 0x01413794 / 0x014137a0 / 0x014137d4 / 0x014137ec / 0x01413804 | **DISASM-VERIFIED** (option table pushed as immediates) | `states::install_console_command` |
| 7 | Arg vocabulary census (76 sites → 56 names) | all callers of `0x0092b570` | — | **DISASM-VERIFIED** (mechanical scan) | `analysis/arg_vocab.md` |
| 8 | Data-dir prefixes (`BaseData/`, `Locale/`, `UserData/`, `Config/`, `Data/`, `./`, `../UserData/`) | `0x007e7e00..0x007e8420` | 0x01413cb0 / 0x01413cc4 / 0x01413c8c / 0x01413ca0 / 0x01413cdc / 0x01413cd4 / 0x01413c58 | **DISASM-PARTIAL** (strings + push order confirmed; the assembly of the final path is not walked) | `engine/fs.cpp` search list |
| 9 | Renderer error contract (4 user strings) | refs: `0x007e6ff7`, `0x007e748f` | `Could not start the renderer.` 0x014139e0, `Could not create the Graphics Device.` 0x01413ac8, `Please ensure your display is set to 32 bits.` 0x014139fe, `Direct X is unavailable.` 0x01413aee | **DISASM-VERIFIED** (strings + referrers) | `render/device.cpp` reuses the exact texts |
| 10 | Default/fallback resolution 800×600 | `0x007e74f1` (`push 0x320`, `push 0x258` → `0x00410330`) | `%dx%d` 0x01413ab8 | **DISASM-PARTIAL** (the callee's role is not confirmed) | `Device::init` falls back to 800×600 |
| 11 | Own log filenames | refs: `0x007e7c85`, `0x007e7c5b` | `Logging.txt` 0x01413bf0, `LocalLogging.txt` 0x01413c08 | **DISASM-VERIFIED** (references) | ours: `OpenSpore.log` (deliberate divergence, see MEMORY) |
| 12 | Crash stack walker capability | dbghelp imports + `0x007e…` | `Stack trace is:` 0x013f8cb0 | **DISASM-VERIFIED** (imports: SymInitialize/SymLoadModule64/SymGetSymFromAddr64/SymGetLineFromAddr64/SymSetOptions/UnDecorateSymbolName/SymCleanup) | `engine/diag.cpp` (produced a real symbolised stack live) |
| 13 | Engine = RenderWare, statically linked, on D3D9 | — | `vb -<used> dumps information about the RenderWare vertex buffer free (or used) lists.` 0x0140a980 (and index-buffer twin 0x0140a9f0) | **DISASM-VERIFIED** (string) / plugin family **not started** | roadmap |
| 14 | Single D3D9 import | IAT slot `0x013cc…` | `d3d9.dll!Direct3DCreate9` (the only d3d9 import) | **DISASM-VERIFIED** (import table) | `render/device.cpp` uses exactly `Direct3DCreate9` |
| 15 | `-w` (windowed) handling | raw-argv parser at `0x00e20b5a` (sole `GetCommandLineA` consumer) | `screenWindowed` 0x014111d0 (**no** immediate .text ref) | **DRAFT** — our `-w` is our own convention | `cmdline.cpp` |
| 16 | Package/asset format | — | `package.big`, `Spore Baked Texture` 0x013ef048, `Created by Spore Editor` 0x013ef05c | **UNVERIFIED-DRAFT** | ⛔ blocked on assets |
| 17 | Beta PDB module topology | — | `c:\BuildAgent\cm4-build312-spore\CMBuild\obj\Win32\debugrelease\SPGonzago\SPGonzagoPCH.obj` | **DISASM-VERIFIED** (ANSI in module streams) | `analysis/pdb_topology.md` (9 projects; `SPGonzago` 348 paths) |
| 18 | **DBPF magic is an immediate, not a string** | `0x008d90d6` (`cmp dword [ecx],0x46504244`) | none — "DBPF" appears 0 times in `.rdata`/`.text` | **DISASM-VERIFIED** (`tools/find_imm.py --ascii DBPF` → 2 sites) | `tools/pkg_records.py` reads it from the files |
| 19 | DBPF header = 0x60 bytes; fields +0x00 magic, +0x04 ver≤3, +0x24 count<0x07FFFFFF, +0x2c index size, +0x3c non-zero, +0x40 index offset | validator `0x008d90b0`; loader `0x008d8f70` (`cmp eax,0x60`) | — | **DISASM-VERIFIED** (quoted in MEMORY) + **57/57 files**: `idx_off+idx_size == filesize` | `source/engine/pkg.*` (pending) |
| 20 | Index = 12-byte prefix + `count` × 24-byte records (fixed-width subset) | parser reached from `0x008d9a90` (vtable+0x90) | — | **DISASM-PARTIAL**: stride proven on **31/57** files by `12+24*count == idx_size`; 25 files (Text/NameGeneration/etc.) carry variable-width records and need the record parser disassembled | — |
| 21 | Package class vtable | `0x01436938` (33+ slots; +0x68 loader, +0x74 validator, +0x80 getsize, +0x90 index loader, +0x34 resource accessor) | — | **DISASM-VERIFIED** (walked from the `0x8d9320` pointer) | roadmap for `pkg.*` |

## Ordered next actions

1. **`.package` header reversal** — assets are HERE (~5.7 GB in `build/bin/Debug/Data` and
   `DataEP1`, 12 `.package` files). Read the first 64 bytes of `Data\Spore_Game.package`
   (authority #2) and find the original's reader (authority #1) **before** writing a parser.
   The extension is `.package`, not `.package.big` (user correction: `.big` is just EA's
   internal name for the archives).
2. **RenderWare plugin-registration family** (row 13): from `0x0140a980`'s referrers outward.
3. **Disassemble `0x00e20b5a`'s callee** to settle `-w` (row 15).
4. Row 8 completion: walk the path-assembly block, then upgrade `fs.cpp` from interpretation
   to reversal.
5. Extract `render/rhi.h` when the RW bridge starts (`IDEAS.md` §1).

Done since this list was first written: **in-engine frame capture** and **the abort-resilience
exercise** — both are in the matrix below with their measurements.

## Verification matrix for our own build (what "verified" means here)

| Claim | How it was verified |
|---|---|
| Builds clean | `cmake --build build --config Debug` → `-> ...\OpenSpore.exe`, no `error C/LNK`, no `warning C` |
| Boots | log shows `yardstick: 10/10 markers OK` |
| Runs with `-w -devDirs` | log: `flags: windowed=1 devDirs=1 shipDirs=0 useDevDirs=1` |
| shipDirs wins | log: `-devDirs -shipDirs` → `useDevDirs=0`, `search mode=SHIP, 3 paths` |
| Window + icon exist | log: `window 010C0D18 'OpenSpore - Loader' … icon=embedded` |
| Renderer initialised | log: `device windowed 1280x720 vsync=1 (desktop 1692x919 fmt=22)` |
| **Frame actually rendered** | in-engine capture wrote `cap.bmp` 3,686,454 B (=1280×720×4+54) with `nonblack=9502/9502 (100.0%)`; `analysis/shots/loader_capture.png` shows readable text: title, `mode: windowed 1280x720 vsync=1 desktop=1692x919`, `state: Loader`, the search-path list, a 28% progress bar, `frames: 20 t=0.70s log lines: 32` |
| **Frame loop survives an abort** | `-exec _abortNow:9` → 5 aborts intercepted inside `run_guarded`, each with a symbolised stack (`abort_handler ← raise ← abort ← frame_body ← os_guarded_call ← run_guarded`), loop kept counting frames; the 6th within 1 s hit the storm limit and exited `0x4F53AB02`. `-exec _abortNow` (single) → `frames=40 aborts=1` then `clean exit code=0` |
| Crash harness works | live: a real `std::mutex` re-lock produced a symbolised 16-frame stack with file:line |
| Log is tailable while running | live: read with `[IO.FileShare]::ReadWrite` while the process was alive |
| Clean shutdown | `WM_CLOSE` / `-quitAfter` → `shutting down: frames=N aborts=… asserts=0` → `clean exit code=0` |
