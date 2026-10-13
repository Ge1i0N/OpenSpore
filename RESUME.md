# RESUME — read this first

Hand-off snapshot. Rewritten at the end of every session, **before** context compaction.
Last update: 2026-10-13 (session 6b).

## One-line status

**There is a booting executable that decodes real Spore content and names it.** RefPack is
solved and verified (30,000/30,000 real records decode to exactly `memSize`), all 57 `.package`
files index-parse (150,945 records), `pkg:-decode` writes actual game data, and type ids resolve
to names (`pkg:-types` shows `cs-cz/NameGeneration` = 13 × `trigger` + 1 × `prop`). The
RenderWare bridge has not started; that is the next big piece. `-w -devDirs` still boots 10/10
markers at ~33 fps.

## Upstream knowledge is now IN the repo (level 4, tested before use)

`upstream/SporeMaster3/` (tracked, ~9.8 MB): another team's RE docs (21 files), the type/group
registries, `resources.csv` (6,005 resource rows) and the beta `KeyMap.txt`. It fixed two of our
bugs and was itself found wrong in two byte-level places. **Never adopt a claim from it without
testing it against the oracle (level 1) or our 57 packages (level 2)** — see MEMORY.md "Upstream
knowledge" and IDEAS.md §5 items 19-20 for the failures.

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

1. **Identify a decoded resource's type** and show real game content on the Loader screen
   (we can already decode; next is to *use* it). `rw4` (0x2F4E681B) is the RenderWare resource
   type - an obvious first target once the RW bridge starts.
2. **RenderWare plugin-registration family** (VERIFY row 13): start from `vb -<used>` at
   `0x0140a980` (field guide §3: bridge `Rw*RegisterPlugin`/`*RegisterPluginStream`/
   `*GetPluginOffset`/`*SetStream{Always,Rights}CallBack` for every PLUGINBASE class; expect
   **no** RxPipeline equivalent).
3. Disassemble the sole `GetCommandLineA` consumer at `0x00e20b5a` and its callee to settle
   `-w` (row 15).
4. Extract `render/rhi.h` **when the RW bridge starts**, not before (`IDEAS.md` §1).

Done and off the list: `.package` header + fixed-stride index (session 4: 57 files, 0 invalid,
17,901 records, 0 violations), in-engine frame capture (✅ `GetBackBuffer` →
`GetRenderTargetData` between `EndScene` and `Present`; `analysis/shots/loader_capture.png`
and `loader_packages.png` show readable text), and the abort-recovery exercise
(✅ 5 recoveries + storm exit).

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
