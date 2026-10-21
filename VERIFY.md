# VERIFY — current evidence ledger

Reviewed **2026-10-15**. Address authority:
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe` (VA, image base `0x00400000`), SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Beta/PDB addresses are **not** GA addresses. Disassembly > shipped data > approach references > inference.

**Evidence types:** static instructions; data/corpus observation; runtime on **our** engine;
host policy; unresolved. None of these alone means the complete original feature is reproduced.

| Claim | GA address / source | Verified result | Limits |
|---|---|---|---|
| Startup | entry `0x11E11E1`, WinMain `0xF489E0`, app_main `0xF48810`, run `0xF47E90` | [startup chain](analysis/startup.md), retained Ghidra audit | subsystem behavior not all implemented |
| Main loop | `0x8482F0`, call site `0xF4840B` | PeekMessageW/Translate/Dispatch loop; window vtable +0x84 | pointer-pump helper is separate (`0x84A050`) |
| Args | `0x92B570`; device creation `0x7E74A0` | `-w/-f/-r` are original flags; w wins | see [argument inventory](analysis/arg_vocab.md) |
| IDs | `0x932E50` | FNV-1 **mul then xor**, lowercase mode for state/resource names | not FNV-1a; type IDs use registry, not this hash |
| DBPF header | `0x8D90B0`, `0x8D8F70` | 0x60-byte header / checks replayed | [package implementation](source/engine/pkg.cpp) |
| Index / compression fields | 57 packages / 150,945 records | shared-field mask, bounds and raw/RefPack split validated | field semantics not all reversed |
| RefPack | `0x92C5B0`, `0x92CCD0`, `0x92CD40` | original loops/signature, 30,000 exact recorded decodes | length check is not a full pixel/content equivalence proof |
| State scripts | `SporeAppStates`, instance `0xD6396913` | 60 states / 17 mode names recovered from ArgScript | script data is not driver behavior |
| AppMode registrations | 10 GA registrations | [mode table](analysis/modes.md) represented in `appmodes.cpp` | enter/tick/render not implemented; missing strings do not prove absent classes |
| GGE primary UI | `0xDDDE30` -> driver vtable `0x147D2B0`; activation-shaped `0xDDD960` -> `0xDEA1B0` -> `0xDE5900` | default string at `0x147D3F4`, initializer `0x136BCE0`, GlobalUI load `0xE035F0`; Main -> GGE presents GlobalUIGGE-9 | default primary UI only, original lifecycle/config overrides/zoom/scene/events not implemented; [evidence](analysis/galaxy-entry.md) |
| Intro post-title gate | handler `0xDEA4B0` / name Intro; update `0xDEAF70`, message `0xDEA820`, MoviePlayer guard `0xFD7550`, DBCheck `0x60B5D0` | phase==6 + idle predicate; DBCheck request before latched notification `0x08085C4C`; one-shot `0xDE9550` call; 2040 controlled-signal checks | runtime observation Unknown; no actual MoviePlayer/DBCheck/state10 side effects; primary UI remains diagnostic, not original Intro visibility; [limits](analysis/gge-intro.md) |
| DBCheck branch executor | `0x60B5D0`, `0x60B340`, `0x60B470` | controlled backend checks bypass, dialog descriptor/root (101 objects / 24 windows), first-callback arm, manager+20/notify/unload and failures | live backend not attached; event is continuation, not DB-success; [evidence](analysis/gge-dbcheck-play.md) |
| State2 Play Spore | ctor `0xE008B0`, vtable `0x147EF50`, name method `0xE00930`; enter `0xDF9620`, refresh `0xDF95A0`, per-entry UI `0xDF9200` | static trace identifies 0xBC game records, per-game UI, scene projection/effect dependencies | no live state2 scene/metadata simulation; original name is not the analyst seed name |
| Catalogue source / projection | `0xB3D3A0` -> global `0x167EAE4`; getter `0xBA6450`, fill `0xBB61C0`; field builders `0xDF7CB0/0xDF7F30` | typed entries preserve order, indices, positions, stage/assets and save-header predicate vs static (0x25,3); 19 grouped controlled checks | header words are NOT a current-save identity; live source, stars.db and generation missing; [evidence/limits](analysis/gge-catalog.md) |
| stars.db serializer prefix | `0x693DE0/0x6928C0`; initializers `0x1325DB0/0x1326120`; load `0xBB8C90` | bounded BE framing, six mixed-endian metadata fields, 19 named opaque manager spans in default mode; 240 constructed checks; `PE` raw/BSS bounds fixed | serializer-stream prefix only, no real-save corpus/outer-transport parity or live producer; explicit wire subset below; [evidence/limits](analysis/starsdb.md) |
| stars.db wire links / StarRecord | list `0xBAE0B0`; context `0x6A06A0/0x6A05F0`; initializer `0x13277D0`; record read `0xBBA6E0` | BE token + optional class wire shape, grid/list validation, 14 serialized StarRecord fields; 362 constructed checks | wire-only modes leave identities unresolved; decoded context subset below; resource fixups, real-save corpus and catalogue producer remain open; [evidence/limits](analysis/starsdb.md) |
| stars.db context index / class bodies | `0x69F1D0/0x69FB90`, CRC `0x69EC20/0x932D70` | decoded index/class readers, envelope total-size/CRC/token/body, class-scoped structural lookup; 126 constructed checks | CRC covers at most 1,000,000 bytes from token; ResourceKey order is instance,type,group; physical adapter below, factories/resource fixups/real-save corpus/runtime missing; [evidence/limits](analysis/starsdb-context.md) |
| stars.db DBPF resource adapter | key serialization `0x8DB310`; lookup `0x8DB6E0`; root factory `0x693CA0/0x6938D0` | exact ResourceKey mapping (instance,type,group), read-only DBPF 3.0/index3, root/context/class structural path; 266 constructed checks | old context constant role names corrected; no real-save corpus/factories/fixups/live producer; [evidence/limits](analysis/starsdb-dbpf.md) |
| StarRecord post-load correction | `0xBBA6E0`; `0xBBA77D JNZ`; global pointer `0x151AC94` initially UTF16 BAD_DATA; generation `0xC8C180`, setter `0xBBA620` | 15 corrected decisions: empty or EQUAL BAD_DATA calls generator; no unbounded retry; profile Unknown/Found/Missing | prior canonical-name/mismatch-cleanup/position-cache interpretation WRONG; services not executed; [correction](analysis/starsdb-postload.md) |
| Save file-header words | initializer `0x131B210`, descriptor `0x1568B08`, probe `0xB2781E..49` | file major/minor IDs 0x04162D88/89, unsigned major>=33 predicate; 116 constructed checks, explicit DBPF + controlled GGE integration | distinct from StarRecord saved versions; unknown/missing/malformed not false/zero; no full save, paths or live producer; [evidence](analysis/save-header.md) |
| PlanetRecord / typed saved adapter | reader `0xB8EBF0`, initializer `0x1322A30`; counted load `0xBBA7B0`, filter `0xBBA900/0xB8DAC0` | structural nested prefixes + six fields, counted keys, first selected planet name and explicit header/terrain → typed saved metadata; 291 constructed checks | no original planet corpus, generator/query services, save discovery/candidates or scene readiness; [evidence](analysis/planet-record.md) |
| GGE star presentation math/plan | projection `0x7C4250/0xDE3C20`, placement `0xDF6110`, config `0xDF9200`, asset default `0x136BC90` | 50 checks, stage-icon/caption/raw flags, explicit camera/margins, two real 152-object/58-window layouts | no camera/effects creation or per-entry mounting; host safety guards differ from original unchecked arithmetic; screen unchanged; [evidence](analysis/gge-star-present.md) |
| GGE host instance / native mount boundary | native load `0xDF7690`, phase1 `0xDF6D80`, attach `0x811C20`, scene init `0xDE46D0/0xDDD150` | owned host graph with object-index mutations/movement/pixel picking; 167 checks on real assets; renderer pixel-layer hook | neither load nor phase1 return proves readiness (phase1 normal returns1); duplicate ControlID 0x026E0002; no native callbacks/plugins/scene or live caller; [evidence](analysis/gge-star-instance.md) |
| GGE phase-1 requirements | `0xDF6D80`; image get/set `0xDE3DB0/0xDE3EB0`; locale `0xDE3F50` | 247 ordered binding-plan checks; image QI0xEF3C47CF distinct from Animated Icon QI0x0106F146; Unknown/duplicate/missing explicit | all inspected native normal returns1, including missing-root skip; host plan executes no services or phase latch; [evidence](analysis/gge-star-phase1.md) |
| GGE image-provider / applied image overrides | ImageDrawable QI `0x97D030`, setter `0x97D070`, getter `0x6C0230`; image interface table `0x14431E8` | exact explicit PNG read/decode + instance-local drawable image override + resolver/painter; 80 checks, actual asset and controlled GPU readback | CF3C4EAE only, no Animated Icon/arbitrary source fallback/full phase/native scene readiness; no automatic GGE caller; [evidence](analysis/gge-image-bindings.md) |
| GGE Animated Icon static selector | QI `0x7F5500`, setter `0x7F5550`, paint `0x7F5B50`, ctor `0x7F5D50` | 159 checks (carrier opens removed); actual five-frame descriptors/direct original atlas decode + per-window index/UV + controlled multi-row GPU readback | separate API; constructor1; invalid indices ignored true; no native normalization/playback/scene readiness; [evidence](analysis/gge-animated-icon.md) |
| GGE star host lifecycle | retained phase1/config/image contracts, centering `0x806E00`, half constant `0x145BE38` | 232 checks: owned open/apply/close, generation tickets, independent action outcomes/retry/per-pass resource reads, UTF name/center/image/frame + direct streaming backend | orchestration is HOST policy, not native phase/dependency replay or readiness; locale refs not loaded text; [evidence](analysis/gge-star-lifecycle.md) |
| GGE direct streaming PNG | existing DBPF/RefPack/PNG contracts; new file/index/resource bounds and 64-bit seeks are HOST implementation | 95 checks; original 942,881,325-byte Graphics atlas direct decode, sparse >2GiB offsets, size/index/key/PNG failures, Windows write-sharing protection and 544 handle cycles | one caller-selected file; no memory carrier/priority/profile scan/scene readiness; [evidence](analysis/gge-streaming-images.md) |
| GGE host scene/camera boundary | `67DCD0` renderer global; `DE3C20` active+58 differs from `DE46D0` controller+198 acquisition; scene/factory/effects static chain | 208 checks: Available catalogue + producer-status gate + explicit active-camera observation, owned projection/placement/layers/pick, stale-frame/identity/budget/reentry guards | controlled camera/metadata/IgnoreMouse picking fixture; no native factory/show/GPU galaxy/live GGE completion; [evidence](analysis/gge-star-scene.md) |
| GGE explicit show/phase case | `DF6D80` cases2..5/equality guard; F8/FC callers `DF7BF0/DF7AC0`; window rect slot34/60 | 195 checks, actual panel41 hit without clearing root IgnoreMouse, flags/names/literal rect copy, case4 returns1 WITHOUT latch+2C | explicit controlled current-phase observations, opaque services Unsupported, no native latch/automatic dispatch; [evidence](analysis/gge-star-transition.md) |
| GGE input/UI-world seam | Handler `+5C DF7BF0`, `+60 DF7AC0`, `+64 DF6110`, `+68 DF6010`; world `DF6490→7C45E0/698B30`, threshold15 | 29 checks: active-ray nearest point-segment world selection vs current-frame UI picking | no callback/event pump/tooltip/CommandID/selection dispatch; no native camera producer; [evidence](analysis/gge-star-input.md) |
| GGEHostPreview | Debug-only host state, real GGEGameStarUIv4/Graphics atlas, explicit scene/camera, normal `input::inject_click` frame path | smoke enters state, clean frame loop and `spui:-click,640,360` logs host card20481; not a native/GGE catalogue test | controlled metadata/camera, no release registration/native callback/catalogue/scene readiness; [evidence](analysis/gge-host-preview.md) |
| GGE catalogue producer boundary | `B3D3A0`, `BA6450`, `DF95A0`, `DF7CB0`, `DF7F30`, `BB61C0`, `BB8C90`, `BB4D40` | manager vectors/order/refill/save persistence contracts retained; 19 typed projection checks | no real stars.db/save corpus, no profile scan/write/live producer; [evidence](analysis/gge-catalog-producer.md) |
| GGE catalogue snapshot | `gge_catalog_snapshot.*` host input port | 10 schema/UTF-8/status/atomic/terrain-key/producer-status checks; explicit `ggepreview:load,<path>` reload | caller-selected file only; no profile/discovery/save/generation, Unknown not empty-ready; [evidence](analysis/gge-catalog-snapshot.md) |
| GGE snapshot inspector | `gge_catalog_snapshot_inspect <snapshot>` | standalone parse/report path, no UI or writes | caller data only; no native/live catalogue claim |
| SPUI binary | `0x82CE10`, `0x82B3F0`, `0x82B240`, `0x13005A0` | 748/748 EOF, exact graph/property framing; 486 negative cases | [binary evidence](analysis/spui-binary.md) |
| Image object | `0x13004A0`, `0x957710/750`, `0x9576F0` | external PNG ref, UV rect and logical dimensions | Image is class `0x01BE6B15`, not ImageDrawable |
| ImageDrawable | `0x97D1E0`, ctor `0x97D840`, table `0x154F690` | None-tiling fixed width/height, scale and alignment | shadow/outline/tiling not implemented |
| Standard / Radio | `0x966A40`, `0x966AD0`, render prefix `0x9655F0` | four/six horizontal state cells, composed with atlas UV | non-tileable branch only; host event adapter not original capture lifecycle |
| Cell archetype field map | `cell` descriptor `FUN_01375e30` @`0x1375e30` (26 entries x 40 B); `cAIData` descriptor @`0x1374725` (0xb4 B, record `+0xE0`); registration thunk `0x1375df0` (type "cell", size 0x31C) | 46 named AI fields + 14 named scalars read by offset; shipped values cross-checked (Ducky speed 11 / chase 8 / food 180; squid food 6 / growCount 4; hero food 10000 / growAmount 0.275) | only the shipped 400 B of the 796 B structure is on disk; the HP/size formula is not recovered |
| Cell tuning curves | `prop` layout: `count(BE)` + `{key(BE),type(BE),item_count(BE),item_size(BE)}` + item data; keys resolved through `reg_property.txt` | 17 curves decoded and keyed by name (evo_points, time_dilation, npc_turn_factor, eye_count, camera_lookahead/lerpfactor, mouse_zoom, ...) | the curve consumers are not recovered; `tools/decode_cell_stats.py` used a 3-arg `refpack.decode` and printed garbage (signature is `decode(data, expect)`) |
| Cell save field names | `FUN_013326c0` (mHealthPoints=0x38, mMaxHealthPoints=0x2c), `FUN_01335000` (mFoodValue, mSpeciesKey, mAge, mArchetype, ...), `FUN_01353d90` (mPosition=0x4, mScale=0x64, mBoundingRadius=0x5c), `FUN_01335fe0` | the runtime creature object is named; the 107 wire ids stay structural | naming a wire id needs the consuming code, not the registration code |
| Cell stage runtime | `Game_Cell` registrar `FUN_00e616c0` @`0xe616c0`; cell data source `FUN_00e4d840`/`0xe4d920`/`0xe4db10` (`Simulator/Cell/Data`, `CellGame/Layers/`) | level spawn, per-cell AI from the archetype, eating/growth, HP, respawn, save round-trip; live capture [tide pool](analysis/shots/cell_stage_tidepool.png) | no RenderWare tide-pool scene, no game audio, HP formula and DNA costs are seeds |
| Visible launcher transition | live `-w -devDirs` | `(311,550)` -> CreatureEditor, exit 0, aborts/asserts 0 | reconstructed control/state bridge; editor still missing |
| Package FILE ownership | our `Package` lifetime regression | destructor closes FILE; 544 cycles at CRT limit 512 | host correctness fix, not a recovered GA algorithm |
| Ancestor Clip / renderer scissor | shipped WindowBase Clip mask `0x400`; host `inherited_clip` + D3D9 | nested/empty/boundary picking tests + real flat/image/text/reset readback PASS | shared presentation policy; does not claim original full renderer/layout parity |

## Explicit presentation policies / remaining unknowns

- Parent-relative translation, forward sibling traversal and reverse picking are shared policies;
  dynamic original Z-order/layout WinProcs remain open.
- SPUI/PNG first-key and locale last-key choices do not establish original resource precedence.
- Pointer/enabled -> drawable state is an adapter. General CommandID dispatch and radio mutation
  remain unimplemented.
- Parser completeness for our shipped corpus does not prove support for every possible SPUI file.
- gmdl/raster gallery works for supported subsets; RW4 scene/animation/material behavior is incomplete.

## Corrections to older claim rows

| Old interpretation | Current verdict |
|---|---|
| State hash = FNV-1a; Loader = 0x69CDCB22 | FNV-1 lower; Loader = **0xD7D04C04** |
| SPUI is XML TokenReader / code 19 has inline count / code 20 is six bytes | Wrong neighboring branch / wrong framing; sequential binary reader supersedes it |
| `Window -> DialogDrawable -> ImageDrawable` wrapper | `DialogDrawable` was an overridden property alias; actual object class links are explicit |
| GGE click `(942,261)` proves a visible menu branch | Hidden ancestor; use the visible CreatureEditor tile |
| PNG misses due to IDs or short run | Package FILE leak exhausted CRT streams; RAII fix regression-tested |
| High feature-map index = game completion / named-candidate ratio = code coverage | Neither claim follows from those metrics |
| StarRecord compares a canonical name and cleans up on mismatch | Global initially BAD_DATA; JNZ skips generation on inequality. Empty/equal BAD_DATA triggers generation; +0x80 purpose not established |
| DF6D80 phase1 false result prevents bad initialization | No false return established; normal exits set AL=1, including missing-root body skip. Return is not dependency validation |

The [archived row ledger](docs/archive/VERIFY-2026-10-15.md) preserves the original measurements,
addresses and mistaken hypotheses. [Audit snapshot](analysis/audit.md) is generated from
`analysis/claims.json`; its PASS/FAIL/UNKNOWN totals are not an audit of every current implementation.

## Next evidence needed

Original Button capture/checked/command behavior, needed tiling/outline
branches, then GalaxyGameEntry driver enter/tick/render. Reproduce current checks via
[README](README.md#проверки), retain bounded evidence and update the feature source after verification.
