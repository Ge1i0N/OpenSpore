# RESUME — read this first

Hand-off snapshot. Rewritten at the end of every session, **before** context compaction.
Last update: 2026-10-12 (session 3, evening).

## One-line status

**There is a booting executable.** `build/bin/Debug/OpenSpore.exe` reaches 10/10 boot markers
with `-w -devDirs`: window + icon, D3D9 device, font atlas, developer console, and an
AppStateManager-analog that switches into a Loader state which renders a progress bar.
The reversal oracle and the six docs are in place; the RenderWare bridge has not started.

## Yardsticks (report both after every run)

| yardstick | value | how to check |
|---|---|---|
| boot markers OK | **10/10** | grep `yardstick:` in `build/bin/Debug/OpenSpore.log` |
| `N_verified` (DISASM-VERIFIED rows) | **12** of 17 | count `**DISASM-VERIFIED**` in `VERIFY.md` |
| build warnings | **0** | `cmake --build build --config Debug` output |
| frames rendered per run | **50** (`-quitAfter 50`), aborts=0 | `shutting down: frames=` |
| in-engine capture | **1280x720, 100% non-black** | `wrote cap.bmp ... nonblack=` |
| fault-tolerance | **5 aborts recovered, 6th → storm exit** | `-exec _abortNow:9` → exit `0x4F53AB02` |

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

1. **Package reader** — the only thing between us and the game's real states. Assets are
   present (see above); read the first 64 bytes of `Data\Spore_Game.package` before writing
   anything, and cross-check the parser against the original's own reader in `.text`.
2. **x64** switch exists in CMake, never compiled. **RenderWare bridge** not started.
3. `-w` semantics (VERIFY row 15) and the path-assembly block (row 8) are still DRAFT/PARTIAL.

## Next actions, in order

1. **`.package` header reversal**: bytes of a real file (authority #2) + the original's
   reader (authority #1). Then a reader, then list what `Spore_Game.package` actually contains.
2. **RenderWare plugin-registration family** (VERIFY row 13): start from `vb -<used>` at
   `0x0140a980` (field guide §3: bridge `Rw*RegisterPlugin`/`*RegisterPluginStream`/
   `*GetPluginOffset`/`*SetStream{Always,Rights}CallBack` for every PLUGINBASE class; expect
   **no** RxPipeline equivalent).
3. Disassemble the sole `GetCommandLineA` consumer at `0x00e20b5a` and its callee to settle
   `-w` (row 15).
4. Extract `render/rhi.h` **when the RW bridge starts**, not before (`IDEAS.md` §1).
5. Real game states once packages parse.

Done and no longer on the list: in-engine frame capture (✅ `GetBackBuffer` →
`GetRenderTargetData` between `EndScene` and `Present`; `analysis/shots/loader_capture.png`
shows readable text) and the abort-recovery exercise (✅ 5 recoveries + storm exit).

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
