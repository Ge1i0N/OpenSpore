# PROGRESS — boot / load chain, one table

Status legend: ✅ done+verified · 🚧 partial · ❌ not started · ⛔ blocked (reason in RESUME.md)

## Yardstick

`N_verified` = rows in `VERIFY.md` at status `DISASM-VERIFIED`. Currently **3**.
Runtime yardstick (now that a build boots): **boot-chain markers OK = 10/10**.
Both are monotonic and machine-checkable; report both after every run.

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
| Read `.package.big` | ⛔ | no assets yet — user offered to drop them in `Data/` (see RESUME.md) |
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
| Static-analysis toolchain (`tools/*.py`) | ✅ 12 tools, all evidence-producing |
| Build: CMake + MSVC, x86 Debug, icon, version resource | ✅ clean build, 0 warnings |
| x64 build | ❌ CMake has an `OS_ARCH` switch; never exercised |
| Crash/stack harness | ✅ proven live (symbolised `file:line` stack from a real throw) |
| Abort-resilient frame loop (`diag::run_guarded`) | ✅ built, **not yet exercised** — needs a deliberate abort test |
| Log consolidation (per-site ledger) | ✅ live; boot markers moved to `LOG_ONCE` after the ledger swallowed 9/10 |
| Screenshot harness | 🚧 `tools/shot.ps1` works for GDI windows, useless for D3D9 content |
| In-engine frame capture | ❌ **next**: `IDirect3DDevice9::GetFrontBufferData` → `GetRenderTargetData` (SYSTEMMEM) → `LockRect` → write BMP. This is the only machine-checkable visual evidence available without D3DX. |
| Unattended run script | 🚧 ad-hoc in this session; needs `tools/run.ps1` |

## Explicitly not started (roadmap, not forgetting)

- RenderWare reimplementation/bridge (field guide §3) — the big one.
- RHI wrapper (`render/rhi.h`) so an OpenGL backend is possible later → `IDEAS.md` §1.
- Package reader, unpacker CLI, beta assets, multiplayer, own Sporepedia server → `IDEAS.md` §2.
