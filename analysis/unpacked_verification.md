# Verifying `SporeApp.exe.unpacked.exe`'s .text is a correct decryption

- candidate : `refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` (24,895,528 bytes)
- control   : `refs\bins\ReleaseGA\SporeApp.exe` (encrypted .text, known garbage)
- `.text`   : 0x00401000..0x013cbf2a (16,560,128 bytes), entropy 6.638 vs 8.000
- `.rdata`/`.data`/`.rsrc`/`.reloc`/`.tls`/`CONST` are **byte-identical** between the two
  files (sha256, `analysis/build_identity.md`), so agreement between decrypted code and
  those sections is evidence, not coincidence.

Every metric is reported against an explicit **chance baseline**, because the control's
denominators are degenerate: encrypted bytes almost never form 0xCC runs at all (241 runs vs 83114), so a *rate* on the control is meaningless without
the baseline (random alignment = 0.0625, random opcode-in-set = 0.645).

## Metric table with control

| # | invariant | unpacked | encrypted (control) | chance baseline | verdict |
|---|---|---|---|---|---|
| A | int3 run ends 16-aligned | 83,046/83,114 = 99.92% | 14/241 = 5.81% | ctrl expect 6.25% | **PASS** |
| A2 | int3 run **count** (padding exists at all) | 83,114 | 241 | random data has ~no CC runs | **PASS** |
| B | call/jmp targets valid (exact, first 0x10000 of .text) | 1,312/1,312 = 100.00% | 89/290 = 30.69% |  | **PASS** |
| C | absolute ds: operands: in-image + offender structure | 166/173 in-image; offenders={'0x0': 7} | 2/306 in-image; offenders: 304 distinct of 304 | real code's offenders are 0x0, not noise | **PASS** |
| D | IAT slots referenced somewhere in .text | 495/495 | 3/495 |  | **PASS** |
| D2 | IAT slots with an FF 25 jmp thunk | 100/495 | 0/495 |  | **PASS** |
| E | known .rdata string addresses referenced | 7/7 | 0/7 |  | **PASS** |
| F | function starts begin with a real opcode | 82,215/83,046 = 99.00% | 10/14 = 71.43% | ctrl expect 64.5% (n=14, noise)) | **PASS** |
| G | .text byte equality vs the encrypted original | 64,529 equal / 16,560,128 = 0.3897% | - | independence predicts 64,688 equal bytes | **PASS** |

- control alignment rate 5.81% vs chance 6.25% (n=241) -> at chance
- control opcode rate 71.4% vs chance 64.5% (n=14) -> at chance
- the control's degenerate denominators are why A2/F-chance are stated separately.

## Detail: .text equality (metric G)

- 16,495,599 of 16,560,128 bytes differ (99.61%).
- equal bytes: 64,529; byte-wise independence predicts 64,688 (99.75% of prediction).
- Interpretation: the two .text blobs are statistically independent byte streams, i.e. the
  unpack rewrote the **whole** section rather than patching part of it. A partial or tampered
  dump would show a large excess of equal bytes.

## Detail: string cross-references (the decisive test)

| string | address | refs (unpacked) | refs (encrypted ctrl) | bytes at that address |
|---|---|---|---|---|
| `Failed to initialize palette layout page %x` | 0x013f8368 | 1 | 0 | `Failed to initialize palette layout page %x
` |
| `SP_Havok` | 0x013f8d24 | 2 | 0 | `SP_Havok` |
| `Stack trace is:` | 0x013f8cb0 | 1 | 0 | `Stack trace is:
` |
| `UI/MVJ/FileCache` | 0x013f8ea8 | 1 | 0 | `UI/MVJ/FileCache` |
| `devDirs` | 0x013f8dc8 | 5 | 0 | `devDirs` |
| `shipDirs` | 0x013f8db4 | 6 | 0 | `shipDirs` |
| `utfres` | 0x013f8d94 | 1 | 0 | `utfres` |

## Detail: absolute-operand histogram (where decrypted code points)

- unpacked: {'.data': 24, '.rdata': 142}
- encrypted control: {'.text': 2}
- the 7 out-of-image operands in the unpacked window:
  - 0x00402c8b: `mov edx,DWORD PTR ds:0x0`
  - 0x00402e5a: `mov edx,DWORD PTR ds:0x0`
  - 0x00402fa5: `mov edx,DWORD PTR ds:0x0`
  - 0x004055f0: `mov eax,ds:0x0`
  - 0x00405634: `mov eax,ds:0x0`
  - 0x0040b2f6: `mov eax,ds:0x0`
  - 0x00410213: `mov eax,ds:0x0`
- control out-of-image operands (first 5 of 304):
  - 0x0040137b: `mov edi,DWORD PTR ds:0x64221075`
  - 0x004013c5: `mov al,ds:0x190f51d6`
  - 0x00401427: `not DWORD PTR ds:0xaf117f8b`
  - 0x0040147f: `mov al,ds:0xecb094aa`
  - 0x00401531: `cmp ebp,DWORD PTR ds:0x8cfba357`

## Detail: IAT coverage

- 495/495 slots referenced by the decrypted .text; 100 of those via an `FF 25` jmp thunk.
- control (encrypted .text): 3/495 referenced.
  - 0x013cc710 -> `WINMM.dll!timeEndPeriod`
  - 0x013cc714 -> `WINMM.dll!timeBeginPeriod`
  - 0x013cc718 -> `WINMM.dll!timeGetTime`
  - 0x013cc730 -> `WSOCK32.dll!send`
  - 0x013cc734 -> `WSOCK32.dll!listen`
  - 0x013cc738 -> `WSOCK32.dll!connect`
  - 0x013cc73c -> `WSOCK32.dll!accept`
  - 0x013cc740 -> `WSOCK32.dll!getsockname`

## Verdict

**PASS on every metric, with clean separation from the encrypted control.**
`refs\bins\ReleaseGA\SporeApp.exe.unpacked.exe` is a genuine post-decryption dump of the
GA build and is the **level-1 disassembly oracle** for this project. This restores the field
guide's authority order #1 (disassembly of the original binary), which was unusable while
`.text` was encrypted.

## Method notes (wrong invariants, recorded so they are not re-derived)

1. **int3 run *start* alignment.** Revision 1 asserted each CC run starts 16-aligned and
   reported 52,843 failures. The invariant was backwards: MSVC aligns the *next function*, so
   the run **end** is the aligned edge. Function at 0x00401000 is 9 bytes
   (`55 8B EC A1 00 0C 5D 01 C3`); the CC run is 0x00401009..0x00401010; 0x00401010 is the next
   start and is aligned. The dump was fine.
2. **Raw E8/E9 byte scanning as a branch census.** Counts immediates and data bytes as
   branches (18% 'bad' on genuine code). Replaced by an exact linear-disassembly window (B).
3. **`FF 25` thunks as the only import form.** Revision 1 demanded a jmp thunk per IAT slot and
   got 100/495, then called that a failure. Most imports are reached by `FF 15 call [slot]`,
   `push [slot]` or `mov reg,[slot]`; the correct metric is 'slot address appears in .text' (D),
   with D2 kept as the thunk-only sub-count.
4. **No control.** The first two revisions judged numbers in isolation. Every metric here is
   now paired with the same measurement on the encrypted .text; a metric that does not separate
   the two is not evidence and is marked as such.
5. **Thresholds ignoring the chance baseline.** Revision 3 required the control's opcode rate to
   be <10%, but the allowed-opcode set covers 64.5% of all byte values, so random data scores
   ~64.5% and the test was unsatisfiable by construction. Likewise it required the control's
   alignment rate to be <2%, when random addresses are 1/16 = 6.25% aligned. Both are now
   compared against the computed baseline, and the baseline is printed in the table.
6. **str vs int comparisons.** The absolute-operand offenders were compared as text ('0') against
   an integer (0x0), producing a spurious FAIL on a metric that was actually clean.
7. `objdump` over the whole 16 MB .text is avoided: a 0x10000 linear window plus raw byte scans
   for the whole-section metrics (A/D/E/F) gives the same evidence far cheaper
   (field guide §2: never dump the whole exe).
