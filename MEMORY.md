# MEMORY — durable facts, layouts, traps

## TOOL PRIORITY (user rule, 2026-10-12 — this overrides habit, follow it first)

**Disassembly is THE PRIMARY tool.** Study the data files (hexdumps, header diffs, entropy)
**only when the disassembly does not answer the question** — never as the first move.

Concretely, for every "how does X work?" the order is now:
1. `objdump` the oracle (`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`) for the function
   that implements X. Find it by: string xref (`tools/xref.py`), immediate-magic xref
   (`tools/find_imm.py` — for format magics/hashes there is often NO string), import
   consumer (`tools/arg_vocab.py` pattern), or the caller chain of something already known.
2. Read the disassembly until the algorithm is clear, quoting instructions in the comment.
3. **Only if** the code is ambiguous, stripped, or data-dependent in a way disasm cannot
   resolve → look at the shipped data files (`tools/hd.py`, `tools/pkg_headers.py`) to
   confirm which branch the real bytes take.
4. Data-driven inference (entropy, correlation across files) is a *tie-breaker and a
   sanity check*, not a discovery method. It produced useful numbers this project, but
   every time it led, we then had to spend builds guessing API shapes — see the trap list.

Why the rule exists: the data-first detour in session 3b cost several compiler errors
(`GetTextMetricW`, `ETO_CLAMPED`, `GetSurfaceDesc`, `GetFrontBufferData` under DISCARD) that
one grep of the right header, or reading the original's own calls, would have prevented.
The original binary already solved every one of those problems; reading its answer is faster
than deriving our own and being wrong.

## Authority order (from `ai-brief-and-commands.md`, memorised): 1. disassembly of the original
binary (+ its PDB, + its `.data`) → 2. its shipped data files → 3. other reimplementations of
the same *engine* (approach only) → 4. wikis/SDK headers (names/types only) → 5. my reasoning.
Everything below says which level it came from.

## Binaries we hold

| path | sha256 / size | identity |
|---|---|---|
| `refs\bins\ReleaseGA\SporeApp.exe` | `3010a413f9a4a05ee9c4de24756c1c94c7fb218f70cad29bcf5bc0b4abfa2466` | **the target.** GA retail 3.1.0.29, `.text` encrypted |
| `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` | 24,895,528 B | same link, `.text` decrypted. **Oracle.** |
| `refs\bins\Beta190208\SporeApp.pdb` | 134,269,952 B | Feb-2008 beta PDB, different build (see traps) |
| `refs\bins\FindTheUnencryptedDotText\Also1\SporeAppEP1.exe` | 24,564,736 B | GA, plaintext `.text`, **different build** |
| `refs\bins\FindTheUnencryptedDotText\Also2\SporeAppEP1.wtf` | 24,904,704 B | GA, plaintext `.text`, different build (`.wtf`/`.bgm` are renamed copies; `.bgm` keeps the encrypted `.text`) |
| `refs\bins\FindTheUnencryptedDotText\SporeCreatureCreatorCSA2024.exe` | 12,569,400 B | standalone Creature Creator, plaintext |
| `refs\bins\Proto\GonzagoGL.exe` | 1,974,272 B | **pre-alpha prototype** ("Gonzago" = the early Spore codename), plaintext, OpenGL era |

None of the `FindTheUnencryptedDotText` candidates is the same link as the target: 0/12 match on
CodeView, `.rdata` or `.data` (`analysis/build_identity.md`). They are **secondary** oracles —
useful for names/approach, never for addresses or values.

## GA target layout

```
image_base      0x00400000      size_of_image  0x1938000 (encrypted) / 0x190e000 (unpacked)
.text   0x00401000 vsize 0x00fcaf2a   <- encrypted in the shipping file, decrypted in the dump
.rdata  0x013cc000 vsize 0x0013f5de   <- plaintext in BOTH; strings, RTTI, vtables, .eBT records
.data   0x0150c000 vsize 0x00212764 raw 0x000c4c00  (VirtualSize > raw: BSS tail)
.tls    0x0171f000 0xd          CONST 0x01720000 0x1f
.rsrc   0x01721000 vsize 0x00506ac0   <- 22 RT_VERSION blocks, RT_MANIFEST, no icon-heavy data
.reloc  0x01c28000
.bind   0x01d0e000 vsize 0x00029cd0   <- EncryptPE loader; ENTRY POINT LIVES HERE (0x01d0e310)
```

- Section characteristics are unusual and worth knowing before you `VirtualProtect` anything:
  `.text` = `0x60000020` (CODE|READ|WRITE — writable code, needed by the decryptor),
  `.data`/`.tls`/`CONST` = `0xc0000040` (INIT_DATA|READ|**EXECUTE**).
- DLL characteristics `0x8140` = DYNAMIC_BASE | NX_COMPAT | TERMINAL_SERVICE_AWARE. So the target
  is **ASLR-capable**: never hardcode 0x00400000-relative addresses in our own code, and expect
  relocations when diffing.
- No export directory, no exception directory, **no load configuration directory** (so no
  SEH-guard/CFG metadata), no delay imports. SEH is `_except_handler4_common` (GS cookies), which
  matches the `push fs:0x0` / `mov fs:0x0, eax` prologues seen at 0x011e1214.
- CRT: MSVCR90 (VS2008) via 159 imports. Our own build must therefore be comfortable with a
  VS2008-era *binary*, while we compile with VS2022 — do not assume CRT internals match.

## Runtime surface (what the reimplementation must replace)

| need | what the target uses |
|---|---|
| render | `d3d9.dll!Direct3DCreate9` — the *only* D3D import. `d3dx9_27.dll`: D3DXCheckVersion, D3DXCompileShader, D3DXGet{Pixel,Vertex}ShaderProfile, D3DXMatrix{Multiply,MultiplyTranspose,Transpose}, D3DXVec3{Transform,TransformNormal} |
| audio | `DSOUND.dll` ordinals **#1** and **#9** only (direct-create style, no XAudio) |
| input | `DINPUT8.dll!DirectInput8Create` — polled, so `PostMessage` will not drive it (field guide §2) |
| timing | `WINMM!timeBeginPeriod/timeEndPeriod/timeGetTime` + `QueryPerformanceCounter/Frequency` |
| net | `WSOCK32.dll` (23 fns, BSD-style) + `WS2_32` (WSAIoctl, WSASocketA, WSASetLastError) |
| steam | `steam_api!SteamAPI_Init/Shutdown/SteamApps` — 3 calls; the DRM/entitlement seam |
| crash | `dbghelp!SymInitialize/SymLoadModule64/SymGetSymFromAddr64/SymGetLineFromAddr64/SymSetOptions/UnDecorateSymbolName/SymCleanup` + `Stack trace is:` string → in-process stack walker |
| misc | `IMM32` (IME), `USP10` (ScriptItemize/ScriptBreak → its own text shaper), GDI (CreateDCA/ExtEscape/GetDeviceCaps = **video-RAM probing**), `PSAPI!GetModuleInformation` |
| files | `CreateFileA/W`, `ReadFile`, `SetFilePointer`, `CancelIo`, `FindFirstFileW` … plus `SHGetFolderPathW`, registry (Reg*), `GetVolumeInformationW`, `GetComputerNameExA` (a hardware-ID/license fingerprint) |

**Engine = RenderWare on D3D9, statically linked.** Evidence: `vb -<used> dumps information about
the RenderWare vertex buffer free (or used) lists.` (0x0140a980) and the same for index buffers
(0x0140a9f0). There are **no `Rw*` imports**, so RW is inside `.text` → field guide §3 applies, and
the first thing to find is the plugin-registration family, not the D3D calls.

## DevDirs / command line (level 1: disasm of the oracle, 0x7e7e00..0x7e8420)

Instructions this section is based on, quoted verbatim:

```
7e7f27:  push 0x13f8dc8            ; L"devDirs"
7e7f2c:  mov  ecx, ebp             ; <config object>
7e7f3b:  call 0x92b570             ; -> index, or -1 if absent
7e7f43:  cmp  eax, 0xffffffff
...
7e8365:  push 0x13f8dc8            ; L"devDirs"   (second site, same object)
7e8373:  mov  BYTE PTR [esi+0x159], 0x0
7e837a:  call 0x92b570
7e837f:  cmp  eax, 0xffffffff
7e8384:  mov  BYTE PTR [esi+0x159], 0x1   ; devDirs present -> use dev dirs
7e838e:  push 0x13f8db4            ; L"shipDirs"
7e8395:  call 0x92b570
7e839a:  cmp  eax, 0xffffffff
7e839f:  mov  BYTE PTR [esi+0x159], 0x0   ; shipDirs present -> force ship dirs
```

So: **`[configObj+0x159]` = useDevDirs (BYTE)**, `devDirs` sets it, `shipDirs` clears it, and
`shipDirs` is evaluated after so it wins. Lookup helper = `0x92b570(this=ecx, name, a2, a3, a4)`.
Sibling keys: `dataDir` 0x01413cfc, `userDataDir` 0x01413c74. Path prefixes in the same function:
`UserData/` `BaseData/` `Locale/` `Config/` `Data/` `./` `../UserData/` `BaseData`.

`-w` (windowed) is **not yet located** — it is not among these keys. Open task, see VERIFY.md.

## DBPF package format (level 1: disasm of the oracle; level 2: 57 real files)

Found by `tools/find_imm.py --ascii DBPF` (the magic is an **immediate**, not a string —
there is no "DBPF" anywhere in `.rdata`), then confirmed field-by-field.

**Reader cluster** (all in one class, vtable at `0x01436938`, 33+ slots):
| address | slot | role |
|---|---|---|
| `0x008d8f70` | vtable+0x68 | header loader: seeks, reads `0x60` bytes, `cmp eax,0x60` (must read exactly 96), maps fields |
| `0x008d90b0` | vtable+0x74 | header **validator** |
| `0x008d8bf0` | — | the byte reader (`read(buf,size)`), 3 direct callers |
| `0x008d9a90` | vtable+0x90 | index loader: reads `[obj+0x324]`(offset) + `[obj+0x32c]`(size) via `0x8dc4b0`, then parses |
| `0x008d9320` | vtable+0x80 | get-size |
| `0x008da6f0` | vtable+0x34 | the resource accessor |

**Header = 0x60 (96) bytes.** Validator `0x8d90ae` (quoted):
```
8d90d6: cmp DWORD PTR [ecx],0x46504244   ; "DBPF"
8d90de: cmp DWORD PTR [ecx+0x4],0x3      ; version <= 3   (ja = reject)
8d90e4: cmp DWORD PTR [ecx+0x24],0x7ffffff ; count < 0x07FFFFFF
8d90ed: mov edx,[ecx+0x40] / cmp edx,eax / jae   ; index_offset < filesize
8d90f4: mov esi,[ecx+0x2c] / add esi,edx / cmp esi,eax / ja  ; index_size + index_offset <= filesize
8d90fd: cmp DWORD PTR [ecx+0x3c],0x0 / je       ; field +0x3c must be non-zero
```
So the **confirmed** header fields are:
`+0x00` magic "DBPF" · `+0x04` version (≤3; all 57 files are 3) · `+0x24` entry count ·
`+0x2c` index byte size · `+0x3c` non-zero (unknown meaning) · `+0x40` index file offset.

**Data confirmation (authority #2, `tools/pkg_records.py`):** across **all 57** shipped
`.package` files, `index_offset + index_size == filesize` exactly (`tail = +0` for 57/57),
and `index_offset` is always the *last* thing in the file. That is the invariant the
validator enforces, and it is what makes a reader testable without any wiki.

**Index records — NOT yet reversed (explicit).** The naive `index_size / count` is not an
integer (UI 84/3=28, NameGen 348/14=24.86, Text 3936/196=20.08), so records are
**variable-width** and contain a per-record size. The header loader maps `+0x20`→`[obj+0x328]`
and `+0x28`→`[obj+0x32c]`, while the validator checks `+0x24`/`+0x2c`/`+0x40` — i.e. the
loader's cached copies and the validator's offsets are *different fields*, so "count at
+0x24" and "size at +0x2c" are the validator's truth and the loader's +0x20/+0x28 copies are
something else (a second count/size pair). Do not guess the record layout from hexdumps:
the next step is to disassemble the vtable+0x24 parse call that `0x8d9a90` makes on the
stream object at `[obj+0x2d0]`, which is where the stride and the name/string table live.

## Beta PDB facts (level 1: the PDB's own bytes)

- MSF 7.00, page_size **1024**, num_pages 131123, dir_size 532088 (0x81E78), 2683 streams,
  130338 directory pages, all distinct, none overlapping the directory, accounted page budget
  131121 of 131223 → validated by `tools/pdb.py`.
- Directory is **two-level**: super block @52 holds 3 entries (0x20030/31/32) = the pages of the
  stream that contains the 520-entry directory page list. Assuming one-level is the classic trap.
- **No MSF information-stream magic** (`Information for MSF 7.00` occurs nowhere) and **no
  named-stream map** (`/TPI`, `/DBI`, `/GLOBAL`, `/PUBLIC` absent). Stream 1's first DWORD is
  0x01312E94, not the PDB70 signature 0x00504442. Conclusion: this is a stripped/rewritten PDB.
  Do not apply the documented v70 stream layout to it.
- Stream 9 (280 B) = the beta image's section-header array: `.text` 0x1671f2c @0x1000,
  `.rdata` 0x218db4, `.data` 0x2efec8, `.idata` 0x5ae0, `.tls` 0x313, `.rsrc` 0x84fc,
  `.reloc` 0xf68ee. Note `.data` raw 0x10c000 vs vsize 0x2efec8, and a **separate `.idata`** —
  the 2008 layout differs from the 2024 GA link, which is more proof the PDB is not our build.
- Module paths are readable ANSI in module streams, e.g.
  `c:\BuildAgent\cm4-build312-spore\CMBuild\obj\Win32\debugrelease\SPGonzago\SPGonzagoPCH.obj`.
  CodeView record header in *this* file is `{u16 length; u16 type}` (verified by walking stream 14:
  `0x62/0x1101` = S_OBJNAME, then `0x3a/0x1116` = S_COMPILE3, each landing exactly on the next
  header). Most CodeView docs put length first too — but confirm per-file, never assume.
- `SPGonzago` as a project name ties the beta PDB to `Proto\GonzagoGL.exe`: "Gonzago" is the
  early Spore codename. So the prototype and the 2008 beta PDB are the same lineage.

## AppStateManager / game states (level 1: disasm of the oracle)

The user remembered this as "типа режимы запуска, управлял в девбилде" — it is real, and it is
reachable in the **release** build too.

- **State identity = FNV-1a 32 of the name**, stored at `[state+0x28]`:
  ```
  7e40f2: push 0x811c9dc5 / push eax(name) / call 0x932e50   ; hash
  7e4103: mov [ecx+0x28], eax                                ; store as the ID
  7e4132: cmp ecx,[eax+edx*4]                                ; duplicate-ID probe
  7e413b: push 0x1413600                                     ; "A state with this ID already exists"
  ```
  So `fnv1a`/`fnv1a_upper` in `source/common.h` is the engine's own primitive, not our choice.
- **No nesting**: `can't nest states or transitions` 0x014135dc (refs 0x7e35ce, 0x7e3fde).
  Transitions are a chain, not a stack; per-state callbacks `entry`/`exit`/`onExit`
  (0x01413180 / 0x01413178 / 0x01413624), `transition` 0x01413188, `endChain` 0x014130f8,
  `gotoState` 0x0141316c, `ArgScript/GotoState` 0x0141351c.
- **Console verb is literally `state`** (ctor 0x007e4f40, dispatch 0x007e501c):
  help 0x014136dc, arg spec `[<state:cstring>]` 0x0141371c, options `-list^` 0x0140e430 /
  `-next^` 0x01413758 / `-prev^` 0x01413774 / `-reload^` 0x01413794, listing format
  `  %-40.40s [0x%08x]` 0x014137ec, current `Current state: '%s'` 0x014137d4,
  miss `no such state` 0x01413804.
- **Launch keys** (same helper `0x92b570` as devDirs): `state` 0x01413588 at 0x007e22a0,
  `preload` 0x014135a4 at 0x007e2235.
- **The set of state names is DATA, not code**: `AppStateManager` 0x0141319c sits inside the
  `.eBT` behaviour-tree string cluster, so the game's own states come from scripts in
  `package.big`. We have no assets → we own the mechanism and ship our own `Loader` state
  (named after the original's `AppModePluginLoader` 0x01413814 / `UIWorld_Loading` 0x01418274 /
  `*Loading...*` 0x013ffae2 / `EventLoadScreenBufferPercentage` 0x01446c18 family).

## Full arg vocabulary

`analysis/arg_vocab.md`, produced mechanically by `tools/arg_vocab.py 0x92b570`: 76 call
sites, 56 distinct names. Notable: `devDirs shipDirs state preload safe noVSync vSync noDev
noPlugins noScripts noSound dumpShaders dumpShadersForPIX dumptelemetry multipleInstances
writeableData userDataDir dataDir dataBaseDir locale baseGame ep1 demo gonzago flock mce mono
stereo pix hostIP clientIP isHost jobPriority jobThreads nodefaultvox nosamplecache
noBackground nofocus noDevEffects noEffectScripts noMaterialScripts showConfigAlerts silent
verboseaudio 4.0 5.1 automation`.
Keys are stored **without** a leading dash; the dash is the shell-side convention.
`-w` is absent from this list → our windowed flag is our own convention (VERIFY row 15).

## Traps hit in session 3b (capture + fault tolerance)

1. **MSVC `signal()` resets the handler to SIG_DFL on every invocation** (C99 semantics).
   Our abort handler *recovers* instead of terminating, so the second `abort()` in the
   process went to the default handler and exited `3`. Observed exactly: hit #1 intercepted
   and recovered, hit #2 vanished with no log line. Fix: re-arm `signal(SIGABRT, handler)`
   as the first statement inside the handler. (`_signal()` also exists; re-arming is
   explicit and survives a future change of install site.)
2. **`GetRenderTargetData` while the source is the current render target hangs.** The first
   capture attempt ran inside the open scene: the request logged, `cap.bmp` was never
   created, and the process had to be killed. Correct slot is **between `EndScene` and
   `Present`** — frame complete, scene closed, back buffer still holding the image.
3. **`GetFrontBufferData` fails with `D3DERR_INVALIDCALL` (0x8876086C) under
   `D3DSWAPEFFECT_DISCARD`** even inside an open scene (`scene_open=1` in the log). DISCARD
   gives no persistent front buffer. Same HRESULT as the earlier `LockRect` failure — an
   HRESULT alone is not a diagnosis; log the call site and the state.
4. **`ETO_OPAQUE` fills the rectangle with the BACKGROUND colour.** With the default white
   background, every atlas cell became solid after the luminance→alpha conversion, so text
   rendered as white bars (caught by looking at the first captured frame). Fix:
   `SetBkMode(OPAQUE)` + `SetBkColor(RGB(0,0,0))` + white text. Guard added: the atlas now
   logs its ink coverage and errors if it is 0% or >90% (this bug showed as ~100%).
5. **`GetTextMetricW` does not exist** — it is `GetTextMetricsW` (verified: 0 vs 3 hits in
   SDK 10.0.26100). `ETO_CLAMPED` does **not** exist in this SDK at all (0 hits).
   `IDirect3DSurface9::GetSurfaceDesc` does not exist either — it is `GetDesc`
   (`shared\d3d9.h:1639`). Pattern: these were all guesses; every one cost a build. Grep the
   header before writing the call.
6. **Cell width must equal the advance.** A 16×16 square cell with a 0.6·cell advance made
   glyphs overlap by ~6 px — bars again. Now the cell is derived from `TEXTMETRICW`
   (`tmAveCharWidth` × `tmHeight` → 7×15 for Consolas at −13) and cell width == advance.
   Also use POINT filtering: LINEAR bleeds neighbouring cells at these sizes.
7. **PowerShell quoting through `Start-Process -ArgumentList` is unreliable**: `-exec`
   `\"_abortNow 9\"` arrived at the app as `"_abortNow 9\`. Do not build a harness that
   depends on shell quoting — added the `name:arg` form (`_abortNow:9`) plus repeatable
   `-exec`, which needs no quotes at all.
8. **Command names must be lower-cased at registration.** `exec_line` lower-cases the input
   before comparing, so `logLevel`/`_abortNow` could never match ("unknown command:
   _abortnow"). Found only by running the verb.
9. **A test that fires outside the guarded region tests nothing.** The first `_abortNow`
   called `abort()` inside its own handler, which runs in `console::poll()` — outside
   `run_guarded`. It now arms a counter that `frame_body()` consumes one-per-frame *inside*
   the guard, which is also the only way to reach the storm limit (>5/1s).
10. **A fully-buffered log hides a working frame loop** (see session-3 trap 11): the tail
    after the last forced flush is lost when the process is killed, so a healthy app looked
    hung at the boot chain. Now every line flushes.

## Traps hit in session 3 (first booting build; each one cost a run)

1. **`setvbuf(f, nullptr, _IOLBF, 0)` is not "let the CRT choose".** NULL buffer means
   "allocate your own of THIS size", and 0 trips the UCRT assert
   `setvbuf.cpp:57: 2 <= buffer_size_in_bytes && buffer_size_in_bytes <= INT_MAX` → modal
   dialog → exit `0xC0000409` with a **zero-byte log**. Use an explicit size (64 KiB).
   Corollary: `diag::install()` must run *before* `log::init()`, and `early_neutralise()`
   must be the first statement of `wWinMain`, or the harness cannot catch an assert raised
   by the very code that stands up the harness.
2. **`std::mutex` is non-recursive.** `console::init` held `g_mux` while calling `reg()`,
   which locks it again → `future_error` (0xE06D7363). Our own harness produced the full
   symbolised stack (16 frames, file:line), which is the proof the harness works. Rule: never
   call a public locking API while holding the same mutex.
3. **`D3DPOOL_DEFAULT` texture without `D3DUSAGE_DYNAMIC` rejects `LockRect`** with
   `D3DERR_INVALIDCALL` (0x8876086C) — silently, no capability negotiation. For a static
   upload use a `D3DPOOL_SYSTEMMEM` staging texture + **device** `UpdateSurface`
   (`shared\d3d9.h:464`: `UpdateSurface(pSrcSurface, pSrcRect, pDstSurface, pDstPoint)`).
   There is **no** texture-level `UpdateSurface` and **no** `UpdateSourceRect` in SDK
   10.0.26100 (0 hits in `shared\d3d9.h` and `um\d3d9helper.h`) — two guesses were wrong
   before the header was read.
4. **`GetPresentationParameters` does not exist on `IDirect3DDevice9`** in this SDK (it is
   9Ex-only). Cache the `D3DPRESENT_PARAMETERS` yourself instead of querying the device —
   which also matches the target, since it imports only `Direct3DCreate9`.
5. **`Clear()` outside a scene is silently dropped** by D3D9. Order: `begin_frame()` (returns
   bool) → `clear()` → draw → `end_frame()`; Present only if the scene actually opened.
6. **`SearchPath` collides with the Win32 macro** `#define SearchPath SearchPathW`. Naming a
   type `SearchPath` produced "std::vector: 'SearchPathW' is not a valid template type".
   Renamed to `DirEntry`. Do not name types after Win32 function names.
7. **No preprocessor directives inside macro arguments.** `LOG_INFO("app", "%s",
   ifdef…)` is a hard C2121; hoist the `#ifdef` above the call into a `const char*`.
8. **`_fsopen_s` does not exist**; the API is `_fsopen` (returns `FILE*`). `_SH_DENYNO` needs
   `<share.h>`. `_CRT_REPORT_HOOK` is `__cdecl`, not `NTAPI`. The SIZE_MINIMIZE wParam is
   `SIZE_MINIMIZED`. `/OPT:NO` is not a valid link option (`/OPT:NOREF /OPT:NOICF` are).
   `SymFromAddr`'s displacement is `PDWORD64`, `SymGetLineFromAddr64`'s is `PDWORD`.
   `StackWalk64` takes 10 args including `SymFunctionTableAccess64`/`SymGetModuleBase64`.
   `STACKFRAME64` has no `Size` member.
9. **C4611 fires for ANY call from a frame containing `_setjmp`**, not just ones with
   destructors. So the setjmp lives in an `extern "C"` trampoline that only calls a function
   pointer, with the warning suppressed and the accepted cost written down (a lock held by
   an aborted callee stays held; the log's CS is only entered via `raw_write` on the crash
   path, so the handler cannot deadlock on it).
10. **A per-site log ledger keys on the call site, so it consolidates *different* messages
    that share one site.** All 10 boot markers come from one `mark()` call, so marker 9
    printed "x9, further repeats suppressed". One-per-run lines need `LOG_ONCE_*`.
11. **`PrintWindow` cannot capture D3D9 content**: it returned an all-black client area
    (1 colour × 1600 samples) for a window that was verifiably rendering. Machine evidence
    for a rendered frame needs an in-engine capture
    (`GetFrontBufferData` → `GetRenderTargetData` → `LockRect` → BMP). Do not conclude
    "nothing rendered" from it, and do not conclude "it rendered" from it either.
12. **`[System.Drawing.Color]` is a struct** — it has no `Dispose()`, and sampling a Bitmap
    after `Dispose()` is a use-after-free on the GDI side (`tools/shot.ps1`).

## Traps hit in session 1 (recon; kept for the class of mistake, not the detail)

1. **`.rdata` is not all text.** A UTF-16 scan that decodes a whole section with
   `errors="ignore"` silently drops unpaired surrogates and every later address drifts, which
   produced a phantom `devDirs` inside a float constant pool at 0x013f8592 (real content there:
   `.eBT` behaviour-tree records + `0x40490fdb` = 3.14159f). Fix: walk 16-bit units explicitly
   (`tools/strings_scan.py`). Symptom of the bug: a "string" whose neighbours are floats.
2. **`pefile` stores RVA, not VA** in section headers and data directories — subtract the image
   base first. Also `fast_load=True` skips the debug/resource/import directories: re-open fully
   parsed for CodeView, RT_VERSION and imports. Cost me three wrong "no debug directory" claims.
3. **`.data` VirtualSize (0x212764) > SizeOfRawData (0xc4c00)**: the BSS tail has no file bytes.
   Any range test must use `max(vsize, raw)` or you lose addresses near the end of a section.
4. **The certificate is at a file offset** in `DATA_DIRECTORY[4]`, not an RVA. `dwLength` includes
   the 8-byte header. `cryptography` 44 returns a plain list from `load_der_pkcs7_certificates`,
   older versions returned `(certs, crls)` — handle both.
5. **RT_VERSION key is `VS_VERSION_INFO`** (with the underscore, 15 chars → 32 bytes with the
   NUL), not `VS_VERSIONINFO`. Off-by-a-NUL put the fixed info 2 bytes off and every version read
   as 0. And for text blocks (`wType==1`) `wValueLength` counts **characters**, not bytes.
6. **psubman/PowerShell**: `head` does not exist (use `Select-Object -First N`); heredocs `<<'PY'`
   do not work (use the edit tool); the console is cp1251 and dies on U+FFFD, so print via
   `sys.stdout.buffer.write(...encode('ascii','replace'))` and keep UTF-8 in the report files.
7. **My own verification thresholds were wrong three times** (see `analysis/unpacked_verification.md`
   §Method notes): CC-run *start* alignment (it's the *end* that aligns), raw E8/E9 counting as a
   branch census, `FF 25` thunks as the only import form, and rates compared to nothing instead of
   to the chance baseline (random addresses are 1/16 aligned; my opcode set covers 64.5% of bytes).
   Rule going forward: **state the chance baseline next to every rate, and always run the control.**

## ⚠️ Do not repeat

- Do **not** try to disassemble `refs\bins\ReleaseGA\SporeApp.exe`. Its `.text` is encrypted;
  objdump output there is garbage (304/306 absolute operands out of image, 30% valid branches).
  Use the `.unpacked.exe`.
- Do **not** use any address from the Beta190208 PDB, from `Also1`/`Also2`, or from `GonzagoGL.exe`
  against the GA target. Different links (0/12 identity metrics match). Names/approach only.
- Do **not** assume Spore is RenderWare-free because there are no `Rw*` imports. It is statically
  linked RW on D3D9; find the plugin registry inside `.text`.
- Do **not** decode a whole section as UTF-16 in one shot (trap 1).
- Do **not** treat `Also2\*.bgm` as a distinct asset format — it is the encrypted `.text` copy of
  the same GA exe (entropy 8.000, identical layout).
