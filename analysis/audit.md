# Ghidra audit of every claim we have made

Independent re-check of `analysis/claims.json` against Ghidra's own analysis of
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`.

- Ghidra 12.1.4, language x86:LE:32:default, image base 0x00400000, entry None, 54611 functions carved.
- Claims: FAIL 1, UNKNOWN 11, PASS 17, SKIP 1

`FAIL` = Ghidra disagrees with the doc. `UNKNOWN` = Ghidra could not answer, so the
claim is still only as good as our own objdump reading. `SKIP` = not a disassembly
question at all. **Only `FAIL` is a correction; `UNKNOWN` is unfinished work.**

## FAIL (1)

| claim | sev | doc | verdict |
|---|---|---|---|
| `main.loop.caller` | load-bearing | analysis/startup.md:95-96 | **FAIL** |

### main.loop.caller — FAIL

- `PASS` [ok] `func` [PASS] calls 0x0067dcb0 present
  - claim: the window-object getter
- `PASS` [ok] `func` [PASS] reads global 0x015fd89c
  - claim: the window singleton
- `FAIL` [MISMATCH] `insn_at` 0x00f48405 is 'MOV EAX,dword ptr [EDX + 0x84]', we expected /mov.+:.*0x84/
  - claim: 0xf48405 LOADS the vtable+0x84 slot. CORRECTED by the audit: startup.md called this the 'call site'; the actual indirect call is the very next instruction. Regex is whitespace/case tolerant because Ghidra's printer writes 'MOV EAX,dword ptr [EDX + 0x84]'.
- `PASS` [ok] `insn_at` 0x00f4840b: CALL EAX
  - claim: THE main-loop call site is 0xf4840b (`call eax`), not 0xf48405

## UNKNOWN (11)

| claim | sev | doc | verdict |
|---|---|---|---|
| `pe.shape` | load-bearing | analysis/startup.md:7 | **UNKNOWN** |
| `appmode.shaders` | medium | analysis/startup.md:85 | **UNKNOWN** |
| `window.class` | load-bearing | analysis/startup.md:132-135 | **UNKNOWN** |
| `event.dispatcher` | high | analysis/startup.md:141-142 | **UNKNOWN** |
| `appmode.state.names` | load-bearing | analysis/startup.md:144-156 | **UNKNOWN** |
| `statemgr.class` | load-bearing | VERIFY.md row 3-6, source/states/statemgr.h | **UNKNOWN** |
| `hash.fnv1` | load-bearing | MEMORY.md (FNV-1 mul-then-xor, mode 1 = tolower) | **UNKNOWN** |
| `refpack.decoder` | load-bearing | source/engine/pkg.h:46-50 | **UNKNOWN** |
| `config.lang` | high | VERIFY.md row 41 | **UNKNOWN** |
| `resmgr.spui` | high | analysis/ui.md, RESUME.md | **UNKNOWN** |
| `allocators` | medium | analysis/startup.md:32,75,87 | **UNKNOWN** |

### pe.shape — UNKNOWN

- `UNKNOWN` [?] `entry_point` no entry_point in Ghidra meta
- `SKIP` [skip] `subsystem` PE-header fact, not exposed by the Ghidra export; check with tools/pe.py
- `PASS` [ok] `image_base` Ghidra says 0x00400000
- `SKIP` [skip] `section_executable` PE-header fact, not exposed by the Ghidra export; check with tools/pe.py

### appmode.shaders — UNKNOWN

- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007eaae0 (listing: MOV EAX,FS:[0x0]). Either our address is wrong or Ghidra failed to carve a function there.

### window.class — UNKNOWN

- `PASS` [ok] `vtable_slot` [0x0141cc10+0x0] = 0x00849e80
  - claim: CORRECTED by the audit: slot 0 is the DELETING DESTRUCTOR (0x849e80), not the ctor. startup.md was right that the ctor is 0x849b20 - it is simply not a vtable slot, which is why reading slot 0 for it FAILed. The two slots that matter (+0x84 main loop, +0x88 mouse pump) both PASSED.
- `PASS` [ok] `vtable_slot` [0x0141cc10+0x84] = 0x008482f0
  - claim: +0x84 THE MAIN LOOP
- `PASS` [ok] `vtable_slot` [0x0141cc10+0x88] = 0x0084a050
  - claim: +0x88 the mouse pump
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x00849bf0 (listing: SUB ESP,0x14c). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: Window::Create

### event.dispatcher — UNKNOWN

- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e7160 (listing: None). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: hashed event ids 0x255abf5 / 0xf62add / 0x212d3e7 / 0xae1cfe73
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e7160 (listing: None). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: Sleep(0xfa) in the retry loop

### appmode.state.names — UNKNOWN

- `PASS` [ok] `data_pointer` [0x0153f85c] = 0x01413868
  - claim: [ptr] -> 'quit'
- `PASS` [ok] `data_pointer` [0x0153f860] = 0x01413864
  - claim: [ptr] -> 'app'
- `PASS` [ok] `data_pointer` [0x0153f868] = 0x01413814
  - claim: [ptr] -> 'AppModePluginLoader', the initial AppMode state
- `PASS` [ok] `data_string` 'quit'
- `PASS` [ok] `data_string` 'AppModePluginLoader'
- `PASS` [ok] `data_string` 'EventLoadScreenBufferPercentage'
- `PASS` [ok] `data_string` 'UIWorld_Loading'
  - claim: stored as UTF-16LE; a 1-byte ASCII read of this address yields only 'U'
- `UNKNOWN` [?] `data_string` Ghidra found no readable string at 0x01550498
  - claim: NEGATIVE control: this is what our doc wrongly called EventLoadScreenBufferPercentage

### statemgr.class — UNKNOWN

- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e3fb0 (listing: MOV EAX,FS:[0x0]). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: state identity is the hash
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e3fb0 (listing: MOV EAX,FS:[0x0]). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: seed 0x811c9dc5
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e3fb0 (listing: MOV EAX,FS:[0x0]). Either our address is wrong or Ghidra failed to carve a function there.
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e3fb0 (listing: MOV EAX,FS:[0x0]). Either our address is wrong or Ghidra failed to carve a function there.
- `PASS` [ok] `vtable_slot` [0x01413260+0x0] = 0x007e3fb0

### hash.fnv1 — UNKNOWN

- `PASS` [ok] `func` [PASS] immediates 0x01000193 present
  - claim: the 0x01000193 prime is inside the hasher
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007e40f2 (listing: PUSH 0x811c9dc5). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: CORRECTED by the audit: the 0x811c9dc5 seed is NOT a constant inside 0x932e50 - the hasher takes it as a PARAMETER ([esp+0x14]) and the CALLER pushes it at 0x7e40f2. Our first audit pass FAILed because we looked for the seed in the wrong function.
- `PASS` [ok] `func` [PASS] imports tolower present
  - claim: mode arg 1 folds case through the CRT function loaded from IAT 0x13cc500
- `PASS` [ok] `func` [PASS] 3 IMUL
  - claim: TRAP: multiply BEFORE xor (FNV-1, not FNV-1a)
- `PASS` [ok] `pseudo_regex` pseudocode matches /\*\s*0x1000193\)?\s*\^|\^\s*[A-Za-z0-9_\[\]]+\)?\s*\*\s*0x1000193/: '* 0x1000193 ^'
  - claim: the decompiler's expression tree shows the OPERAND ORDER, which a flat listing hides: FNV-1 is (h*prime)^c, FNV-1a is (h^c)*prime. This is the single most load-bearing arithmetic fact in the project - every group/instance/state id depends on it.
- `PASS` [ok] `pseudo_not_regex` forbidden pattern /\(\s*[a-zA-Z_][A-Za-z0-9_]*\s*\^\s*[^)]{1,24}\)\s*\*\s*0x1000193/ absent
  - claim: forbidden: the FNV-1a form (h ^ c) * prime. If Ghidra shows this, our hash direction is wrong.
- `PASS` [ok] `hash_vectors` SporeAppStates -> 0xd6396913 OK; AppModePluginLoader: (no expectation recorded)
  - claim: GROUP/INSTANCE names only. A TYPE id is NOT a hash of its name (0/6 variants tested, IDEAS.md 5.20) - an earlier revision of this check asserted fnv1_lower('locale')==0x02fac0b6, which is FALSE (it is 0xe31a5c1b) and the audit rightly flagged it. Type ids come from reg_type.txt, never from hashing.

### refpack.decoder — UNKNOWN

- `PASS` [ok] `func` [PASS] no CMP of 0x00001ffb
  - claim: TRAP: (& 0x1FFF) == 0x10FB, so 0x10FB and 0x50FB are both valid signatures
- `UNKNOWN` [?] `func` check records no assertion
  - claim: TRAP: counters are test-before-decrement, so the byte count is reg+1

### config.lang — UNKNOWN

- `UNKNOWN` [?] `func` check records no assertion
  - claim: the command registrar exists as a function
- `UNKNOWN` [?] `func` check records no assertion
  - claim: setOption executor
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007c7f50 (listing: MOV EAX,dword ptr [ECX + 0x4]). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: setVariables executor
- `UNKNOWN` [?] `func` check records no assertion
  - claim: alert executor
- `UNKNOWN` [?] `func` Ghidra has NO function at 0x007c90f0 (listing: PUSH ECX). Either our address is wrong or Ghidra failed to carve a function there.
  - claim: card/vendor executor
- `PASS` [ok] `data_string` 'setOption'
- `PASS` [ok] `data_string` 'boolProp'

### resmgr.spui — UNKNOWN

- `UNKNOWN` [?] `func` check records no assertion
  - claim: the resource manager request site
- `PASS` [ok] `func` [PASS] calls 0x008de4b0 present
  - claim: the UI layout request
- `PASS` [ok] `func` [PASS] push of 0x0510a95b present
  - claim: id 0x510a95b (written as hex: an earlier revision put a hand-computed decimal here and it was wrong)
- `PASS` [ok] `vtable_slot` [0x01413260+0x24] = 0x007e19e0
  - claim: +0x24 used by the spui request path

### allocators — UNKNOWN

- `UNKNOWN` [?] `func` check records no assertion
  - claim: the tagged allocator used by app_main/run
- `UNKNOWN` [?] `func` check records no assertion
  - claim: the allocator pair member used by the spui registrar
- `UNKNOWN` [?] `func` check records no assertion
  - claim: the 0x148 allocation ctor

## PASS (17)

| claim | sev | doc | verdict |
|---|---|---|---|
| `chain.entry.crt` | high | analysis/startup.md:7-9 | **PASS** |
| `chain.tmain.winmain` | load-bearing | analysis/startup.md:11-18 | **PASS** |
| `chain.winmain.app_main` | load-bearing | analysis/startup.md:20-24 | **PASS** |
| `guard.single_instance` | high | analysis/startup.md:26-30 | **PASS** |
| `run.extent` | load-bearing | analysis/startup.md:69 | **PASS** |
| `run.args` | high | analysis/startup.md:72-74 | **PASS** |
| `run.fsmode` | load-bearing | analysis/startup.md:43,60-61 | **PASS** |
| `appmode.created` | load-bearing | analysis/startup.md:44-45,77-78 | **PASS** |
| `appmode.vtable` | load-bearing | analysis/startup.md:80-85 | **PASS** |
| `appmode.device` | high | analysis/startup.md:84 | **PASS** |
| `appmode.plugins` | medium | analysis/startup.md:83 | **PASS** |
| `main.loop` | load-bearing | analysis/startup.md:100-123 | **PASS** |
| `mouse.pump` | high | analysis/startup.md:125-128 | **PASS** |
| `window.created.in.appmode` | high | analysis/startup.md:136-140 | **PASS** |
| `statemgr.verbs` | medium | source/states/statemgr.h:14-17 | **PASS** |
| `model.manager.blocked` | load-bearing | RESUME.md Blocked: the rw4 handler slots are MAP OPS, not a parser | **PASS** |
| `flags.device` | high | VERIFY.md: -w/-f/-r are real original flags | **PASS** |

### chain.entry.crt — PASS

- `PASS` [ok] `func` [PASS] calls 0x011e1588 present
  - claim: entry calls _security_init_cookie
- `PASS` [ok] `func` [PASS] flows to 0x011e0f21
  - claim: entry jumps to __tmainCRTStartup

### chain.tmain.winmain — PASS

- `PASS` [ok] `func` [PASS] imports GetStartupInfoA present
- `PASS` [ok] `func` [PASS] calls 0x00f489e0 present
  - claim: the WinMain call
- `PASS` [ok] `func` [PASS] push of 0x00400000 present
  - claim: hInstance == image base

### chain.winmain.app_main — PASS

- `PASS` [ok] `func` [PASS] calls 0x00f48810 present
  - claim: the real main
- `PASS` [ok] `func` [PASS] calls 0x0092c420, 0x005d74e0 present
- `PASS` [ok] `func` [PASS] ret 0x10
  - claim: ret 0x10 => stdcall, 4 args

### guard.single_instance — PASS

- `PASS` [ok] `func` [PASS] calls 0x00f47e90 present
  - claim: run
- `PASS` [ok] `func` [PASS] calls 0x0092b570 present
  - claim: the arg lookup
- `PASS` [ok] `func` [PASS] string 0x0148da14 ('multipleInstances') referenced | [PASS] string 0x0148d9f8 ('Global\\SporeEditorOrGame') referenced | [PASS] string 0x0148d9c8 ('Another instance of the game is already ') referenced | [PASS] string 0x0140a5d8 ('Spore') referenced | [PASS] string 0x013ebc48 ('App') referenced

### run.extent — PASS

- `PASS` [ok] `func` [PASS] 0x00f4880a lies inside body 0x00f47e90..0x00f4880c (one function)
  - claim: run is ONE function spanning to 0xf4880a
- `PASS` [ok] `func` [PASS] ret 0x4

### run.args — PASS

- `PASS` [ok] `func` [PASS] string 0x01413db0 ('demo') referenced | [PASS] string 0x013f8dc8 ('devDirs') referenced | [PASS] string 0x013f8db4 ('shipDirs') referenced | [PASS] string 0x014081a8 ('baseGame') referenced | [PASS] string 0x0148d898 ('automation') referenced | [PASS] string 0x0148d88c ('mouse') referenced | [PASS] string 0x0148d870 ('hostIP') referenced | [PASS] string 0x0148d85c ('clientIP') referenced | [PASS] string 0x0148d84c ('isHost') referenced

### run.fsmode — PASS

- `PASS` [ok] `func` [PASS] calls 0x00687bd0 present
  - claim: dev/ship filesystem mode chooser
- `PASS` [ok] `func` [PASS] calls 0x006b4eb0, 0x0081d530, 0x008055e0 present

### appmode.created — PASS

- `PASS` [ok] `func` [PASS] calls 0x007e8a00 present
  - claim: CreateAppMode
- `PASS` [ok] `func` [PASS] push of 0x000001f0 present
  - claim: alloc 0x1f0 = 496 bytes
- `PASS` [ok] `func` [PASS] calls 0x007e76e0 present
  - claim: the AppMode ctor

### appmode.vtable — PASS

- `PASS` [ok] `vtable_slot` [0x01413b50+0x14] = 0x007e6850
  - claim: +0x14 subsystem init
- `PASS` [ok] `vtable_slot` [0x01413b50+0x18] = 0x007e93d0
  - claim: +0x18 plugins
- `PASS` [ok] `vtable_slot` [0x01413b50+0x1c] = 0x007e7430
  - claim: +0x1c create the graphics device
- `PASS` [ok] `vtable_slot` [0x01413b50+0x20] = 0x007eaae0
  - claim: +0x20 shader/script config

### appmode.device — PASS

- `PASS` [ok] `func` [PASS] push of 0x00000280, 0x000001e0 present
  - claim: tries 640x480 first
- `PASS` [ok] `func` [PASS] string 0x01413ac8 ('Could not create the Graphics Device.\nDi') referenced

### appmode.plugins — PASS

- `PASS` [ok] `func` [PASS] string 0x01413e28 ('noPlugins') referenced | [PASS] string 0x01411b8c ('safe') referenced
- `PASS` [ok] `func` [PASS] calls 0x007e8810 present

### main.loop — PASS

- `PASS` [ok] `func` [PASS] refs 0x013cc62c, 0x013cc5cc, 0x013cc630 present
  - claim: PeekMessageW / TranslateMessage / DispatchMessageW IAT slots
- `PASS` [ok] `func` [PASS] imports PeekMessageW, TranslateMessage, DispatchMessageW present
- `PASS` [ok] `func` [PASS] forbidden imports absent (peekmessagea)
  - claim: TRAP: the loop is the W variant; the A variant is the separate mouse pump
- `PASS` [ok] `func` [PASS] cmp of 0x00000012 present
  - claim: cmp [msg.message],0x12 == WM_QUIT
- `PASS` [ok] `func` [PASS] push of 0x00000001 present
  - claim: PM_REMOVE == 1
- `PASS` [ok] `func` [PASS] refs 0x013cc678 present
  - claim: SetForegroundWindow in the focus branch

### mouse.pump — PASS

- `PASS` [ok] `func` [PASS] imports PeekMessageA present
  - claim: TRAP: direct A-variant call, IAT 0x13cc5f0 - this is why the loop was missed
- `PASS` [ok] `func` [PASS] refs 0x013cc5f0 present

### window.created.in.appmode — PASS

- `PASS` [ok] `func_contains_insn` 0x007e6f21 inside 0x007e6850..0x007e7151: CALL 0x00849b20
  - claim: the window ctor call sits at 0x7e6f21
- `PASS` [ok] `func` [PASS] string 0x01413a2c ('flock') referenced | [PASS] string 0x014139d4 ('vSync') referenced | [PASS] string 0x014139c4 ('noVSync') referenced | [PASS] string 0x014139e0 ('Could not start the renderer.\nPlease ens') referenced

### statemgr.verbs — PASS

- `PASS` [ok] `data_string` 'state'
- `PASS` [ok] `data_string` 'gotoState'
- `PASS` [ok] `data_string` 'transition'
- `PASS` [ok] `data_string` 'entry'
- `PASS` [ok] `data_string` 'exit'
- `PASS` [ok] `data_string` 'endChain'
- `PASS` [ok] `data_string` '.eBT'

### model.manager.blocked — PASS

- `PASS` [ok] `func` [PASS] calls 0x007ac2a0 present
  - claim: factory -> ctor
- `PASS` [ok] `vtable_slot` [0x0140fb48+0x6c] = 0x007ab7f0
  - claim: +0x6c
- `PASS` [ok] `vtable_slot` [0x0140fb48+0x70] = 0x007ab050
  - claim: +0x70
- `PASS` [ok] `func` [PASS] calls 0x007aa7d0 present
  - claim: TRAP: it is a map INSERT, not a parser
- `PASS` [ok] `func` [PASS] calls 0x006b66f0 present
  - claim: TRAP: it is a map ERASE, not a parser
- `PASS` [ok] `direct_callers` 0 call xrefs (as claimed)
  - claim: TRAP: the registry dispatcher has ZERO direct callers - follow the stream object instead

### flags.device — PASS

- `PASS` [ok] `func_in_range` one function 0x007e7430..0x007e76d0 covers 0x007e74a0..0x007e7637
  - claim: the device-flag function lives in this span
- `PASS` [ok] `data_string` 'screenWindowed'
  - claim: TRAP: a RenderWare device metric, not a CLI flag

## SKIP (1)

| claim | sev | doc | verdict |
|---|---|---|---|
| `gmdl.stride` | load-bearing | MEMORY.md gmdl: stride = vertex_bytes / vertex_count | **SKIP** |

### gmdl.stride — SKIP

- `SKIP` [skip] `not_disasm_verifiable` data-derived: settled by measuring 11,041 real gmdl resources (6,979 parse), not by code. The trap it corrects: descriptor field 16 is a reserved code, not the stride.

