# PROGRESS — boot / load chain, one table

Status legend: ✅ done+verified · 🚧 partial · ❌ not started · ⛔ blocked (reason in RESUME.md)

## Yardstick

`N_verified` = rows in `VERIFY.md` at status `DISASM-VERIFIED`. Currently **15** (rows 1-12,
17-19, 21). Runtime yardsticks, all machine-checkable, report after every run:

| yardstick | value | how |
|---|---|---|
| boot markers OK | **10/10** | `yardstick:` in `OpenSpore.log` |
| packages **index-parsed** | **57 / 57** (0 kind-unknown, 0 invalid) | `packages:` |
| records parsed | **150,945**, 0 invariant violations | `python tools/pkg_kinds.py` |
| state hash | `Loader` = **0xd7d04c04** (FNV-1 + tolower) | `-exec state:-list` |
| fps | **33.4 / 31.2** | `fps=` |
| build warnings | **0** | `cmake --build` output |

## Boot chain (our executable)

| # | Step | Status | Evidence / where it lives |
|---|---|---|---|
| 1 | `diag.install` (no modal dialogs, non-fatal asserts, exception filter) | ✅ | log line 1 of every run; proved itself by symbolising the mutex crash |
| 2 | `log.init` (share-readable file + consolidated console) | ✅ | `OpenSpore.log` tailable while running (`_SH_DENYNO`) |
| 3 | `cmdline.parse` (`-w -devDirs` per PROMPT.md) | ✅ | `flags: windowed=1 devDirs=1 useDevDirs=1` |
| 4 | `fs.init` (reversed devDirs/shipDirs contract) | ✅ | 6 search paths: `Data/BaseData/Locale/Config/UserData/cwd` |
| 5 | `window.create` (+ our icon) | ✅ | `window ... 1296x759 (client 1280x720) windowed=1 icon=embedded` |
| 6 | `d3d.device` (`Direct3DCreate9`, same single import as the original) | ✅ | `device windowed 1280x720 vsync=1 (desktop 1692x919 fmt=22)` |
| 7 | `font.atlas` (GDI → staging texture → `UpdateSurface`) | ✅ | `font atlas ready: cell=16 cols=16 glyphs=95` |
| 8 | `console.init` (dev commands in Debug) | ✅ | 5 commands: `help logLevel echo quit state` |
| 9 | `states.register` (AppStateManager analog + Loader) | ✅ | `registered 'Loader' id=0x69cdcb22` (FNV-1a, as the original stores IDs) |
| 10 | `state.goto` | ✅ | `-> Loader [0x69cdcb22]`, `enter Loader state` |
| 11 | frame loop alive | ✅ | `frame loop alive (2 frames rendered)` |
| 12 | loader screen visible | ✅ (human) | user: "О да я видел полоски загрузки". **Not** machine-verified: `PrintWindow` returns an all-black client area for D3D9 content, so `tools/shot.ps1` cannot prove it. Needs the in-engine capture below. |

## Load chain (the game's own)

| Step | Status | Note |
|---|---|---|
| Locate `Data/`, `BaseData/`, `Locale/`, `Config/`, `UserData/` | ✅ | our interpretation, from the reversed prefixes |
| Assets present | ✅ | user dropped ~5.7 GB: `Data\` (base game, 7 `.package`) + `DataEP1\` (GA, 5 `.package` + `Config`) + 17 locale dirs. **Extension is `.package`, not `.package.big`** |
| Open + validate `.package` header | ✅ | **57 distinct files, 0 rejected.** Reader replays the validator's own clauses (`0x008d90b0`) in order, with its reasons |
| Parse fixed-stride index | ✅ | 31 files / 17,901 records, 0 invariant violations (`offset < idx_off` and `offset+size <= idx_off`) |
| Parse **all** indexes | ✅ (session 5) | the index's first dword is a **shared-field bit mask**, so `prefix = 4+4*popcount(K)`, `stride = 32-4*popcount(K)` → **57/57 files, 150,945 records, 0 violations** |
| Split the former "sentinel" u32 | ✅ (session 5) | it is `compression:u16 + flags:u16`; 0xFFFF=RefPack (98,407 recs, payload starts `10 FB`, packed<mem), 0x0000=raw (52,538 recs, never `10 FB`, packed==mem) — verified by probing payload bytes |
| Read a resource's bytes | ✅ | `pkg:-dump,0,3,res0.bin` → 1371 B out of `Data\Locale\cs-cz\NameGeneration.package` (type 0x024A0E52, RefPack) |
| RefPack decode | ❌ | `tools/refpack.py` is **UNVERIFIED** and faults on a real stream; its table came from a third-party doc (which is also wrong about endianness). Aimed route in IDEAS.md §6: disasm `0x0092c5b0` |
| Resource type-id → name | 🚧 | ids are **not** FNV of names (0/6 tested); the answer is the registry lookup in `upstream/SporeMaster3/data/reg_type.txt` (593 lines). Top ids known: 0x01A510BB (98%), 0x2F4E681B=`rw4`, 0x024A0E52 (Czech lexicon) |
| `group`/`unknown` semantics | ❌ | the bit-mask model names them but not their meaning |
| RenderWare plugin registration family | ❌ | field guide §3; start from `vb -<used>` at 0x0140a980 |
| RW stream reader (`RpClumpStreamRead`) | ❌ | blocked on the above |
| `MoviePlayer` / `.bik` splash | ❌ | field guide: stub the intro path, keep windowed device creation ours |
| Real game states (space/creature/tribe/civilisation) | ⛔ | they are `.eBT` **script data**, not code; need assets |
| Developer console command table | 🚧 | 5 of ours; the original's is enumerated in `analysis/arg_vocab.md` |
| Audio (DSOUND #1/#9) | ❌ | |
| Input (DINPUT8 polled) | ❌ | harness must use `SendInput`, not `PostMessage` (field guide §2) |
| Save/load, `UserData/` writes | ❌ | |
| Steam entitlements (`SteamAPI_Init/Shutdown/SteamApps`) | ❌ | the DRM seam; 3 calls only |

## Infrastructure

| Item | Status |
|---|---|
| Static-analysis toolchain (`tools/*.py`) | ✅ 18 tools, all evidence-producing |
| Build: CMake + MSVC, x86 Debug, icon, version resource | ✅ clean build, 0 warnings |
| x64 build | ❌ CMake has an `OS_ARCH` switch; never exercised |
| Crash/stack harness | ✅ proven live (symbolised `file:line` stack from a real throw) |
| Abort-resilient frame loop (`diag::run_guarded`) | ✅ **exercised**: 5 aborts recovered inside the guard, 6th in 1 s → storm exit `0x4F53AB02` |
| Log consolidation (per-site ledger) | ✅ live; boot markers moved to `LOG_ONCE` after the ledger swallowed 9/10 |
| Screenshot harness | 🚧 `tools/shot.ps1` works for GDI windows, **useless for D3D9 content** (PrintWindow returns all-black) |
| In-engine frame capture | ✅ `GetBackBuffer(0,0,MONO)` → `GetRenderTargetData` → SYSTEMMEM → `LockRect` → 32bpp BMP, executed **between `EndScene` and `Present`**. `-capture <path> -captureAfter <n>`, and the `capture <path>` console verb. Log reports ink coverage (`nonblack=9502/9502`), and `analysis/shots/loader_capture.png` shows readable text |
| Unattended run script | ✅ `-exec <cmds>` / `-execAfter <n>` / `-quitAfter <n>` + repeatable `-exec`, with `name:arg` so no shell quoting is needed |
| Fault-tolerance exercised | ✅ `_abortNow <n>` arms one abort **per frame inside `run_guarded`**: 5 recovered with the loop continuing (frames kept counting), the 6th within 1 s hit the storm limit and exited `0x4F53AB02`. Single-abort case still exits clean (`frames=40 aborts=1 code=0`) |

## Explicitly not started (roadmap, not forgetting)

- RenderWare reimplementation/bridge (field guide §3) — the big one.
- RHI wrapper (`render/rhi.h`) so an OpenGL backend is possible later → `IDEAS.md` §1.
- Package reader, unpacker CLI, beta assets, multiplayer, own Sporepedia server → `IDEAS.md` §2.
