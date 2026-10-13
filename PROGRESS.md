# PROGRESS — boot / load chain, one table

Status legend: ✅ done+verified · 🚧 partial · ❌ not started · ⛔ blocked (reason in RESUME.md)

**Progress map:** `analysis/shots/reverse_map.png` (picture) and `analysis/features.md` (text
list), both generated from `analysis/features.json` by `tools/feature_map.py`. That is the
project-wide feature view; the tables below are the boot/load chain and the runtime yardsticks.

## Yardstick

`N_verified` = rows in `VERIFY.md` at status `DISASM-VERIFIED`. Currently **21** of 30.
Runtime yardsticks, all machine-checkable, report after every run:

| yardstick | value | how |
|---|---|---|
| boot markers OK | **10/10** | `yardstick:` in `OpenSpore.log` |
| packages **index-parsed** | **57 / 57** (0 kind-unknown, 0 invalid) | `packages:` |
| records parsed | **150,945**, 0 invariant violations | `python tools/pkg_kinds.py` |
| **RefPack decoded exactly** | **30,000 / 30,000**, 0 failures | `python tools/refpack.py --verify --limit 30000` |
| resources enumerated | **150,945**, 10.27 GB, 90 types (86 named) | `python tools/resource_inventory.py` |
| resources written to disk | **2,944 locale + 14,549 png**, 0 failed | `pkg:-unpack,all,<dir>,<type>` |
| solved formats exported | `raster`→BMP, `gmdl`→OBJ (2,770 BMP + 2,216 OBJ from Spore_Content) | `pkg:-unpack,all,<dir>,raster\|gmdl` |
| **whole game exported** | **5,747 textures → BMP** + **4,084 models → OBJ**, all 57 pkgs, 0 failed | `pkg:-unpack,all,<dir>,raster` / `,gmdl` |
| headless unpack | same output, **no window/device**, 7.6 s | `-headless -exec pkg:-unpack,...` |
| **gmdl mesh decoded** | positions reproduce the file bbox **exactly (err 0.0000)**, 4 variants | `python tools/gmdl_mesh.py --pkg <i>` |
| **engine draws a real mesh** | 3765 verts / 6000 tris, fps 32.3 | `analysis/shots/mesh_engine.png` |
| **engine draws a TEXTURED mesh** | real gmdl UVs + real raster texture, fps 32.7 | `analysis/shots/mesh_textured.png` |
| **engine draws native raster** | `raster` DXT1/DXT5/raw decoded in C++ and drawn | `analysis/shots/raster_engine.png` |
| **game config read** | `[config] 63 properties, 83 variables from Config/*.txt`; `cfg <name>` resolves | `cfg -props` / `cfg AudioMasterVolume` |
| **real Spore geometry rendered** | limb / creature / plant | `analysis/shots/gmdl_pkg{48,52,54}.png` |
| real game asset rendered | `vehiclepainticons~/be_basic_shake_05.png` (128×128) | `analysis/shots/hero_real_asset.png` |
| resource names resolved | **112,155 / 150,945 (74.3%)** | `inventory:` in the log |
| game content decoded | 312 B + 10,191 B Czech text, == `memSize` | `pkg:-decode,0,0` / `,0,2` |
| state hash | `Loader` = **0xd7d04c04** (FNV-1 + tolower) | `-exec state:-list` |
| fps | **33.3** | `fps=` |
| build warnings | **0** | `cmake --build` output |
| **function coverage (code)** | **29 / 105,239 (0.021%)** | `python tools/functions.py` |

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
| Write resources to disk by name | ✅ (session 9) | `pkg:-unpack <i\|all> <dir> [type]`; 2,944 locale + 14,549 png, 0 failed; PNG signature verified 200/200 |
| RefPack decode | ✅ **SOLVED** | `source/engine/refpack.cpp`; **30,000/30,000 real records decode to exactly `memSize`** (`tools/refpack.py --verify`). `pkg:-decode` writes the decoded bytes |
| Decode a real asset | ✅ (session 6) | `pkg:-decode,0,0` → 312 B and `,0,2` → 10,191 B of Czech name-generation text (UTF-8, BOM), both exactly `memSize` |
| Write the whole game to disk | ✅ (session 9) | `pkg:-unpack,all,<dir>,png` → **14,549 PNGs, 0 failed**, 254 MB; 200/200 sampled carry the PNG signature, and `analysis/shots/unpacked_png_sample.png` is a real UI atlas |
| Render a real game asset | ✅ (session 10) | `imgfile.*` (GDI+ from memory) → `Device::create_texture_bgra` → `Overlay::image`; the Loader draws a real `png` resource (`analysis/shots/hero_real_asset.png`) |
| Decode a real mesh | ✅ (session 12) | `tools/gmdl_mesh.py`: gmdl layout solved (stride 36, position = first float3); positions reproduce the header bbox **exactly (err 0.0000)** on 4 variants; exported to OBJ and rendered (`analysis/shots/gmdl_pkg{48,52,54}.png`) |
| **Draw a decoded mesh in the engine** | ✅ (session 13) | `source/engine/gmdl.cpp` + `source/render/mesh.cpp`; Loader draws one gmdl into its own viewport; `analysis/shots/mesh_engine.png` shows a real Spore creature |
| **Draw the engine's own texture format** | ✅ (session 13) | `source/engine/raster.cpp` (DXT1/DXT5/raw) → `create_texture_bgra`; Loader prefers a `raster` over a `png`; `analysis/shots/raster_engine.png` |
| Name a resource's type | ✅ (session 6b) | `typeids.inc` (450 ids) + `pkg:-types`; `cs-cz/NameGeneration` = 13 x `trigger` + 1 x `prop` |
| Name group/instance ids | ✅ (session 7) | `naming.*` loads the registries; **112,155 / 150,945 names resolved (74.3%)**; `pkg:-path` prints `LexiconPlanets_CS.trigger` |
| Enumerate the whole game | ✅ (session 7) | `tools/resource_inventory.py`: **150,945 resources, 10.27 GB, 90 types (86 named)**; same on the Loader screen |
| Write resources to disk by name | ✅ (session 9) | `pkg:-unpack <i\|all> <dir> [type]`: **2,944 locale + 14,549 png written, 0 failed**, real files (PNG signature verified 200/200) |
| Export solved formats as standard files | ✅ (session 13) | `pkg:-unpack ...,raster` writes **BMP** (DXT decoded, e.g. 512x512 32bpp) and `...,gmdl` writes **OBJ** (e.g. 29,345 verts / 47,016 tris); 2,770 BMP + 2,216 OBJ from Spore_Content, 0 failed |
| Unpack with no window | ✅ (session 12) | `-headless` skips window/device/atlas (marks 4–6 report `headless: no …`); identical unpack (2,944 files, 27,882,571 bytes, 0 failed) in 7.6 s |
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
