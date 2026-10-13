# OpenSpore feature map

> **Generated** from `analysis/features.json` by `tools/feature_map.py` - do not edit by hand.
> Edit the JSON (add a feature, change a `status`), then run the script to refresh this list
> AND the picture `analysis/shots/reverse_map.png`. Both come from the same source, so they
> cannot drift apart.

**Engine reversal: 80.0%** | **Overall: 73.0%** | done 163 / partial 20 / todo 54 (237 features)

**Legend**

| | status | meaning |
|:---:|---|---|
| ✅ | done | reversed **and** implemented **and** verified |
| 🚧 | partial | reversed or implemented in part |
| ⬜ | todo | not started |

The picture (`analysis/shots/reverse_map.png`) shows the same data as a grid of squares.

## Summary

| category | progress | done | partial | todo |
|---|:---:|:---:|:---:|:---:|
| harness | **93%** | 13 | 0 | 1 |
| startup | **95%** | 25 | 3 | 0 |
| launch args | **97%** | 28 | 2 | 0 |
| packages (DBPF) | **89%** | 31 | 4 | 2 |
| gmdl (meshes) | **90%** | 21 | 3 | 1 |
| rw4 (models) | **53%** | 7 | 3 | 6 |
| render | **80%** | 20 | 0 | 5 |
| formats | **18%** | 2 | 3 | 15 |
| gameplay | **0%** | 0 | 0 | 16 |
| tooling | **91%** | 14 | 1 | 1 |
| platform | **25%** | 2 | 1 | 7 |

---

## harness — 93%

*13 done · 0 partial · 1 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | diag install | no modal dialogs (field guide 4) |
| ✅ | done | non-fatal asserts | diag.cpp |
| ✅ | done | exception filter | SetUnhandledExceptionFilter |
| ✅ | done | symbol handler | dbghelp SymInitialize |
| ✅ | done | stack walk | symbolised file:line |
| ✅ | done | log file | share-readable OpenSpore.log |
| ✅ | done | log console | consolidated per call-site ledger |
| ✅ | done | log levels | -logLevel fatal..trace |
| ✅ | done | log once | one line per call site |
| ✅ | done | console cmds | help/logLevel/echo/quit/state/pkg |
| ✅ | done | console stdin | reads the parent console |
| ✅ | done | fault tolerance | 5 aborts recovered; storm limit |
| ✅ | done | frame capture | BMP between EndScene and Present |
| ⬜ | todo | config file | no ini/config layer yet |

---

## startup — 95%

*25 done · 3 partial · 0 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | wWinMain | thin entry -> app::run |
| ✅ | done | CRT startup | GetStartupInfo/argv walk documented |
| ✅ | done | cmdline tokenizer | quote/backslash aware |
| ✅ | done | first-arg fix | lpCmdLine has no program name |
| ✅ | done | arg lookup helper | 0x0092b570 |
| ✅ | done | arg census | 76 sites, 60 names |
| ✅ | done | devDirs/shipDirs | precedence 0x7e838e |
| 🚧 | partial | path prefixes | strings+order; assembly not walked |
| ✅ | done | window class | CreateWindowEx |
| ✅ | done | window create | client 1280x720 default |
| ✅ | done | app icon | assets/OpenSpore.ico embedded |
| ✅ | done | message pump | PeekMessage loop |
| ✅ | done | d3d9 create | single import Direct3DCreate9 |
| ✅ | done | display mode | 32-bit requirement 0x7e74a0 |
| ✅ | done | present params | DISCARD swap, interval one |
| ✅ | done | device lost/reset | TestCooperativeLevel |
| ✅ | done | resize | WM_SIZE -> Reset |
| ✅ | done | font atlas | GDI rasterised, 95 glyphs |
| ✅ | done | state manager | AppStateManager analog |
| ✅ | done | state register | FNV-1 ids, dup rejected |
| ✅ | done | startup chain traced | entry -> WinMain 0xf489e0 -> app_main 0xf48810 -> run 0xf47e90 |
| ✅ | done | main loop located | 0x8482f0: PeekMessageW/WM_QUIT/dispatch/Translate/DispatchMessageW |
| ✅ | done | single-instance guard | Global\SporeEditorOrGame; -multipleInstances bypasses |
| 🚧 | partial | app-mode host | CreateAppMode 0x7e8a00 found; ctor 0x7e76e0 not walked |
| ✅ | done | state goto | transitions, no nesting |
| 🚧 | partial | startup -state | key known; goto via vtable |
| ✅ | done | boot markers | 10/10 |
| ✅ | done | clean shutdown | -quitAfter / WM_CLOSE |

---

## launch args — 97%

*28 done · 2 partial · 0 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | -w windowed | device fn 0x7e74a0 |
| ✅ | done | -f fullscreen | f tested before w |
| ✅ | done | -r WxH | %dx%d 0x1413ab8 |
| ✅ | done | -devDirs |  |
| ✅ | done | -shipDirs | wins over devDirs |
| ✅ | done | -dataDir |  |
| ✅ | done | -userDataDir |  |
| ✅ | done | -dataBaseDir |  |
| ✅ | done | -locale | en-US default |
| 🚧 | partial | -state | startup state name |
| 🚧 | partial | -preload | preloaded state |
| ✅ | done | -safe |  |
| ✅ | done | -noSound |  |
| ✅ | done | -vSync/-noVSync |  |
| ✅ | done | -noPlugins |  |
| ✅ | done | -noScripts |  |
| ✅ | done | -multipleInstances |  |
| ✅ | done | -baseGame/-ep1 | content selection |
| ✅ | done | -demo |  |
| ✅ | done | -gonzago | editor/proto mode |
| ✅ | done | -flock/-mce |  |
| ✅ | done | -mono/-stereo | audio mode |
| ✅ | done | -jobThreads |  |
| ✅ | done | -jobPriority |  |
| ✅ | done | -writeableData |  |
| ✅ | done | -dumpShaders | + -pix/-dumpShadersForPIX |
| ✅ | done | -hostIP/-clientIP | multiplayer keys |
| ✅ | done | -isHost |  |
| ✅ | done | -pollenLogin | Sporepedia login |
| ✅ | done | -automation |  |

---

## packages (DBPF) — 89%

*31 done · 4 partial · 2 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | magic | inline immediate 0x46504244 |
| ✅ | done | header 0x60 | cmp eax,0x60 |
| ✅ | done | version <=3 |  |
| ✅ | done | count limit | < 0x07FFFFFF |
| ✅ | done | index off/size | == filesize |
| ✅ | done | validator | clauses replayed 0x008d90b0 |
| ✅ | done | index kind mask | bit0 type/bit1 group/bit2 unknown |
| ✅ | done | prefix/stride | 4+4*popcount, 32-4*popcount |
| ✅ | done | shared type |  |
| ✅ | done | shared group |  |
| ✅ | done | shared unknown |  |
| ✅ | done | record type |  |
| 🚧 | partial | record group | semantics open |
| ⬜ | todo | record unknown | semantics open |
| ✅ | done | record instance |  |
| ✅ | done | offset/packed/mem |  |
| ✅ | done | compression field | 0xFFFF = RefPack |
| ✅ | done | flags field | 0x0001 always |
| ✅ | done | resource read | pkg:-dump/-rec/-hdr |
| 🚧 | partial | resource manager | getter 0x8de4b0 (global 0x1667aa0); queries 0x686450/0x847780 |
| 🚧 | partial | content loading (trigger) | trigger type 0x24a0e52; groups resolved: properties, ConfigManager, SporeAppStates |
| ✅ | done | config scripts read | source/engine/config.cpp reads Config/*.txt; cfg verb; ids hex/decimal/(hash) |
| ✅ | done | refpack signature | &0x1FFF == 0x10FB |
| ✅ | done | refpack BE size | 3- or 4-byte |
| ✅ | done | refpack loop | 0x0092c5b0 |
| ✅ | done | refpack literals | +1 count trap |
| ✅ | done | refpack copies | +1 count trap |
| ✅ | done | decode verify | 30000/30000 to memSize |
| ✅ | done | multi-pkg scan | 57 files |
| ✅ | done | scan dedupe | canonical path |
| ✅ | done | unpack by type | pkg:-unpack ... <type> |
| ✅ | done | unpack by name | real resource names |
| 🚧 | partial | type registry | 86/90 named |
| ✅ | done | instance naming | FNV-1+lower 112155/150945 |
| ✅ | done | resource_path | group/instance.type |
| ✅ | done | inventory | 150945 resources, 10.27 GB |
| ⬜ | todo | repack/write | no writer yet |

---

## gmdl (meshes) — 90%

*21 done · 3 partial · 1 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | version dword | = 8 |
| ✅ | done | variable header | {tag,value} groups |
| ✅ | done | bbox[7] | float32 min/max + 1.0 |
| ✅ | done | submesh count | bbox+0x1c |
| ✅ | done | descriptor | found by self-consistency |
| ✅ | done | format code 4 |  |
| ✅ | done | index count |  |
| ✅ | done | reserved 16 | NOT the stride (trap) |
| ✅ | done | index bytes | == 2*count |
| ✅ | done | u16 indices | validated in range |
| ✅ | done | multi-submesh | shared vertex buffer |
| ✅ | done | vertex blk header | 1,N,0,2,0,0x000C0000 |
| ✅ | done | decl count N | 5 or 6 |
| 🚧 | partial | decl entries | N=6 and N=5 lay out attrs differently |
| ✅ | done | vertex count | == max_index+1 |
| ✅ | done | vertex bytes | == count*36 |
| ✅ | done | stride 36 | 9 float slots (some packed) |
| ✅ | done | position float3 | offset 0, verified vs bbox |
| ✅ | done | uv (N=6) | 2 floats at offset 16, in [0,1] 100% |
| ✅ | done | bbox exact check | err 0.0000 on 4 variants |
| 🚧 | partial | attrib semantics | uv found; normals/extra open |
| 🚧 | partial | normals | computed from faces, not read |
| ✅ | done | OBJ export | tools/gmdl_mesh.py --obj |
| ✅ | done | engine draw | mesh.cpp, 3765 verts |
| ⬜ | todo | multiple meshes | one mesh on screen now |

---

## rw4 (models) — 53%

*7 done · 3 partial · 6 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | header 28B | template 0x014f6480 |
| ✅ | done | magic 0x34575289 | rep movs 0x011e3f60 |
| ✅ | done | "w32" field | +0x04 |
| ✅ | done | marker 0A1A0A0D | +0x08 |
| 🚧 | partial | flags 0x00042000 | +0x0c meaning open |
| ✅ | done | version "454" | +0x10/+0x14 |
| ⬜ | todo | object table | after the header |
| ⬜ | todo | chunk table |  |
| 🚧 | partial | writer | 0x6c233d writes sentinel |
| ⬜ | todo | reader | not located (magic memcpy'd) |
| ✅ | done | type registration | 0x007635f0 -> 0x6ad090 |
| ✅ | done | handler dispatch | 0x7633c0 via managers |
| 🚧 | partial | model manager | globals 0x15fd8b0..; parser deeper |
| ⬜ | todo | mesh objects |  |
| ⬜ | todo | material refs |  |
| ⬜ | todo | animation refs |  |

---

## render — 80%

*20 done · 0 partial · 5 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | windowed |  |
| ✅ | done | fullscreen | desktop format |
| ✅ | done | backbuffer |  |
| ✅ | done | depth/stencil | D24S8 |
| ✅ | done | clear |  |
| ✅ | done | begin/end scene |  |
| ✅ | done | present |  |
| ✅ | done | vsync | interval one |
| ✅ | done | overlay batch | one DrawPrimitiveUP |
| ✅ | done | glyph metrics | cell 7x15 advance 7 |
| ✅ | done | text stages | SELECTARG1 texture |
| ✅ | done | image quad | aspect fit |
| ✅ | done | texture upload | create_texture_bgra |
| ✅ | done | png decode | GDI+ from memory |
| ✅ | done | raster native draw | engine decodes raster DXT1/DXT5/raw and draws it |
| ✅ | done | mesh VB/IB | managed buffers |
| ✅ | done | mesh normals | smooth, per-vertex |
| ✅ | done | orbit camera | hand-built matrices |
| ✅ | done | fixed-func lighting | directional light |
| ✅ | done | viewport | mesh sub-viewport |
| ⬜ | todo | d3dx helpers | d3dx9_27 returns with the bridge |
| ⬜ | todo | RW device bridge | field guide 3 |
| ⬜ | todo | RW state blocks |  |
| ⬜ | todo | shaders |  |
| ⬜ | todo | UI (UTFWin) | the game's widget stack |

---

## formats — 18%

*2 done · 3 partial · 15 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | png | 14549 decoded |
| ✅ | done | locale text | Czech lexicon end-to-end |
| ⬜ | todo | locale display | no game text on screen |
| 🚧 | partial | raster | header solved; DXT1/DXT5/raw decoded to PNG |
| 🚧 | partial | dxt / bc | DXT1/DXT5 decode (raster_decode.py); not in the engine yet |
| ⬜ | todo | tga |  |
| ⬜ | todo | animation | 4997 resources |
| 🚧 | partial | prop | structure found (BE count + {key,type,count,size}); 2nd variant open |
| ⬜ | todo | material |  |
| ⬜ | todo | sound (snr) | 29400 resources |
| ⬜ | todo | soundProp | 19296 resources |
| ⬜ | todo | gmsh | 2535 resources |
| ⬜ | todo | blockdata | 3141 resources |
| ⬜ | todo | bea | 2621 resources |
| ⬜ | todo | poll | 2590 resources |
| ⬜ | todo | summary | 2590 resources |
| ⬜ | todo | effect |  |
| ⬜ | todo | shader bytecode |  |
| ⬜ | todo | texture atlas |  |
| ⬜ | todo | model (rw4) | gates the whole content set |

---

## gameplay — 0%

*0 done · 0 partial · 16 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ⬜ | todo | simulation |  |
| ⬜ | todo | creature editor |  |
| ⬜ | todo | cell stage |  |
| ⬜ | todo | creature stage |  |
| ⬜ | todo | tribal stage |  |
| ⬜ | todo | civ stage |  |
| ⬜ | todo | space stage |  |
| ⬜ | todo | terraforming |  |
| ⬜ | todo | missions |  |
| ⬜ | todo | traits/abilities |  |
| ⬜ | todo | save/load |  |
| ⬜ | todo | galaxy gen |  |
| ⬜ | todo | multiplayer | hostIP/clientIP/isHost args |
| ⬜ | todo | Sporepedia | pollen_metadata resources |
| ⬜ | todo | achievements |  |
| ⬜ | todo | scripts/mods |  |

---

## tooling — 91%

*14 done · 1 partial · 1 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | feature map | features.json -> PNG + md (this map) |
| ✅ | done | function map | oracle .text candidates vs our named |
| ✅ | done | PDB symbol map | 136,433 names by subsystem |
| 🚧 | partial | address guesser | 220 guesses, 39 HIGH; families not members |
| ✅ | done | refpack verifier | 30000/30000 to memSize |
| ✅ | done | startup map | analysis/startup.md: entry->WinMain->app_main->run |
| ✅ | done | prop scanner | prop_scan.py + prop.py (partial) |
| ✅ | done | arg census | 76 sites, 60 names |
| ✅ | done | resource inventory | 150945 resources, 90 types |
| ✅ | done | index validator | 0 violations on 150945 records |
| ✅ | done | build identity | which reference build matches |
| ✅ | done | name resolver | 112155/150945 names |
| ✅ | done | mesh exporter | gmdl -> OBJ + PNG render |
| ✅ | done | bmp->png capture | view in-engine frames |
| ✅ | done | call-graph / xref | callers.py, xref.py |
| ⬜ | todo | pdb type records | struct/class layouts (level 1 for types) |

---

## platform — 25%

*2 done · 1 partial · 7 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | x86 build | MSVC x86 Debug, 0 warnings |
| ⬜ | todo | x64 build | CMake switch exists |
| ⬜ | todo | beta assets | KeyMap available |
| ⬜ | todo | loose dirs read | unpacked trees as data source |
| 🚧 | partial | standalone CLI | -headless works |
| ⬜ | todo | installer/packaging |  |
| ⬜ | todo | config file |  |
| ⬜ | todo | localization | locale packages decoded only |
| ✅ | done | crash reporting | dbghelp stacks |
| ⬜ | todo | reproducible build |  |

