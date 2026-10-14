# OpenSpore feature map

> **Generated** from `analysis/features.json` by `tools/feature_map.py` - do not edit by hand.
> Edit the JSON (add a feature, change a `status`), then run the script to refresh this list
> AND the picture `analysis/shots/reverse_map.png`. Both come from the same source, so they
> cannot drift apart.

**Engine reversal: 83.2%** | **Overall: 76.6%** | done 195 / partial 28 / todo 50 (273 features)

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
| startup | **96%** | 33 | 3 | 0 |
| launch args | **97%** | 28 | 2 | 0 |
| packages (DBPF) | **90%** | 33 | 4 | 2 |
| gmdl (meshes) | **91%** | 24 | 3 | 1 |
| rw4 (models) | **53%** | 7 | 3 | 6 |
| render | **82%** | 20 | 1 | 4 |
| formats | **60%** | 20 | 9 | 12 |
| gameplay | **6%** | 1 | 0 | 17 |
| tooling | **91%** | 14 | 1 | 1 |
| platform | **30%** | 2 | 2 | 6 |

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

## startup — 96%

*33 done · 3 partial · 0 todo*

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
| ✅ | done | Title UI state | a Title state (real UI atlas bg) auto->Loader; demonstrates transitions |
| ✅ | done | state register | FNV-1 ids, dup rejected |
| ✅ | done | startup chain traced | entry -> WinMain 0xf489e0 -> app_main 0xf48810 -> run 0xf47e90 |
| ✅ | done | main loop located | 0x8482f0: PeekMessageW/WM_QUIT/dispatch/Translate/DispatchMessageW |
| ✅ | done | single-instance guard | Global\SporeEditorOrGame; -multipleInstances bypasses |
| 🚧 | partial | app-mode host | CreateAppMode 0x7e8a00 found; ctor 0x7e76e0 not walked |
| ✅ | done | state goto | transitions, no nesting |
| ✅ | done | startup -state | colon form -state:Name works; confirmed by the shipped script's own header |
| ✅ | done | game state machine (DATA) | SporeAppStates trigger recovered: 8,499 B ArgScript, 60 states / 17 modes, entry Main -id 0 -> GalaxyGameEntry |
| ✅ | done | stage progression (DATA) | Cell -> Creature -> Tribe -> Civ -> Space, each with FromLaunchScreen/FromLoadGame/FromLevelEditor |
| ✅ | done | editor set (DATA) | 13 editors share mode Editors; switch by kMessageSwitchEditor 0x22d308b + hash(Setup) |
| ✅ | done | uilayout verb located | the spui door: uilayout -load <name> <id1> <id2> (E3LaunchScreen 0x027996d8 0x0279bf9e) |
| ✅ | done | reversal audit (Ghidra/REA) | 30 claims / 104 checks from WinMain forward: 17 PASS / 1 FAIL / 11 UNKNOWN / 1 SKIP; 9 of 10 first-run FAILs were checker bugs |
| 🚧 | partial | vtable-only function wall | 7 cited addresses uncarved by Ghidra; 6 proven real entries; fix = seeding (seeds.tsv), not more searching |
| ✅ | done | boot markers | 10/10 |
| ✅ | done | clean shutdown | -quitAfter / WM_CLOSE |
| ✅ | done | state chain drains per frame | Manager::drain_pending(), called at the top of each frame body. The bug it fixes: request_goto from OUTSIDE a transition queued forever and nothing ran it, while the log said "chain: queued " - a feature that looked correct while doing nothing. A queued hop is now guaranteed to be attempted, and reentry is still refused. |

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

## packages (DBPF) — 90%

*33 done · 4 partial · 2 todo*

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
| ✅ | done | config scripts read | source/engine/config.cpp reads Config/*.txt (3 files); cfg verb; $vars resolved |
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
| ✅ | done | export raster->BMP | pkg:-unpack ...,raster writes decoded BMP |
| ✅ | done | export gmdl->OBJ | pkg:-unpack ...,gmdl writes OBJ; ~53% parse, rest raw |
| 🚧 | partial | type registry | 86/90 named |
| ✅ | done | instance naming | FNV-1+lower 112155/150945 |
| ✅ | done | resource_path | group/instance.type |
| ✅ | done | inventory | 150945 resources, 10.27 GB |
| ⬜ | todo | repack/write | no writer yet |

---

## gmdl (meshes) — 91%

*24 done · 3 partial · 1 todo*

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
| ✅ | done | decl count N | 3/4/5/6; sets the stride (24/28/?/36) |
| 🚧 | partial | decl entries | N=3/4/5/6 lay out attributes differently |
| ✅ | done | vertex count | == max_index+1 |
| ✅ | done | vertex bytes | == count*36 |
| ✅ | done | variable stride | stride = vertex_bytes/vertex_count; N=3/4/6 -> 24/28/36 |
| ✅ | done | position float3 | offset 0 at every stride; bbox exact on 1747/4209 |
| ✅ | done | uv (N=6) | 2 floats at offset 16, in [0,1] 100% |
| ✅ | done | textured mesh draw | mesh.cpp applies the raster texture via UVs (MODULATE) |
| ✅ | done | model<->texture binding | raster with the same instance id (865/865 match); Loader binds it |
| ✅ | done | model cycling | Loader draws a 2x2 grid of four real models, each with its texture |
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

## render — 82%

*20 done · 1 partial · 4 todo*

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
| 🚧 | partial | UI (UTFWin) | 748/748 sequential SPUI files and child graphs; visibility and pointer picking use the graph. Rendering remains diagnostic geometry/text; drawables, layout/Z WinProcs, general commands and game modes remain incomplete. analysis/spui-binary.md |

---

## formats — 60%

*20 done · 9 partial · 12 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | png | 14549 decoded |
| ✅ | done | locale text | Czech lexicon end-to-end |
| ✅ | done | locale display | the game's own labels on our Title screen (New Game 0x05B08767, Options 0x00010009, Exit 0x045BEDD8) + the txt verb |
| 🚧 | partial | raster | header solved; DXT1/DXT5/raw decoded to PNG |
| 🚧 | partial | dxt / bc | DXT1/DXT5 decode (raster_decode.py); not in the engine yet |
| ⬜ | todo | tga |  |
| ⬜ | todo | animation | 4997 resources |
| 🚧 | partial | prop | structure found (BE count + {key,type,count,size}); 2nd variant open |
| ⬜ | todo | material |  |
| ⬜ | todo | sound (snr) | 29400 resources |
| ⬜ | todo | soundProp | 19296 resources |
| 🚧 | partial | spui layout | GA binary reader 0x82ce10 / property dispatch 0x82b3f0 recovered; 748/748 exact EOF, 93,726 objects, 22,384 windows, 20,659 Children links. Object graph known; widget/drawable behavior remains partial. Previous XML-tokenizer interpretation was the wrong loader branch. |
| ✅ | done | spui token alphabet | GA 0x82b3f0: id:u32/type:u16/count:u16; 19=count u16 object indexes, FFFF=null; 20=struct metadata then nested properties; 18=UTF-16 or table/key. Object marker 5FF5 is real, not a type code. Unknown ids preserve framing. 748/748 exact EOF. |
| ✅ | done | spui schema (the dictionary) | 14 `xml` resources = the shipped Spore UI Designer project: 306 proxy ids (66 Class + 240 Property), 227 present in spui, 748/748 layouts carry them |
| ✅ | done | spui windows | Sequential reader: 93,726 objects, 22,384 windows, 18,232 Areas, 20,659 child links; Python/C++ fixture verifier and 486 malformed-input negative cases. Raw Area is LTRB; placed rectangle applies ancestor origins as our current presentation policy. |
| ✅ | done | spui drawn on screen | Title renders the real layout at real coordinates with the game's own captions (Create a New Game / color picker / Selected Color) over the real UI atlas |
| ✅ | done | spui alphas | the shipped ARGB is meaningful (0x007F7F7F transparent); forcing alpha opaque turned the screen into slabs - use as stored + outline |
| ✅ | done | spui all 748 layouts | 748/748 resources consumed exactly to EOF in Python and C++; 1,183,787 exact properties, 20,659 child links. 524 instance ids / 224 shadows (18 size differences). Corpus fixture verifier also rejects 486 malformed inputs atomically. |
| ✅ | done | layout table -> text | Caption ref is FFFF/table/key. Exact sequential reader retains 2,152 inline captions and 1,426 refs; 1,384 resolve (97.1%). Historical 1,391 included wrong-table answers from the fixed map-key narrowing bug. |
| ✅ | done | nested packages (DBPF in DBPF) | DataEP1/Spore_EP1_Locale_* hold 21 resources of type `package` (0x06EFC6AA), each itself a DBPF, together 1,364 locale resources; the en-us one is instance fnv1_lower("en-us")=0x7080DB55 |
| ✅ | done | locale corpus | 265 distinct tables / 23,933 distinct table-key texts / 32,038 entries / 14,657 keys. Historical 369/37,608 counted duplicate container contributions. |
| ✅ | done | layout -> locale table | code 18 = 0xFFFF|table:u32|key:u32 (a KEY, not just a table). 1,384 of 1,426 refs resolve (97.1%); 2,152 inline. Was 1,391 - and 7 of those were answers from the WRONG table, because m_by_table was map<u32,...> fed a u64 key, so the table half was silently truncated away. |
| ✅ | done | spui full sequential parse | GA 0x82ce10/0x82b3f0/0x82b240: 748/748 exact EOF, Python/C++ match, 486 corrupt inputs rejected unchanged. Former vocabulary-wall explanation was wrong: code 19 references and code 20 structs were misread. |
| 🚧 | partial | gmsh | stable 24-byte header found (BE words + float runs); layout not derived |
| 🚧 | partial | blockdata | vec3 array; first two = min/max bbox |
| ⬜ | todo | bea | 2621 resources |
| ⬜ | todo | poll | 2590 resources |
| ⬜ | todo | summary | 2590 resources |
| ⬜ | todo | effect |  |
| ⬜ | todo | shader bytecode |  |
| ⬜ | todo | texture atlas |  |
| ⬜ | todo | model (rw4) | gates the whole content set |
| ✅ | done | state drives the UI | unchanged from today's first half: DevLauncher's `uilayout -load E3LaunchScreen` executed; 65.9% of pixels differ from our own largest-layout pick. Now with a pointer on top. |
| 🚧 | partial | uilayout's two trailing ids | ARG1 IDENTIFIED: 0x027996d8 is a CONTROL ID of E3LaunchScreen, present in BOTH shipped copies (PatchData and Spore_Graphics). ARG2 0x0279bf9e is NOT a control id in any of the 748 layout resources, is in no resource id field of all 150,945 records, and no registry name hashes to it. So `uilayout -load <name> <control_id> <?>` is addressed by (layout, control) and one operand is still unknown - stated with its size, not guessed at. |
| 🚧 | partial | spui precedence (resource cache) | ASSUMED, not reversed: spui takes the FIRST container (so PatchData overrides base content), text takes the LAST parsed - two different rules in one engine. Measured surface: 18 spui layouts and 3 of 23,933 locale strings could differ. EP1 beating PatchData on locale is provably right (EP1 fixes the patch's typo), so the real rule is not simply 'earliest wins'. |
| ✅ | done | locale at three depths | flat Data/Locale/<lang> + nested in DataEP1/Spore_EP1_Locale_0{1,2} + nested in Data/PatchData (42 en-us tables). 265 distinct tables from 5 containers, 23,933 distinct (table,key) texts. The third depth was invisible for two sessions because the search was scoped by FILENAME (*Spore_EP1_Locale*) instead of by TYPE. |
| ✅ | done | caption gaps classified | spui -gaps splits the 42 unresolved refs into COVERAGE (table instance absent from the corpus: mvj 0x277DEE6A, 14 keys) and SEMANTIC (table present, key missing: 4 keys). `legal` 0xADB810C4 was on the missing list for two sessions and is NOT missing - it ships in PatchData's nested packages. |
| ✅ | done | intra-resource repeated key | 10 of 2,944 locale resources state one key twice (23 occurrences, 2 disagreements, both cosmetic). Our parser keeps one Entry per key LINE so it sees them; a {key:text} map reader cannot. Last occurrence wins (unverified against the original). |
| ✅ | done | input -> widget | Pointer uses actual parent graph, ancestor visibility/enabled and IgnoreMouse. Visible CreatureEditor tile at (311,550) verified; old GGE point hit a hidden child and is superseded. |
| 🚧 | partial | z-order (paint order) | Actual Children graph replaces area sorting. Parent-before-children and reverse picking share visibility, Enabled, IgnoreMouse and parent Clip. Forward sibling order is our current policy; original dynamic Z order/layout WinProcs and renderer scissor remain incomplete. |
| ✅ | done | control id == state id (manual id space) | the widget control ids, the state script's manual -id values, and the layout names share ONE manual id space, exactly as the script's author commented ("assign manual IDs just so I can fill in the UI layout stuff more easily"). Measured by spui -xref: 4,187 distinct control ids x 60 states, 9 real hits, <0.001 expected. Discount 0x00000001 - ids below 0x1000 are block-scoped member indices, the standing trap. |

---

## gameplay — 6%

*1 done · 0 partial · 17 todo*

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
| ✅ | done | click -> state transition | Reconstructed launcher bridge: visible click (311,550) -> control 022D9E01 / command 00E1132F -> CreatureEditor state, exit 0. The old GGE demo activated an invisible descendant and is corrected. Original general command dispatcher and editor behavior are not implemented. |
| ⬜ | todo | state enters but cannot act | the honest remainder: entering GalaxyGameEntry logs `mode GalaxyGameEntry not executed`, because a mode is a named AppMode CLASS in code, not data (analysis/modes.md: 11 of 17 names are standalone strings in the oracle, each registered by a per-class factory; 5 do not exist in this build at all). So the next step after this is the AppMode* classes, and it is now REACHABLE BY CLICKING, which it was not before. |

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

## platform — 30%

*2 done · 2 partial · 6 todo*

| | status | feature | notes |
|:---:|---|---|---|
| ✅ | done | x86 build | MSVC x86 Debug, 0 warnings |
| ⬜ | todo | x64 build | CMake switch exists |
| ⬜ | todo | beta assets | KeyMap available |
| ⬜ | todo | loose dirs read | unpacked trees as data source |
| 🚧 | partial | standalone CLI | -headless works |
| ⬜ | todo | installer/packaging |  |
| ⬜ | todo | config file |  |
| 🚧 | partial | localization | en-us locale tables parsed (18,441 entries) and displayed; per-table key space enforced; other 14 langs unparsed |
| ✅ | done | crash reporting | dbghelp stacks |
| ⬜ | todo | reproducible build |  |

