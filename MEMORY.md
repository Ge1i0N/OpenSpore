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

`-w` (windowed) **located** (session 12): it is the device-creation function at
`0x7e74a0..0x7e7637`, not the devDirs block. It looks up `f` (0x1413ab0) then `w` (0x1413ab4)
through the same helper `0x92b570` and sets a windowed byte (`f`→0, `w`→1, so `w` wins because it
is tested last); `r` (0x1413ac4) is parsed with `%dx%d` (0x1413ab8) into width/height. The sole
`GetCommandLineA` consumer is `0x00e20b5a`, which converts the ANSI command line to UTF-16 via
`0x93c510` (the arg helper compares UTF-16 names). `screenWindowed` 0x014111d0 is a **RenderWare
device metric** (`{name,index}` table at 0x1411440, index 14), NOT a CLI flag — that was the trap.
The census missed `w`/`f`/`r` only because it required names ≥2 chars; `tools/arg_vocab.py` now
accepts 1-char names at a real helper call site, so the vocabulary is complete (60 names).

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

**Index records — layout measured on 17,901 records (31 fixed-stride files), parser
disasm still pending.** Record = 6 u32 LE:
| word | meaning | evidence |
|---|---|---|
| [0] | **type id** | 16 distinct values over 31 files; one dominates (0x01A510BB, 98%). Constant per resource class |
| [1] | **instance id** | varies per record; the accessor at `0x008da6f0` copies record words [0],[1],[2] + a WORD at +0xC into its out-param, i.e. (type, instance, offset) is the lookup identity |
| [2] | **file offset** | `offset < index_offset` in 17,901/17,901 records (zero violations) |
| [3] | **size \| 0x80000000** | high bit set in 17,901/17,901 — a constant validity flag, not a compression toggle |
| [4] | uncompressed size (probable) | `[4] >= masked [3]` in 17,901/17,901; unproven, kept raw |
| [5] | sentinel | exactly two values: 0x10000 (88%), 0x1FFFF (12%) |
Invariant verified: `offset + size <= index_offset` for every record of every fixed file.
The 12-byte index prefix = 3 u32: `[0]` = format tag (seen: 6 for NameGen, 7 for UI),
`[1]` = an id/hash, `[2]` = 0 for NameGen / nonzero for UI.

**Var-width files (26/57, e.g. every `Text.package`)**: `idx_size = 16 + 20*count` fits
exactly (Text: 16+196*20=3936 ✓). The prefix is longer and the stride shorter — probably
records without the [4]/[5] pair plus a name table. DO NOT code against this until the
container-load path is disassembled: the index lives in the object at `[pkg+0x2d0]`
(container vtable methods +0x24 (buf,size,count) / +0x28 (get record by index) / +0x38 /
+0x40 (size)), fed from `0x008d9a90` (vtable+0x90). That cluster is the next disasm target.

**The header loader's failure branch** (`0x8d9053 je 0x8d905d`): when the validator returns
false, it calls `0x008dd820` with `[obj+0x20]` — a *fallback* parser that reads a 17-byte
header and matches the 4-byte magic `{0x7f,0x9d,0x88,0xec}` against the stream (plus a
compare against the dword at `0x01436b20`). All 57 shipped files pass the DBPF validator,
so this path is dead for our data; recorded so nobody mistakes it for the main parser.

## Traps hit in session 8 (RW4; recorded so the reader is not searched for wrongly)

1. **A format magic that is `memcpy`'d from a template is NOT findable as an inline immediate.**
   RW4's 28-byte header lives at `.rdata 0x014f6480` and is copied wholesale
   (`mov esi,0x14f6480 / rep movs`), so searching `.text` for the magic value or its bytes finds
   **nothing** (verified: 0 hits for `0x34575289`, for `"w32\0"`, and for the marker dword - and
   0 raw byte occurrences of `\x89RW4`/`w32\0` in `.text`). The reader therefore validates by
   other means, and must be found by CALL GRAPH (from the writer `0x006c233d` or the resource
   accessor `0x004021a0`), not by a magic search. Contrast RefPack, whose magic *is* an inline
   immediate (`0x46504244` / `0x10FB`) and which `tools/find_imm.py` located in one run.
2. **A distinctive sentinel can be writer-only.** `0xcafed00d` was a promising lead for the
   reader, but it occurs exactly twice and both are inside the writer `0x006c233d`. Do not assume
   a constant is shared between the read and write paths until both are seen.

## Traps hit in session 6 (RefPack; each one cost a round)

1. **A copy loop that tests the counter BEFORE decrementing runs N+1 times.** The RefPack copy
   loops are `mov eax,esi / dec esi / test eax,eax / jne` (e.g. `0x92c660`), so for a register
   value of N they emit **N+1** bytes, while the *literal* loops (`dec esi / test esi,esi /
   jne`, e.g. `0x92c752`) emit exactly N. The copy lengths are therefore `+3/+4/+5`, not
   `+2/+3/+4`. My first port used the register value and failed **250/250** real streams. The
   fix was found by requiring an exact length match, not by re-reading (I had re-read the
   disasm twice and it still "looked" correct). Stand up an exactness test early, for any
   decoder.
2. **A masked signature test means the top bits are flags, not part of the magic.** RefPack's
   detector is `bswap(word) & 0x1FFF == 0x10FB`, so `0x10FB` and `0x50FB` are both valid; a
   strict `data[0]==0x10` check rejected 50 of the first 20,000 real records. Whenever the
   original masks a constant, port the mask, not the constant.
3. **Judge a function's identity by its callers, not by a nearby `int3`.** I first disassembled
   from `0x92ccdc`, which is the *middle* of `0x92ccd0`; `tools/callers.py` gave the real entry
   (the package path calls `0x92ccd0`, which in turn calls `0x92c5b0`), and that is what made
   the header/loop split correct.
4. **Third-party docs were wrong again, and again in the direction that would have broken us.**
   SporeMaster3's `DBPF_FORMAT.md` says the RefPack size is little-endian; it is big-endian
   (`10 FB 00 01 38` -> `0x000138` = 312 = that record's `memSize`). That is the third distinct
   claim from that document set to be disproved by the disassembly. Its *code*
   (`RefPack.cs`) is accurate where the prose is not - read the source, not the summary.
5. **A PowerShell exit code read in a race with `WaitForExit` looks exactly like a crash.**
   Again this session: `$p.ExitCode` returned `0xFFFFFFFF` while the log said `exit code=0`.
   Always `[void]$p.WaitForExit(ms)` first, and believe the log.

## RefPack (EA LZRef) — SOLVED, verified on 30,000 real records

Decoded from the oracle's instructions; C++ in `source/engine/refpack.cpp`, Python in
`tools/refpack.py`, wired in as `Package::read_resource_decoded` + the `pkg:-decode` verb.
**Verification: 30,000/30,000 real `.package` records decode to EXACTLY the index's `memSize`,
0 failures** (`python tools/refpack.py --verify --limit 30000`). A wrong table cannot hit an
exact declared length 30,000 times.

Chain (tools/callers.py): `0x008d8b3b` (package resource path) → detector `0x0092cd40` →
header+`0x92ccd0` (codec vtable `0x0140a12c` slot +0x20) → command loop `0x0092c5b0`.
BE reader `0x0092c4e0` (the only 1/2/3/4-byte reader; `shl 8 / add` per byte = big-endian).

Header: 2 magic bytes, then a **3- or 4-byte BIG-endian** uncompressed size at +2, chosen by
bit15 of the magic word. (The upstream SporeMaster3 doc says **little-endian** and is wrong.)

Commands, control byte `c` (quoted from `0x0092c5b0`), `dist` is a byte distance back:
| c | form | literals | copy len | distance |
|---|---|---|---|---|
| `<0x80` | 2-byte (+`b1`) | `c&3` | `((c>>2)&7)+3` | `((c&0x60)<<3)+b1+1` |
| `<0xC0` | 3-byte (+`b0,b1`) | `b0>>6` | `(c&0x3F)+4` | `((b0&0x3F)<<8)+b1+1` |
| `<0xE0` | 4-byte (+`b0,b1,b2`) | `c&3` | `((c&0x0C)<<6)+b2+5` | `((c&0x10)<<12)+(b0<<8)+b1+1` |
| `<0xFC` | literal block | - | `((c&0x1F)<<2)+4` literals | - |
| `>=0xFC` | tail | `c&3` | - (stop) | - |
Copies are byte-at-a-time (matches may overlap → RLE semantics).

**The `+1` is the whole story and cost a full round.** The *copy* loops test the counter
BEFORE decrementing —
```
92c660: mov al,[edi] / mov [edx],al      ; copy one byte
92c664: mov eax,esi                      ; save the pre-decrement value
92c666: inc edx / inc edi / dec esi
92c669: test eax,eax / jne 0x92c660      ; -> N+1 iterations for esi=N
```
— so the byte count is the computed value **plus one** (`+3/+4/+5` above, not `+2/+3/+4`).
The *literal* loops test AFTER (`dec esi / test esi,esi / jne`, e.g. `0x92c752`) so they run
exactly N. A first port that used the register value directly failed **250/250** sampled
streams; with the +1 it is exact on 30,000/30,000.

**Two valid signatures.** The detector masks: `bswap(word) & 0x1FFF == 0x10FB`. Top bits are
flags, so `0x10FB` **and** `0x50FB` pass (0x50FB has bits 12 and 14 set). A strict
`data[0]==0x10` check rejected 50 of the first 20,000 real records — every one a `0x50FB`.

**First real game content ever decoded**: `pkg:-decode,0,0` and `,0,2` of
`Data\Locale\cs-cz\NameGeneration.package` → 312 B and 10,191 B, both exactly `memSize`,
UTF-8 (BOM `EF BB BF`) Czech given names (`Ábel, Adam, Bára, Běla, Emil …`) and toponyms
(`Abélard, Aberdeen, Adrásteia, Agamemnón …`).

## RenderWare 4 (= MODELS / meshes) — header CONFIRMED, object table not reversed

**`rw4` resources are models (meshes)** — user-confirmed, and consistent with the observed data
(12,987 of them, plus 11,041 `gmdl`, are the geometry the game draws). This raises their value:
parsing rw4 is what unlocks the actual 3D content.

The `rw4` resources (0x2F4E681B, 12,987 of them) are **RenderWare 4**, and the field guide's §3
playbook (RW 3.7 chunk ids, librw as a drop-in) does **not** apply as written.

**A 28-byte header template is copied verbatim by the constructor at `0x011e3f60`** (quoted):
```
11e3f72: mov  ecx,0x7
11e3f77: mov  esi,0x14f6480
11e3f7e: rep  movs DWORD PTR es:[edi],DWORD PTR ds:[esi]   ; 7 dwords = 28 bytes
```
The template at `.rdata 0x014f6480` (verified by dumping it):
| +0x00 | 0x34575289 | `"\x89RW4"` |
| +0x04 | 0x00323377 | `"w32\0"` |
| +0x08 | 0x0a1a0a0d | `"\r\n\x1a\n"` (EA end-of-header marker) |
| +0x0c | 0x00042000 | flags (meaning NOT reversed) |
| +0x10 | 0x00343534 | `"454\0"` |
| +0x14 | 0x00303030 | `"000\0"` |
| +0x18 | 0x00000000 | |

After copying the template the constructor fills, in order: `[obj+0x1c]`, `[obj+0x20]`,
`[obj+0x24]=0`, `[obj+0x30]` (a computed 4-byte-aligned stream size), `[obj+0x34]`, `[obj+0x38]=self`,
`[obj+0x44..0x63]` (8 dwords from a sub-call), `[obj+0x64..0x83]` (8 dwords), `[obj+0x84..0x93]`
(4 dwords from the caller's array), `[obj+0x94]`.

The caller that **writes** an rw4 stream (`0x006c233d` → `call 0x11e3f60` at `0x006c24b3`) pushes
the sentinel `0xcafed00d`. The rw4 type id is passed to a resource accessor at `0x004021b3`.

Tooling: `tools/rw4.py` (validates + dumps the 28-byte header; stops there on purpose),
`tools/rw_scan.py` (locate/decode/dump rw4 resources).

**Status: header confirmed, object table NOT reversed.** Do not fill in the object/chunk layout,
the `+0x0c` flags, or whether `"454"` is a version or build tag from any third-party RW4
description - find the reader in `.text` and read it like RefPack was.

**Session 13 progress (the model manager is located, but it REGISTERS, it does not parse).** The
rw4 handler `0x7633c0` dispatches through two manager singletons; the first is global `0x15fd8c4`
(getter `0x67dd20`, setter `0x67df40`). That object is created by the factory **`0x7ac4d0`** (alloc
0x148) -> ctor **`0x7ac2a0`**, class vtable **`.rdata 0x140fb48`**. The handler calls its `+0x6c`
(**`0x7ab7f0`**) and `+0x70` (**`0x7ab050`**) depending on a mode flag - but BOTH turned out to be
**container operations, not a format parser**: `+0x6c` rebuilds a map by object type (`0x7aa7d0` is
the map insert), and `+0x70` is a mutex-guarded wrapper that calls **`0x6b66f0`**, which is a hash-map
**erase**. So the model manager registers/unregisters models in a map; the actual rw4 stream PARSING
is NOT here. The other manager (`0x15fd8c8`, getter `0x67dd30`) is set from `0x7a88c0`/`0x76e680`.
**Next: the parser is elsewhere** - follow the stream object (`0x7633c0`'s arg) rather than the
manager, or find who reads the RW4 header template (`0x014f6480`) besides the ctor `0x011e3f60`.

**The RW4 registration surface (session 13).** rw4 (0x2f4e681b) is registered in TWO places:
`0x007635f0` (via the hash-map registrar `0x6ad090`) and `0x00764290` (via a different registrar
`0x6ae000`, a vector insert). The second site also registers two MORE handlers - `0x763ec0` for RW
id `0x20003` and `0x763d80` for RW id `0x2000b` - and registers the `raster` type (`0x2f4e681c`) with
the model manager (`0x67dc90` -> global `0x15fd894`) via `[mgr]->vtable+0x24` at `0x7642d4`. So the
RW4 plugin surface is: handler `0x7633c0` (rw4 container), `0x763ec0`/`0x763d80` (RW object types),
plus the raster registration. `0x763d80` caches objects via the model manager (keys from globals
`0x16304d4`/`0x16304c8`, 12 bytes each) into globals `0x16303f8`/`0x16303fc`; `0x763ec0` is an id-list
lookup. Neither is the mesh parser, but the surface is now mapped.

## GMDL (generated model) — LAYOUT SOLVED (session 12); positions verified exactly

`gmdl` (type 0x00E6BCE5, **11,041 resources**) is a baked mesh. Two early hypotheses were
disproven (fixed header triple 18/400; uniform vec3 stride) — see `IDEAS.md` §5. The layout below
was then established and is exact: the extracted positions reproduce the file's own bounding box
with **err 0.0000** on every variant tried.

```
...variable header of {tag, value} groups...
u32              count            (1 or 2)
float32 bbox[7]  minXYZ maxXYZ 1.0
u32              submesh_count    (1 or 2)
repeat submesh_count times:
    u32  format        (= 4)
    u32  index_count
    u32  reserved      (= 16)     <- NOT the vertex stride (that was the trap)
    u32  index_bytes   (== index_count * 2)
    u16  indices[index_count]
u32  1
u32  N                (vertex-declaration entry count: 5 or 6)
u32  0
u32  2
u32  0
u32  0x000C0000
(N-1) x 12 bytes      vertex declaration
u32  0
u32  vertex_count     (== max_index + 1)
u32  vertex_bytes     (== vertex_count * 36)
float32 vertices[vertex_count][9]   ; stride 36 bytes; POSITION = the FIRST float3
```

**Confirmed numbers** (four variants, all exact): `Spore_Content` rec[12] vc=3765 stride36
err=0; `EP1_Content_01` rec[0] vc=16551; `EP1_Content_02` rec[1] vc=28880, **two sub-meshes** whose
second indexes the same buffer (first index 24353); `EP1_Data` rec[579] vc=2357. The vertex block
header's `vertex_count` field sits at `voff + 0x1C + (N-1)*12` (0x58 when N=6, 0x4C when N=5), and
`vertex_bytes == vertex_count*36` follows it. The variable header + bbox moves the descriptor's
offset per file, so the reader FINDS the descriptor by self-consistency, not by offset.

What is NOT done: the meaning of the six non-position floats in each 36-byte vertex (the
declaration table says how many, not what they are). Session 13 pinned down **UV = two floats at
byte offset 16** for the N=6 declaration (100% of vertices in [0,1] on three independent variants,
`tools/gmdl_attribs.py`); offset 12 is a zero dword and bytes 24-35 are still unidentified (a float
plus packed data). N=5 (`EP1_Content_02` rec[1]) lays the attributes out differently, which is what
the declaration table is for. The rest needs the loader's disassembly.

**IMPORTANT (session 13): the vertex stride is NOT constant - it is `vertex_bytes / vertex_count`,
and it varies with the declaration count N.** Hardcoding 36 was a real bug. Measured on the exact
variants: **N=3 -> stride 24, N=4 -> stride 28, N=6 -> stride 36**, and **position = the first float3
at every stride**. With the stride computed, the exact-parse rate jumped: `Spore_Content` **491 ->
1,747** of 4,209 (x3.6), game-wide **1,776 -> 6,066** OBJ. The bbox validation uses a **relative**
tolerance (5% of the bbox diagonal - the header bbox is a rounded summary), which raised it further to
**Spore_Content 2,016** and game-wide **6,979 OBJ (63%)**. `decl_n` is 3/4/5/6 across the game
(Spore_Content {3:1179, 4:483, 5:3, 6:551}); the remaining unparsed ones have errors of 7-16 (>100% of
the diagonal) - a genuinely different layout, not a rounding issue (a block-start shift search always
found shift 0 best). UV is at offset 16 only when stride >= 24 (the N=6 layout).

Tools: `tools/gmdl_mesh.py` (parse + validate bbox + export OBJ), `tools/gmdl_attribs.py` (per-
channel statistics that identified the UV), `tools/render_obj.py` (OBJ -> PNG, no deps),
`tools/gmdl.py` (locate/inspect), `tools/gmdl_header_check.py` (the broad invariant test that
killed the first claim). Evidence: `analysis/meshes/gmdl_pkg{48,52,53,54}.obj`,
`analysis/shots/gmdl_pkg{48,52,54}.png`.

**The engine draws it** (session 13): `source/engine/gmdl.cpp` is the C++ port of the parser,
`source/render/mesh.cpp` uploads positions+indices with smooth normals and a hand-built orbit
camera, and the Loader draws one mesh into its own viewport. Evidence: `analysis/shots/mesh_engine.png`.

## RASTER (RenderWare texture) — header + DXT solved (session 13)

`raster` = type **0x2f4e681c** (immediately after `rw4` 0x2f4e681b in the registry), **5,931
resources**. Header derived from 1,200 decoded payloads (`tools/raster_scan.py`) and confirmed by a
correct decode (`tools/raster_decode.py`):

```
+0x00 u32 = 1            (version)
+0x04 u32 = dim          (h or w; every sampled texture is square)
+0x08 u32 = dim          (the other dimension)
+0x0c u32 = code         (10/9/6/7 - mip/quality code; meaning open)
+0x10 u32 = 8            (constant in every sample)
+0x14 char[4]            format tag: "DXT5" (704/1200), "DXT1" (122/1200), or u32 0x15 = raw BGRA
+0x18 u32 = base bytes   (== w*h for DXT5, w*h/2 for DXT1, w*h*4 for raw)
+0x1c ...                base-level pixels, then mip levels
```

Decoding the base level works: `Spore_Content` rec[125] is a 256x256 **DXT1 normal map**
(`analysis/shots/raster_dxt1.png` - the classic purple/blue sculpted relief), which is a clear,
correct decode. DXT5 (alpha atlases) and raw BGRA also decode. Note: this game stores **normal maps
uncompressed** (`raw`, tag byte 0x15), which is why a demo texture picker skips `raw` to find a
colour/diffuse DXT texture. NOT done: the exact meaning of the `code` field, the mip-level
count/offsets. Tools: `tools/raster_scan.py` (headers across many resources),
`tools/raster_decode.py` (DXT1/DXT5/raw -> PNG).

**In the engine (session 13):** `source/engine/raster.cpp` is the C++ port; the Loader decodes a
raster and draws it via `create_texture_bgra`, preferring it over the png path. Measured: `showing
real raster 0x40642901/0x067a07e7.raster (512x512 DXT5)` and `analysis/shots/raster_engine.png`
show the game's OWN texture format rendered by our renderer, next to the gmdl mesh.

## UI (SPUI) — data-driven menus (session 13)

The game's menus are NOT code: they are `spui` resources (type **0x250fe9a2**, **524 screens, 407
named**). Name list: `analysis/spui_screens.txt`. Includes `TitleScreen`, `LaunchScreen`,
`GameLaunchScreen`, `SporeTitleSeq`, `CSATitleScreen`, `GameSettings*`, `GlobalUIOptions-2`,
`MessageBox`, `NetworkLoginDialog`, plus in-game HUDs. Related types: `layout_bin` 0x050f4731,
`layout` 0xefbda3ff (only `spui` is present in our packages). A screen is requested by id via the
resource manager: `0x0080fff0` builds `{spui, layout_bin, layout}` and calls `[mgr]->vtable+0x24`.
The `UTFWin/cSPUI*Win` window classes (`.rdata 0x1416c40..`) are registered from the cluster
`0x8009f0..0x800b60` (alloc + ctor + register by name/id). Full notes: `analysis/ui.md`.

## README hero is the 2x2 model grid
The README's hero image is now `analysis/shots/model_grid.png` (four real models, each textured) -
the project's strongest showcase.

## Asset format status (all in `analysis/formats.md`)

A per-type reference: `png`, `raster`, `gmdl`, `locale` are **SOLVED**; `trigger`, `prop`,
`blockdata`, `rw4` are **PARTIAL**; `gmsh` (a mesh-like payload), audio, animation are TODO.
`blockdata` (0x5caa5e28, 3,141) is an array of `vec3` whose first two are a **min/max bounding box**.

## Config scripts = loose text in Config/ (session 13)

The game's options/config are **loose text files**, not packaged resources:
`Data/Config/{Properties.txt,ConfigManager.txt,VideoCards.txt}` (and `DataEP1/Config/...`). This
matches the resource groups the disassembly loads (`properties`, `ConfigManager` — see
`analysis/content-loading.md`).

- `Properties.txt` declares options as `property <Name> <id> [type]` — the game's own name->id
  table. `Data/Config/Properties.txt` has **63** (DataEP1 adds 76). The id comes in **three forms**:
  explicit hex (`0x04754439`), decimal (`41`), or **`(hash(Name))`** where the hashed name can DIFFER
  from the property name (e.g. `property AudioMasterVolume (hash(mastervolume))`). 14 of the 63 use
  `(hash(...))`. **Never just hash the property name**; the file is authoritative. Our C++ reader
  (`source/engine/config.cpp`) resolves all three forms.
- `ConfigManager.txt` is an imperative script with keywords `set/seti/setb/setOption/boolProp/
  intProp/setResolution/setVariables/if/elseif/else/endif/trace/include/property/alert` and
  `$`-variables (`$Medium`, `$resolutionTarget`). Its blocks are hardware-conditional; our reader
  records assignments but does NOT evaluate the conditionals (211 assignments, 198 conditional lines
  in the shipped file). Values that reference a `$variable` are resolved against the `set` table
  (e.g. `setOption OptionVideoRes $Medium` -> `2`; the LAST assignment wins).
- `VideoCards.txt` maps `vendor "AMD" 0x1002` and `card 0x9442 "Radeon HD 4850"` — the game's
  hardware database (**4 vendors, 616 cards**).
- Full table + language notes: `analysis/config-scripts.md`.
- **Our engine reads all three**: `source/engine/config.cpp` loads them through the fs search paths at
  boot (log `[config] config: 63 properties, 83 variables, 4 vendors, 616 cards`), and the `cfg`
  console verb exposes it (`cfg -list|-props|-vars|-cards|<name>`).

## Localized text (`locale`, type 0x02FAC0B6) — SOLVED, session 13

Full detail: `analysis/text-locale.md`. The durable facts:

* **Format**: UTF-8 **with BOM** (`ef bb bf`), records separated by **CRLF**, grammar
  `0x<8 hex> [~markup~] <text>` — an optional `~...~` token right after the key. 0 unterminated
  tokens in 18,439 key lines. `#` lines are comments (5,434); 17 of 31,209 lines are text
  continuations (a record's value may wrap).
* **Scale**: 2,944 locale resources over 15 languages. en-us: 200 resources → **197 parse,
  18,441 entries / 10,246 distinct keys / 684 ambiguous / 53 non-ASCII texts**.
* **THE RULE — the key space is PER TABLE.** Within one locale resource a key appears at most once
  (0 of 200 en-us tables repeat one). Across tables the low band collides:
  `<0x1000` → **36.2% ambiguous**, `0x1000..0xFFFF` → 1.9%, `>=0x10000` → 0.5–0.7%. So `0x00000011`
  means different things in different tables, while `0x05B08767` ("New Game") is effectively global.
  **`get()` refuses an ambiguous key instead of guessing** — the same discipline as `naming`. A
  wrong string in a menu is worse than no string.
  Consequence for the menu path: **UI labels live in the high (unique) band; guide/tutorial text
  lives in the low per-table band** — which is also how a `spui` layout can reference a key without
  naming its table.
* UI label keys arrive in **contiguous runs** (`0x045BEDD1 Save`, `…D4 Cancel`, `…D8 Exit`,
  `…D9 Quit`): an allocated group, not a hash spread. Hint for `spui` later — a widget likely
  stores base key + small index.
* Real labels recovered: `New Game 0x05B08767`, `Load 0x0045B9EB`, `Options 0x00010009`,
  `Settings 0x00001611`, `Credits 0x0473EF30`, `Exit 0x045BEDD8`, `Quit 0x045BEDD9`,
  `Save 0x045BEDD1`, `Cancel 0x045BEDD4`, `Back 0x05B08747`, `Ok 0x06162C25`, `Play 0x05B08733`,
  `Creator 0x058B5C13`, `Spaceship 0x0630D565`, `Upload 0x0549366D`, `Help 0x045FFE89`.
  **`Load Game` does NOT exist as a string** (the game says `Load`) — never invent a label.
* One record (instance `0x04DC2F94`, 1,644 keys) opens with the game's own prose: `# WARNING: This
  is an assembled content string table. It is auto-generated.` So some tables are tool output.
* `source/engine/text.cpp` + the `txt` console verb (`txt -list | -label <text> | -ambig |
  <0xKEY> | <tableINST> <0xKEY>`). `to_display_ascii()` folds UTF-8 to our ASCII-only GDI font
  atlas, one `?` per unknown code point (never a per-byte burst) — a display shim, not a
  transliteration.

## TRAP: the locale search path (corrected BY DISASSEMBLY, session 13)

`fs.cpp` used to add **`exe_dir/Locale/<lang>`**, which matches NOTHING on disk. The shipped layout
is `Data/Locale/<lang>/`, so the text reader was silently starved and logged "no Locale package"
while 200 tables sat right there. The original settles it: at `0x7e863c..0x7e8661` it loads the wide
key `'Locale/'` (`0x01413cc4`), walks it to its NUL (`0x7e8641..0x7e864e`), and **appends** it to a
base-path object (`call 0x429760`), then appends the language — so `Locale/` hangs off a **DATA
ROOT**, not off the executable directory. The same cluster treats `'BaseData/'` (`0x01413cb0`) and
`'UserData/'` (`0x01413c8c`) identically, which is the tie back to VERIFY row 2. `fs::init` now
attaches `Locale/<lang>` to every data root (`Data`, `BaseData`), matched case-insensitively (the
tree ships lowercase `en-us`, `-locale` may be spelled any way).

**Lesson worth more than the fix: a search path that yields "no data" is itself a bug to
disassemble, not a fact to accept.** Two of our "the data does not exist" conclusions (locale text,
and the state names still outstanding) are of this class.

## The `spui` door: the format's DICTIONARY is shipped in the game, and it is BYTE-packed

Full measurement: **`analysis/spui-schema.md`**. The durable facts, INCLUDING A RETRACTION of what this
file said earlier in the same session.

* **The door is named by the DATA, not the bytes.** The recovered state script contains
  `cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"`. Grepping `.rdata` for the **verb**
  produced the class cluster in one step, after grepping for the *format* had produced nothing for
  sessions. **Search for the command, not the data, when the data is opaque.**
* **Factory `0x00810870`** (gated on request tag `0x0510A95B`, alloc 0x24 tag
  `"App/SPUILayoutResource"`) -> **loader `0x00810940`**, which does `cmp dword [eax+4], 0x250FE9A2` —
  **the `spui` type id confirmed in CODE**, not inferred from `reg_type.txt` + the index. It builds
  `UI/UILayoutBinary/SharedPointer` (0x18) + `.../MemoryStream` (0x24) +
  `UI/cSPUILayoutResource/TokenIOContext` (0x38); the reader ctor is `0x00903330`, the family is
  UTFKernel's **`XML/TokenReader`** (`0x014183c4`, `0x0143a450`, used at `0x00903100`).
* **The schema was shipped all along.** A type histogram over all 150,945 resources found 14 resources
  of type **`xml` (0xCF6C21B8)** nobody had ever dumped: the Spore UI Designer's own project files,
  including a 70,032-byte format definition and a 37,425-byte class catalogue. Harvesting their `proxy`
  attributes yields **306 ids (66 Class + 240 Property)**; 227 appear in the `spui` binaries, and
  **748/748 layouts contain them**. There is no stride to guess because the names were in the data.
* **A record is `id(4) | code:u16 | marker:u16(==1) | payload`.** The u16 is a **TYPE CODE, stable per
  id** (94.5% of the high ids have exactly one value across the corpus), and the marker is 0x0001 in
  98.4% of 1,098,129 anchors. Measured payload sizes: code 9/11 -> 4, **16 -> 16 (`rectf`)**, 17 -> 8
  (`pointf`), 20 -> 6, 19 -> a u16 array count, 18 -> string.
* **RETRACTION: the stream is BYTE-packed, NOT bit-packed.** This file previously said "1,441,987 of
  1,911,484 id hits are off-boundary, so the stream is not word-aligned, exactly as a token stream
  predicts". The aggregate mod-4 histogram (24.6/24.7/25.6/25.1%) is nearly uniform, and so is the
  **zero-free** subset (215 ids, 494,552 hits: 24.2/22.7/28.8/24.4%) — which padding cannot explain, but
  which **record sizes that are not multiples of 4** explain exactly (code 20 -> a 14-byte record
  drifts the cursor by 2, and it accumulates). **A distribution that matches your hypothesis is not
  evidence for it when other hypotheses predict the same distribution; the discriminating test was
  per-id, and it was cheap.** No bit cursor is needed.
* **`Area` is RESOLVED, and it is LTRB, not x/y/w/h.** An earlier note here called its pitch 6 and left
  it as the most important open question; that was an artifact of pooling block-scoped LOW ids with HIGH
  ones. `Area` carries code 16 and a 24-byte record = `rectf` of four floats. The four floats are
  **LEFT, TOP, RIGHT, BOTTOM** (a corner pair, like a Win32 RECT): `(168, -10, 192, 14)` is a 24x24 icon
  and `(0, 3, 181, 5)` is a 181x2 bar, neither of which an x/y/w/h reading can produce. **The
  confirmation is quantitative, not aesthetic** - switching to LTRB raised the usable-area count from
  7,838 to **9,547** of 11,465 windows, because x/y/w/h rejects any rect whose "h" is a bottom
  coordinate below "y" or absurdly small. Negative left/top are legitimate (off-screen anchors for
  slide-in animation).
* **The alphas in a layout are MEANINGFUL - do not force them opaque.** Our first render ORed
  `0xFF000000` into `BackgroundColor` and the screen became opaque grey slabs hiding the art: the
  shipped values are `0x007F7F7F` (fully transparent grey) and `0xFFFFFFFF` (opaque white). Use the
  ARGB as stored, and outline the box so a transparent window is still visible as the designer's rect.
* **WE DRAW A REAL SCREEN NOW.** `source/engine/spui.{h,cpp}` + the generated
  `source/engine/spui_schema.inc` (`tools/gen_spui_schema.py`, same pattern as `typeids.inc`), driven by
  `title_render()`: 400 layouts, 11,465 windows, 9,547 with a usable Area, 472,291 records read, real
  captions from the layout itself - "Create a New Game", "color picker", "Selected Color" - over the
  real UI atlas. `analysis/shots/spui_real.png`, VERIFY row 48.
  The reader is **anchor-driven, not sequential**, on purpose: a sequential walk derails in 637/748
  files on ids the designer XML omits, but every anchor whose id IS known can be read exactly without
  interpreting the bytes between two anchors. Unfinished vocabulary must not block finished structure.
* **THE INTEGRATION THAT CLOSES THE MENU PATH: a layout names its LOCALE TABLE.** Code 18 is
  overloaded, split by a leading u16 of `0xFFFF`: 4,207 inline UTF-16 strings (`len=8 -> "To..."`) and
  **3,107 references, of which 21 of 36 distinct values are real en-us `locale` TABLE INSTANCES**
  (accidental rate 200/2^32 = 4.66e-08). The tables they name are the game's menu text:
  `0xC0152A6D` "Spore Guide" x846, **`0x2B7A0311` = `fnv1_lower("GalaxyGameEntry")` x394**,
  `0xAD7B2086` "Audio Settings" x309, `0x7C6511A1` "Sort By:" x140, `0x2C978DA4` "Create a Creature",
  `0x40D7E685` "OK", `0x8B96F855` "Missions List". So a layout carries **the table it draws text from**,
  not a per-string key. `0x2B7A0311` appearing in BOTH directions (as a state-name hash and as a layout
  reference) is the confirmation, and it links the state machine, the localized text and the UI layout —
  three systems reversed independently in one session.
* **Still open, and it is not the bit format:** a sequential walk reaches EOF cleanly in **0/748**
  layouts and derails in 637 with "id not in schema", because the designer XML lists only 227 of the ids
  the binaries use. **Extending the schema is the remaining job**, and it is vocabulary, not structure.
  Two further items are open and are NOT blocking: `Reader::load(limit)` currently caps at 400 of the
  748 layouts, and `Window::caption_table` (the locale table a layout names) is not yet resolved through
  `engine/text.cpp`, so only the INLINE captions reach the screen today.
* **TRAP: a `.rdata` pointer is not always a vtable.** `0x903337` stores `0x0143a460` into `[esi]`
  exactly where a ctor puts a vtable — it is the ASCII `"UTF/XmlTokenReader/TokenIOContext/mAllocator"`.
  As a vtable it would have invented a fake 12-method class; as a string it named the reader family.
  Same shape made `0x013ebcbc` / `0x01418398` in the factory look like vtables. **Try ASCII before
  treating a stored pointer as a vtable.**

## Ghidra is now part of the toolchain (session 13)

The oracle is imported into the Ghidra project `refs/ghidra/proj/OpenSporeOracle` (gitignored — it
is 543 MB of tool, not content). The bridge is OURS, not an upstream wrapper:

* `tools/ghidra_audit.py request` → `analysis/audit/request.json` (walks `analysis/claims.json`)
* `tools/ghidra_export.py` (PyGhidra, CPython 3.11 in `refs/ghidra/venv`) → `analysis/audit/facts.json`
  plus `analysis/audit/pseudo/0x*.c`
* `tools/ghidra_audit.py verify` → `analysis/audit.md` + `analysis/audit/audit.json`

Verdicts are **PASS / FAIL / UNKNOWN / SKIP**. The two rules that matter: **UNKNOWN is not PASS**
(Ghidra could not answer → the claim is still only as good as our own objdump reading) and **SKIP is
not PASS** (data-derived claims, e.g. the gmdl stride measured on 11,041 resources, cannot be settled
by a decompiler at all). Ghidra's pseudocode is a lossy reconstruction — on MSVC x86 `/GR-` with SEH
it can produce plausible-but-wrong C — so a FAIL means "re-read the disassembly", never "the doc is
wrong" by itself. Checking ORDERING claims (FNV-1 multiplies before it xors) is where a decompiler
is genuinely stronger than a flat listing, hence the `pseudo_regex` / `pseudo_not_regex` check kinds.

Two setup facts that cost a round each: Ghidra 12 **removed embedded Jython**, so a `-postScript
*.py` fails with "Ghidra was not started with PyGhidra" — run the export through PyGhidra instead;
and **`-analysisTimeoutPerFile 0` means "time out immediately", not "no limit"** — the first run
analyzed nothing and still exited 0, which looked like success.

## The Ghidra/REA audit (session 13) — did we start reversing correctly? YES

Full reading: **`analysis/audit-review.md`**. Machine output: `analysis/audit.md` (regenerated by
`tools/ghidra_audit.py verify`). The ledger that made it checkable: **`analysis/claims.json`**, 30
claims / 104 checks, every address assertion we have ever written down, each citing its doc line,
severity, and whether our C++ is load-bearing on it.

Verdict: **PASS 17, FAIL 1, UNKNOWN 11, SKIP 1.** The first run said FAIL 10; **nine of those ten were
bugs in my own checker, not in our reversal.** The whole load-bearing startup chain — entry →
CRT → `__tmainCRTStartup` → WinMain `0xf489e0` → app_main `0xf48810` → run `0xf47e90` → AppMode
`0x7e8a00` → main loop `0x8482f0` — PASSED, including both of our self-written TRAP notes (the
W-vs-A `PeekMessage` split, and `screenWindowed` being a RenderWare metric).

**The one real doc error found:** `startup.md` cited `0xf48405` as the call site into the main loop.
It is `0xf4840b`; `0xf48405` only LOADS the vtable slot:

```
f48405  mov eax,[edx+0x84]    ; LOAD slot          <-- what we cited
f4840b  call eax              ; THE CALL
```

That split is the shape of **every** virtual dispatch in this binary, and it is precisely why the main
loop was hard to find: a search for `call [reg+0x84]` matches nothing.

**The structural finding, and the most useful result of the audit.** Seven cited addresses had **no
function at all** in Ghidra's 54,611-function inventory: `0x7e3fb0` (statemgr add-state),
`0x7e7160` (event dispatcher), `0x7eaae0` (AppMode +0x20), `0x849bf0` (Window::Create),
`0x7c7f50` / `0x7c90f0` (config executors), `0x7e40f2` (mid-function). `tools/gen_seeds.py` walked each
back to the preceding `int3` gap and proved six ARE real entries — **Ghidra never reached them because
they are callable only through a vtable**, and six of the seven start with the MSVC SEH prologue
`mov eax,fs:0x0`, which Ghidra left typed as data. At `0x7e7160` it produced the datum `?? 64h` while
the objdump listing shows all three of our documented hashed event ids sitting in plain sight
(`cmp eax,0x255abf5` / `0xf62add` / `0x212d3e7`).

**This reframes our blockers.** rw4/spui/prop were recorded as "the handler slots are map ops, follow
the stream object" — the audit *confirmed* that (the `direct_callers == 0` check PASSED outright). But
the deeper reason we kept failing is now named: **the interesting functions here are vtable-only, so
neither auto-analysis nor a text search for `call` will find them.** The fix is seeding, in both
toolchains: `analysis/audit/seeds.tsv` (21 entries) → our `tools/ghidra_seed.py`, and the same file is
what `REA_GHIDRA_SEED_FILE` wants.

Corrections applied to the docs as a result (beyond the call site above):

* Window vtable `0x141cc10` **slot 0 is the deleting destructor `0x849e80`**, not the ctor. The ctor
  `0x849b20` is real but is **not a vtable slot** — a category error on my part, not a wrong address.
* `.data 0x153f858..` is a table of **pointers into `.rdata`**, not of strings.
* The FNV seed `0x811c9dc5` is a **parameter pushed by the caller** at `0x7e40f2`; the hasher
  `0x932e50` reads it from `[esp+0x14]` and contains only the prime `0x01000193`.
* `EventLoadScreenBufferPercentage` is at `0x0148227c` (only `.text` ref: `0x01373538`). The previously
  cited `0x01550498` is a pointer to **`EAReportUploader`**. Kept as an explicit negative control.
* `UIWorld_Loading` is **UTF-16LE** at `0x01418274`; there is no ASCII copy anywhere.
* `fnv1_lower("locale")` is `0xe31a5c1b`, **not** the type id `0x02fac0b6`. This re-confirms the
  existing rule (type ids are assigned, never hashed) — I had violated it in a check I wrote that same
  session, and the audit caught it.

**The transferable lesson: a FAIL from a tool is a hypothesis about the tool as much as about the
code.** Read the evidence; never act on the verdict alone. Three of my nine checker bugs were
invisible-API artifacts (`Memory.getInt(addr, True)` is **big-endian**, not "unaligned OK";
`JArray("b")` raises `ClassNotFoundException` — it needs the `JByte` **class**; regexes written against
objdump spelling cannot match Ghidra's `MOV EAX,dword ptr [EDX + 0x84]`), and each produced a
confident-looking wrong answer.

### The trajectory after seeding, and the wall MEASURED

| stage | PASS | FAIL | UNKNOWN | SKIP |
|---|---|---|---|---|
| first run (unseeded project) | 13 | 10 | 6 | 1 |
| after fixing my checker's byte order / strings / regexes | 17 | 1 | 11 | 1 |
| after seeding the 7 vtable-only functions + re-analysis | **29** | **0** | 3 | 1 |

Seeding the seven functions cascaded to **+683 carved functions** (54,611 → 54,618 → 55,294). That
number IS the vtable-only wall, measured rather than asserted: seven entry points let the analyzers
reach 683 more functions they could not see before. **Not one boot-chain reading was wrong.**

### TWO more checker artifacts (the class of mistake keeps paying)

1. **Ghidra prints conditionals by SEMANTIC name, objdump by the historical ALIAS.** Our docs say `je`;
   Ghidra says `JZ`. Same instruction, and a `/je/` regex FAILed against it. The auditor now matches
   both spellings (`normalize_insns` in `tools/ghidra_audit.py`).
2. **A function body is not always contiguous, so an ABSENCE is only evidence when the tool actually
   examined that ground.** The event dispatcher's body came back as
   `[[0x7e7160,0x7e71c4] [0x7e72bf,0x7e7300]]` — a **250-byte hole** in the middle Ghidra never
   disassembled, because it stops at the MSVC alignment padding `8d 49 00` and types those bytes as
   data. Our documented retry values (`mov [esp+0x30],0xa` at `0x7e7233`, `mov [esp+0x14],0xfa` at
   `0x7e7264`) live INSIDE that hole, so "immediate nowhere in the function" described Ghidra's
   coverage, not the binary. The export now carries `body_ranges`/`body_contiguous`, and the auditor
   downgrades such a miss to UNKNOWN **naming the gap** instead of claiming a refutation.

`tools/gen_seeds.py` derives `code` seeds for every detected gap (7 found: the event dispatcher, the
RefPack decoder, the mouse pump, `__tmainCRTStartup`, a model-manager handler). Three walls in the
filler were mine, and all three are the same shape — **a plausible API that does not exist**:

* `clearCodeUnits` is on **neither** `FlatProgramAPI` **nor** `ProgramDB`; it is the app command
  `ghidra.app.cmd.disassemble.ClearCodeUnitsCmd`.
* Stopping the walk at any defined datum can never fill anything, because **every hole BEGINS at exactly
  such a mis-typed datum** — the datum is the cause, not a boundary.
* `getFunctionContaining()` reports the enclosing function for an address inside the hole, so it is
  useless as a stop test. The gap is bounded by the body ranges, so the next DECODED INSTRUCTION is
  the real edge.

Also: **a gap may not be a gap.** `0x7e71c5` (the dispatcher's 250-byte hole start) holds a DECODED
instruction that lies outside `0x7e7160`'s body ranges — Ghidra carved the middle region as a separate
function or left it unaffiliated. So those bytes are analyzed; they just do not belong to the function
my claims cite, which is why `uses_imm` on `0x7e7160` cannot see them. **The fix was not seeding: it is
that an audit must cite the function that actually owns the bytes.** Re-deriving the gap set from
CURRENT facts (never replaying a saved `seeds.tsv`) is what exposed this.

And one doc correction it forced: the retry wait helper `0x922080` is **`SleepEx(ms, bAlertable=1)`**
via IAT `0x13cc17c`, not `Sleep` (resolved through the PE import directory), and the branch
re-dispatches event id `0xf62ade`. `startup.md`'s "Sleep(0xfa)" was wrong in detail and right in
substance (10 retries × 250 ms) — the audit is what forced the precision.

## Ghidra API surfaces that do NOT exist in 12.1.4 (each one cost a round)

`ProgramDB.getEntrypoint()` · `Listing.getListing()` / `FunctionManager.getFunctionManager()` /
`ReferenceManager.getReferenceManager()` as statics (they are instance methods on `Program`;
`getSymbolTable().getReferenceManager()` does not exist either) · `GhidraFile.getPath()` ·
`GhidraFile.save(String, TaskMonitor)` (it is `save(TaskMonitor)`) · `DecompInterface.setAutoUpdate()` ·
`DataDB.isString()` (use `getValue()`, and fall through to a raw read when the value is not a string —
`0x013ebc48` is typed as the integer `0x41` while its bytes are literally `"App"`) ·
`GhidraProgramUtilities.analyzeProgram()` · `FlatProgramAPI.createFunction(Address, String, SourceType)`
(two args only) · `clearCodeUnits` on `FlatProgramAPI` or `ProgramDB` (it is `ClearCodeUnitsCmd`) ·
`MemoryMapDB.getRange()` · `Address.getBytes(int)` · `Memory.getInt(Address, boolean)` where the boolean
is **bigEndIndexOrder, not alignment** · `JArray("b")` (needs the `JByte` CLASS) ·
`analyzeAll` requires an enclosing transaction or every analyzer worker dies with
`db.NoTransactionException` — and even then the worker threads cannot see a script's transaction, so
**drive analysis with `analyzeHeadless -process`, not from a script.**

## NESTED packages: the EP1 locale text is a DBPF *inside* a DBPF (session 13)

Measured in `tools/nested_locale.py` / `tools/nested_langs.py`, implemented in
`pkg::Package::open_from_memory` + `text::Tables::load`:

* `DataEP1/Spore_EP1_Locale_01.package` (3.8 MB) holds **17 resources of type `package`
  (0x06EFC6AA)**, and `_02` holds 4. Each of those resources **is itself a whole DBPF**. Together they
  carry **1,364 `locale` resources** - more than the 2944 base-game ones per-language.
* **A nested package is named by the same rule as a locale table**: the en-us one has instance
  `0x7080DB55 == fnv1_lower("en-us")`. Verified both directions
  (`fnv1_lower("EP1_Buttons") == 0x8B1A2341`, the table the layouts reference). So choosing the right
  nested container for a language is a lookup, not a guess.
* **This was the THIRD shape of the session's recurring error.** Not a wrong directory
  (`exe_dir/Locale` vs `Data/Locale`), not a wrong id field (group vs instance), but a wrong
  **CONTAINER DEPTH**. The data was in the tree; the tree was deeper than we looked.
* Effect on our engine, measured before/after. **The "+ nested" column was restated on 2026-10-14
  after `get(table,key)` was fixed (see below): the old numbers double-counted containers and the
  resolution rate included seven answers taken from the WRONG table.**

  | | flat only | + nested (restated, honest) |
  |---|---|---|
  | locale tables | 197 | **265 distinct, from 5 containers** |
  | distinct (table,key) | ~18,441 entries | **23,933 texts from 32,038 entries** |
  | distinct keys | 10,246 | **14,657** |
  | layout caption references that resolve | 1,139 / 1,426 (79.8%) | **1,384 / 1,426 (97.1%)** |

* The tables that were missing are exactly the ones the shipped UI layouts ask for: `EP1_Buttons`
  ('Replay', 'Done', 'Accept Reward', 'Interact'), `EP1_Headers` ('Scores', 'Mission Complete!',
  'Mission Summary', 'Respawn'), `EP1_Tooltips` (183 keys), `GGETextEP1` ('Play Adventure', 'Create
  Adventure', 'Captain Outfitter'), `ScenarioBuildMode`, `legal`. Verified live in the engine:
  `txt:0x8B1A2341:0x077427D2` -> **"Accept Reward"**, `txt:0xAD7B2086:0x0589E51B` -> **"640x480"**,
  `txt:0xC4F86AE0:0x07736311` -> **"Mission Complete!"**.
  (`mvj` `0x277DEE6A` was listed here too and is **wrong** - see the coverage/semantic split below.)
* **`Package` now has two backings** (FILE* or an in-memory buffer) behind one `read_at()`, and BOTH
  open paths run the SAME validator clauses. The refactor's first version left `read_resource()` on
  `fseek(m_file,...)`, which failed LOUD (`stream.valid()` assertion, m_file == nullptr) instead of
  silently returning garbage - a good outcome, and the reason the nested path is worth having one
  accessor rather than two read paths.
* The `locale` parse is now ONE method (`Tables::parse_package`) shared by the flat and nested passes.
  Deliberate: a second copy of the grammar is exactly how two views of one dataset start disagreeing.
* Also fixed here, because it was the same bug class waiting to bite again: `appstates` and `spui` both
  re-entered the **double-visit trap** (the dev search list has both `Data=<exe>\Data` and `cwd=<exe>`,
  so a recursive scan visits every package twice - `spui` reported 1,349 layouts where the corpus has
  748, and `appstates` listed 5 copies of a script that exists 3 times). Both now dedupe by
  `weakly_canonical`, as the `pkg` verb already did.

## Locale, session 13 later: an implicit 64->32 narrowing, and PatchData's own nested packages

Two findings, and the second is the **fourth shape** of this session's recurring error.

* **`get(table,key)` was silently ignoring the table.** `m_by_table` was declared
  `std::map<u32,std::string>` while every access packed `((u64)table << 32) | key`. The narrowing was
  **implicit**, so it compiled with 0 warnings and behaved like `get(key)`. In a corpus whose key
  space is PER TABLE - the very finding that made `get()` refuse ambiguous keys - that is exactly the
  failure the design exists to prevent, and it was putting another table's string into our menu.
  * **The symptom was in plain sight in a log line I wrote the same day**: "14,657 distinct
    (table,key) texts" EQUAL to "14,657 distinct keys". Two counts can only agree like that if one
    dimension is not being used at all. I nearly recorded it as a success.
  * Fix: declare the map `u64`, spell the key in ONE function (`pack()`), and put the trap in the
    header. Effect: caption resolution 1,391 -> **1,384** of 1,426. The number went DOWN because it
    is now correct; seven refs had been answering from the wrong table.
  * **Class of bug worth its own check: any 64-bit composite key built by shift-and-or and stored in
    a 32-bit container.** It never warns. Grep for `<< 32` and ask what the destination type is.
* **`Data/PatchData.package` carries its OWN nested locale packages** - one per language, **42 en-us
  tables** against EP1's 65, overlapping on exactly one table instance. The engine had been parsing
  them all along (its nested pass scans by TYPE across every container). My MEASUREMENT TOOLS did not,
  because they filtered on the filename `*Spore_EP1_Locale*`.
  * So "the `legal` and `mvj` tables are absent from our 57 packages" - asserted with confidence, in
    the hand-off, after a search that supposedly ruled it out - was **half wrong**: `legal`
    `0xADB810C4` ships in PatchData's nested packages and always did. Fourth shape: **wrong SCOPING OF
    THE SEARCH** (the first three were wrong directory, wrong id field, wrong container depth).
  * **Rule: enumerate by TYPE, never by filename.** `tools/nested_every.py` exists to make that
    unavoidable; it walks all 57 containers and reports every resource of type `package`.
* **Only `mvj` `0x277DEE6A` is genuinely absent** - 14 keys, no table instance anywhere, all depths.
  That is a COVERAGE gap (data we do not have), not a FORMAT gap. `spui -gaps` now splits the two,
  because conflating them is what kept a solved problem alive for two sessions:
  `COVERAGE 2 tables / 15 keys; SEMANTIC 3 tables / 4 keys` (semantic = `GalaxyGameEntry`,
  `settings`, `RegistrationAndLogin`, each missing one or two keys their layouts ask for).
* **A hash is invertible when you have the name list.** `fnv1_lower` over `reg_file.txt` (125,272
  names) resolves an unknown table instance back to its name directly: `0x277DEE6A`->`mvj`,
  `0xADB810C4`->`legal`, `0x2B7A0311`->`GalaxyGameEntry`, `0xAD7B2086`->`settings`,
  `0xB7BCEF68`->`RegistrationAndLogin`. Three lines of Python, and it turns "unknown table" into a
  searchable name. Do this BEFORE concluding data is missing.
* **Precedence between containers is now measured, not assumed.** The same `(table,key)` can arrive
  twice; which wins was an accident of traversal order. Of 32,038 entries -> 23,933 distinct pairs,
  **8,102 redefinitions AGREE and exactly 3 DISAGREE**. Two are PatchData-vs-EP1 on table
  `0xADE7FFAA`, and EP1 wins - **correctly**, because EP1 fixes the patch's typo ("an error has
  occured" -> "occurred"), so the later container is also the newer text. With only 3 affected strings
  out of 23,933, the original's exact rule is not worth reversing yet.
* **A key can repeat INSIDE one resource.** `tools/locale_dupkeys.py`: 10 of 2,944 locale resources
  state a key twice (23 occurrences), only 2 disagree, both cosmetic (`SPORE™` vs `SPORE`,
  `Spore-nytt` vs `Spore-Nytt`). Our parser stores one `Entry` per key LINE, so it sees these; **a
  reader that keys a table by `{key: text}` cannot see them at all** and silently picks the first.
  Ours lets the last one win.
* **`en-us` is shipped byte-identically in both EP1 containers** (65 tables, 514,211 B, identical
  SHA-1 per table). A byte-identical nested container is now skipped WITH A COUNT instead of doubling
  the corpus. The skip is conditional on a real digest match, not on the instance being seen before:
  a same-instance/different-bytes pair is still parsed twice and logged, because losing text is worse
  than an extra pass.

## Git: commit here, the user pushes (session 13)


`git push` from a session **cannot authenticate** in this environment: it fails with
`fatal: User cancelled dialog.` then `could not read Username for 'https://github.com': No such file
or directory` (no `/dev/tty`). **The user pushes manually via GitHub Desktop.** So: commit liberally,
never spend a round on `git push`, and do not treat "local only" as a problem to solve or report as a
risk. `origin/main` lagging `HEAD` by a commit or two is the normal steady state.

## TRAP: scoop's `current` is a JUNCTION, and REA refuses it (session 13)

`C:\Users\DartPower\scoop\apps\ghidra\current` is `<JUNCTION>` → `ReparsePoint` in its attributes. REA's
Windows native admission **opens every path component without following a reparse point** and rejects
the whole request (`windows-ghidra-p0.md`: "Mounted-folder entries that expose reparse tags fail
component admission"). So `GHIDRA_INSTALL_DIR` must be the **real versioned directory**:

```
C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921
```

The trap is that **`rea doctor --provider ghidra` reports `healthy: true` with the junction path** —
doctor validates the *installation*, while path admission happens later, at operation time. Three
imports failed before this was found, and the failure text is easy to misread as a target problem:

```
failure_cause: "Reparse path component is unsupported (tag 2684354563)
                [C:\Users\DartPower\scoop\apps\ghidra\current] (Win32 50)"
failure_message: "Ghidra target snapshot failed admission"
```

Two more Windows-only requirements learned in the same run: the target must also live on a **fixed
local NTFS** volume with no reparse component, and `REA_GHIDRA_STARTUP_TIMEOUT_MS` must be raised for
this binary (our own headless import of the 25 MB `.text` took **1,197 s** against a 330,000 ms
default; the MCP client's per-call timeout is a SEPARATE deadline that can fire first).

Full toolchain note: **`analysis/toolchain-rea.md`**.

## Toolchain: REA is PRIMARY, our PyGhidra bridge is the persistent-project fallback

* **REA 6.4.0** — installed globally via npm, registered as an MCP server in
  `C:\Users\DartPower\.config\opencode\opencode.json` (`rea doctor --provider ghidra --json` →
  `healthy: true`: Ghidra 12.1.4, native `decompile.exe` for win_x86_64, JDK 21 64-bit, Windows native
  authority verified). 170 tools, reachable as `tools.rea.*`. Use it for interactive investigation:
  `open_binary` → `analyze_function` / `procedure_pseudo_code` / `xrefs` / `find_xrefs_to_name` /
  `trace_call_path` / `get_call_graph` / `list_unknowns`. Its evidence contract reports
  `facet_availability` per facet and states that **"an empty call list with partial or unknown coverage
  is not proof that the function has no calls"** — exactly the discipline our
  `direct_callers == 0` claim needed.
* **Our bridge** (`tools/ghidra_export.py` + `ghidra_audit.py` + `ghidra_seed.py`, venv at
  `refs/ghidra/venv`) exists because **REA uses an ephemeral `-readOnly -deleteProject` session per run
  and cannot accumulate** seeds, names or comments across invocations — every session re-imports and
  re-analyzes (~20 min for this 25 MB `.text`). Our persistent project at `refs/ghidra/proj/
  OpenSporeOracle` retains them, which is what makes seeding cheap. Also: our bridge is what turns
  the whole claim ledger into a batch verdict in one pass, which is an audit, not an investigation.
* Two settings that matter at this target size, both from REA's own docs:
  **`REA_GHIDRA_STARTUP_TIMEOUT_MS`** must be raised well above the 330,000 ms default, and
  **`GHIDRA_HEADLESS_MAXMEM`** defaults to `2G`. The MCP client's per-call timeout is a SEPARATE
  deadline and can fire first.
* Ghidra 12 **removed embedded Jython**, so `-postScript *.py` fails outright ("Ghidra was not started
  with PyGhidra"). REA is right about this and works around it with packaged Java scripts; ours runs
  under real CPython + JPype from the wheels inside the Ghidra distribution.

## Upstream knowledge: SporeMaster3 (secondary — do NOT start here)

**Priority rule (user, repeated): disassembly first, always.** Third-party sources — docs,
another team's reimplementation, even their *source code* — are **not a first port of call**.
They are only consulted when the disassembly is unclear, and even then only as a hint that is
immediately tested against the oracle (level 1) or our shipped data (level 2). Its C# source
did prove accurate where its prose was not, so *if* it is consulted, read the source rather
than the summaries — but that is a fallback, not a workflow.

`upstream/SporeMaster3/` (TRACKED, ~9.8 MB) holds a copy of another team's reverse-engineering
of the same game: 21 format docs + `reg_type/reg_file/reg_property/reg_simulator` +
`resources.csv` + the beta `KeyMap.txt`. Deliberately excluded: their 20 MB of hash-guesser
dictionaries (`guess_left/right.txt`), which are tooling, not format knowledge.

It is level 4 in the authority order, so **every claim from it was tested before being used**.
Score of this session:

| their claim | verdict | how |
|---|---|---|
| index dword[0] is a **shared-field bit mask** (bit0 type, bit1 group, bit2 unknown) | **CONFIRMED**, and it fixes our formula | their model implies `prefix = 4+4*popcount(K)`, `stride = 32-4*popcount(K)`; measured K=4→(8,28), K=6→(12,24), K=7→(16,20) on all 57 files |
| `compression:u16 = 0xFFFF` means RefPack, `0x0000` means raw | **CONFIRMED** on 150,945 records | probed the first bytes at every record's offset: 0xFFFF → starts `10 FB` in 100% of 98,407 samples and `packed < mem`; 0x0000 → `10 FB` in 0% of 52,538 samples and `packed == mem` |
| a full record is 32 bytes (type,group,unknown,instance,offset,packedSize,memSize,compression:u16,flags:u16) | **CONFIRMED** | explains the constant 36 = prefix+stride we had measured without understanding |
| "RefPack size field is little-endian" | **WRONG** — it is BIG-endian | `10 fb 00 08 48` → BE = 2120 = that record's `memSize`; LE = 4,720,640, absurd for a 1371-byte payload. Also confirmed in the oracle at `0x0092ccdc`, which reads the length through the big-endian helper at `0x0092c4e0` |
| "registry line without a hash means `ID = FNV(name)`" | **WRONG for type ids** | tested 6 pairs from `reg_type.txt` (`rw4`=0x2f4e681b, `prop`, `soundProp`, `gmdl`, `plt`, `creature_traits`) against FNV-1/FNV-1a × {raw,lower,upper}: **0/6 match**. Type ids are assigned, not hashed |
| `resources.csv` columns `type,group,instance,compressed,packedSize,memSize,path` | **CONFIRMED shape**, different package set | none of our type ids appear in it (it was generated from their corpus), but the column model matched our records exactly once split |

So: their **structural** model was right and we adopt it; their **byte-level** claims were
wrong in two places, and one of those (LE) would have broken our decoder had we copied it.
This is the authority order earning its keep.

## Our own hash bug, found because of them (FIXED)

Their docs say the hash is FNV-1; our `common.h` implemented **FNV-1a** with uppercase
folding. Disassembly of the oracle settles it — `0x00932e50`:

```
932e8a: imul edi,edi,0x1000193   ; h *= prime  FIRST
932e90: xor  edi,eax             ; h ^= c      SECOND
```

and the third argument selects the folding via the CRT (IAT slots verified with pefile):
mode 0 = raw (`0x932ed6`), mode 1 = `tolower` (`0x013cc2ec`), mode 2 = `toupper`
(`0x013cc500`). The state-registration site `0x007e40f2` pushes `(name, 0x811c9dc5, 1)`,
so **a game state id is FNV-1 with tolower**. `Loader` was `0x69cdcb22` (wrong) and is now
`0xd7d04c04` (matches the original's algorithm). This bug was silent by construction: ids
are only ever compared to other ids, so our own `state -list`/`goto` kept working.

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
- **Consequence for a "function map" (session 13):** because the PDB is a different link (0/12
  addresses match — `analysis/build_identity.md`), it can supply function NAMES only where identity
  matches, never addresses. So the denominator for code coverage comes from the ORACLE itself:
  `tools/functions.py` counts candidate starts in `.text` (E8/E9 rel32 targets + .rdata/.data
  pointers into .text + prologues) = **105,239** (an upper bound), against the
  `analysis/functions_named.json` list = **29** reversed → **0.021%**. This is the honest counterweight
  to the (deliberately scoped) feature map; the map draws both.
- **The PDB is NOT stripped:** it has no `/GLOBAL`/`/PUBLIC` streams, but it is full of MSVC-mangled
  names — 194,027 unique, **136,433** unique function/method name paths (`tools/pdb_symbols.py` →
  `analysis/pdb_names.txt`, `analysis/pdb_symbols.md`). Subsystems: SP 63,246, EA 24,214, eastl 9,874,
  **rw 6,582** (RenderWare), nSPCreatureAnim 2,423, nSPSkinner 1,561.
- **Address guesser: partial, NOT full.** A full "PDB name -> oracle address" map is impossible, and
  the reasons are now verified, not assumed: (1) there is no beta BINARY, only the PDB, so there is no
  code to fingerprint; (2) no reference build matches the oracle (all `Also*` have different CodeView
  GUIDs); (3) the GA build is **/GR- (RTTI off)** — only 2 type-descriptor names exist — and it has
  almost no assert function-name strings. What DOES work is a string-reference guesser:
  `tools/name_guess.py` attributes each `.rdata` string referenced by a `push imm32`/`mov r32,imm32`
  to its enclosing function, then matches distinctive strings to PDB name tokens (scored by string
  length, token rarity, and how many functions use the string). Result: **428 candidate
  address->name guesses, 120 HIGH-confidence** (e.g. `0x007f6160 SPUICreateAnimationTimerRamp`,
  `0x0089f060 EA::Allocator::StackAllocatorCoreAllocate`, `0x008457a4
  EA::ArgScript::cParser::CommandExists`, `0x00a73e40 SP::EffectsManager`,
  `0x00ea6710 SP::cPlanetModel::IsUnderWater`). Every row is a GUESS to check by disassembly, not
  truth. Refined (session 13): use STRICT function starts (prologues + `E8` targets only — .rdata/
  .data pointers are often jump-table case labels and gave mid-function addresses); the push site is
  reported; and two limits are documented — it keys on self-describing functions (so it barely
  overlaps our hand-reversed workhorse routines: validation 0/29 by construction, not failure), and
  when one string matches several PDB names it identifies the FAMILY reliably but the member shown is
  arbitrary (`0x0092c540` pushes `Refpack` -> `EA::Refpack::*`). Coverage 220 / 48,243 strict starts =
  0.46%. Output: `analysis/name_guesses.md`.

## Startup chain (level 1: disasm of the oracle) — full doc in analysis/startup.md

Traced from the PE entry point (session 13). The chain is:

```
entry 0x011e11e1 -> 0x011e1588 (CRT init) -> 0x011e0f21 (__tmainCRTStartup)
  -> WinMain 0x00f489e0  (called at 0x11e105c with hInstance 0x400000)
  -> app_main 0x00f48810
  -> run 0x00f47e90
```

- **WinMain `0x00f489e0`** is a thin wrapper: ctor `0x92c420(nShowCmd)` -> `app_main` -> dtor.
- **app_main `0x00f48810`**: reads `multipleInstances` (0x0148da14); if absent, a named object
  `Global\SporeEditorOrGame` (0x0148d9f8) enforces one instance (`Another instance of the game is
  already running` 0x0148d9c8, return -1); allocates the App object (0x48 bytes, tag `App`
  0x013ebc48) and calls `run`.
- **run `0x00f47e90`**: `0x6b4eb0` early-init check; reads `-demo` (0x01413db0); allocates a
  328-byte App object; sets the filesystem mode from `-devDirs` 0x013f8dc8 / `-shipDirs` 0x013f8db4
  via `0x687bd0`; **CreateAppMode `0x007e8a00`** (alloc 0x1f0 = 496 bytes, tag `App`, ctor
  `0x007e76e0`) -> `[app+0x20]`; then subsystem setup and the main loop (vtable through `[app+0x20]`);
  reads `-baseGame` (0x014081a8) later.

Do not assume the AppMode ctor's contents; `0x007e76e0` is the next thing to walk. The `trigger`
resources are text and are read through this path (not yet located).

**The main loop is `0x008482f0`** (window-class vtable `.rdata 0x141cc10` slot `+0x84`), called from
`run` at `0xf48405` (`call [0x67dcb0()]->vtable+0x84`; `0x67dcb0` returns the window object from
global `0x15fd89c`). It is a classic message loop: `mov edi, ds:0x13cc62c` (**PeekMessageW**) then
`PeekMessageW(&msg,0,0,0,PM_REMOVE)` -> `cmp [msg.message],0x12` (WM_QUIT) -> dispatch to the window
event handlers `[esi+0x5c/0x60/0x64]` -> `TranslateMessage` (IAT 0x13cc5cc) -> `DispatchMessageW`
(IAT 0x13cc630) -> repeat until the queue is empty, return 1. **Trap:** the engine has TWO
PeekMessage users — the main loop uses `PeekMessageW` via a register (`edi`), while the mouse helper
`0x84a050` uses `PeekMessageA` directly (window slot `+0x88`). A search for direct callers of the
pump therefore misses the loop; the `ff 15 <IAT>` search misses it too because the IAT load and the
call are separated by `mov edi, [IAT]` + `call edi`.

## AppStateManager / game states (level 1: disasm of the oracle)

The user remembered this as "типа режимы запуска, управлял в девбилде" — it is real, and it is
reachable in the **release** build too.

- **State identity = FNV-1 (NOT FNV-1a) 32 of the name**, stored at `[state+0x28]`:
  ```
  7e40f2: push 0x811c9dc5 / push eax(name) / call 0x932e50   ; hash
  7e4103: mov [ecx+0x28], eax                                ; store as the ID
  7e4132: cmp ecx,[eax+edx*4]                                ; duplicate-ID probe
  7e413b: push 0x1413600                                     ; "A state with this ID already exists"
  ```
  **FNV-1, i.e. multiply THEN xor — the direction matters and this heading used to say FNV-1a.**
  The hasher's own loop is unambiguous:
  ```
  932e8a: imul edi,edi,0x1000193    ; h = h * prime     FIRST
  932e90: xor  edi,eax              ; h = h ^ c         SECOND
  ```
  The Ghidra audit (session 13) confirmed both the direction (`pseudo_regex` matched `* 0x1000193 ^`)
  and the absence of the FNV-1a form (`pseudo_not_regex` on `(h ^ c) * prime` PASSED). Note also that
  **the seed `0x811c9dc5` is a PARAMETER pushed by the caller** at `0x7e40f2`; the hasher reads it from
  `[esp+0x14]` and contains only the prime. Our `fnv1_lower` in `source/common.h` implements this
  primitive; every group/instance/state id in the project depends on the multiply-first direction.
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
sites, **60 distinct names** (session 12; the earlier census dropped single-char names).
Notable: `devDirs shipDirs state preload safe noVSync vSync noDev
noPlugins noScripts noSound dumpShaders dumpShadersForPIX dumptelemetry multipleInstances
writeableData userDataDir dataDir dataBaseDir locale baseGame ep1 demo gonzago flock mce mono
stereo pix hostIP clientIP isHost jobPriority jobThreads nodefaultvox nosamplecache
noBackground nofocus noDevEffects noEffectScripts noMaterialScripts showConfigAlerts silent
verboseaudio 4.0 5.1 automation` **and the device flags `w` (windowed), `f` (fullscreen),
`r` (resolution `WxH`)**.
Keys are stored **without** a leading dash; the dash is the shell-side convention. The key/value
separator in the original is a **COLON**: states are launched as `-state:SpaceAccessoriesEditor`
(and generally `-key:value`). Our parser accepts both `:` and `=` (splitting on whichever comes
first, so a Windows path in a value keeps its colons).
**Trap:** the original really does use single-letter flags (`w`/`f`/`r`), so a name-length
filter of ≥2 silently loses them — `tools/arg_vocab.py` now takes `min_len=1` at a verified
helper call site.

## Traps hit in session 13 (mesh rendering + command line)

1. **The FIRST command-line argument was silently dropped.** `wWinMain`'s `cmdline` is
   `lpCmdLine`, which EXCLUDES the program name, but `parse_command_line` erased the first token
   as if it were `argv[0]`. So `-headless -devDirs` lost `-headless` (and any leading flag). It hid
   because `-w` was always first and windowed is the default, and because the `-f`/`-r` tests put
   `-headless` first so `-f` was really the second argument. Fixed with an explicit
   `has_program_name` parameter: `false` for lpCmdLine, `true` for `GetCommandLineW()`. If you add
   a new entry point, keep that distinction. (Consequence: session 12's "headless" run was not
   headless - recorded as a correction in HISTORY.)
2. **Do not put D3D resource pointers inside `Loader`.** The state is reset by copy-assignment
   (`g_loader = Loader{}`), which would copy a raw pointer and double-free it. The mesh and its
   name live in file-scope statics instead.
3. **An FVF with no texture coordinates must disable the texture stage**, or the mesh is modulated
   by whatever texture the 2D overlay left bound: set `COLOROP = SELECTARG1(DIFFUSE)` and
   `ALPHAOP = DISABLE` before `DrawIndexedPrimitive`, and `SetTexture(0, nullptr)`.
4. **`D3DXVECTOR3` is a D3DX type** and D3DX is not available here (no June 2010 SDK). Use the
   plain `D3DVECTOR` struct or your own math (the matrices in `render/mesh.cpp` are hand-built).

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
