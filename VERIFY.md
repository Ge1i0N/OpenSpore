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
| SPUI binary | `0x82CE10`, `0x82B3F0`, `0x82B240`, `0x13005A0` | 748/748 EOF, exact graph/property framing; 486 negative cases | [binary evidence](analysis/spui-binary.md) |
| Image object | `0x13004A0`, `0x957710/750`, `0x9576F0` | external PNG ref, UV rect and logical dimensions | Image is class `0x01BE6B15`, not ImageDrawable |
| ImageDrawable | `0x97D1E0`, ctor `0x97D840`, table `0x154F690` | None-tiling fixed width/height, scale and alignment | shadow/outline/tiling not implemented |
| Standard / Radio | `0x966A40`, `0x966AD0`, render prefix `0x9655F0` | four/six horizontal state cells, composed with atlas UV | non-tileable branch only; host event adapter not original capture lifecycle |
| Visible launcher transition | live `-w -devDirs` | `(311,550)` -> CreatureEditor, exit 0, aborts/asserts 0 | reconstructed control/state bridge; editor still missing |
| Package FILE ownership | our `Package` lifetime regression | destructor closes FILE; 544 cycles at CRT limit 512 | host correctness fix, not a recovered GA algorithm |

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

The [archived row ledger](docs/archive/VERIFY-2026-10-15.md) preserves the original measurements,
addresses and mistaken hypotheses. [Audit snapshot](analysis/audit.md) is generated from
`analysis/claims.json`; its PASS/FAIL/UNKNOWN totals are not an audit of every current implementation.

## Next evidence needed

Renderer Clip/scissor parity, original Button capture/checked/command behavior, needed tiling/outline
branches, then GalaxyGameEntry driver enter/tick/render. Reproduce current checks via
[README](README.md#проверки), retain bounded evidence and update the feature source after verification.
