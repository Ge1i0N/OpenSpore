# Ghidra audit of every claim we have made

Independent re-check of `analysis/claims.json` against Ghidra's own analysis of
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`.

- Ghidra 12.1.4, language x86:LE:32:default, image base 0x00400000, entry None, 55294 functions carved.
- Claims: FAIL 0, UNKNOWN 3, PASS 29, SKIP 1

`FAIL` = Ghidra disagrees with the doc. `UNKNOWN` = Ghidra could not answer, so the
claim is still only as good as our own objdump reading. `SKIP` = not a disassembly
question at all. **Only `FAIL` is a correction; `UNKNOWN` is unfinished work.**

## UNKNOWN (3)

| claim | sev | doc | verdict |
|---|---|---|---|
| `event.dispatcher` | high | analysis/startup.md:141-142 | **UNKNOWN** |
| `refpack.decoder` | load-bearing | source/engine/pkg.h:46-50 | **UNKNOWN** |
| `spui.loading_path` | load-bearing | analysis/spui-loading.md | **UNKNOWN** |

### event.dispatcher — UNKNOWN

- `UNKNOWN` [?] `func` [UNKNOWN] no CMP of absent from Ghidra's view, but its body has undischarged gaps [0x007e71c5..0x007e72be (250 B)]; seed those with kind=code before treating this as a refutation
  - claim: hashed event ids 0x255abf5 / 0xf62add / 0xf62ade / 0x212d3e7 / 0xae1cfe73. GHIDRA-CONFIRMED after seeding: the decompiler renders the whole switch (param_2 < 0x255abf6 / == 0x255abf5 / == 0xf62add / ...). 0xf62ade is the FORWARD id the branch re-dispatches.
- `UNKNOWN` [?] `func` [UNKNOWN] immediate absent from Ghidra's view, but its body has undischarged gaps [0x007e71c5..0x007e72be (250 B)]; seed those with kind=code before treating this as a refutation
  - claim: CORRECTED, and this was the TENTH checker artifact of the audit: the retry loop stores 250 with `mov [esp+0x14],0xfa` (0x7e7264) and the counter with `mov [esp+0x30],0xa` (0x7e7233) - NEITHER is a PUSH, so a mnemonic-bucketed `pushes_imm` check FAILed a correct doc claim. Use the mnemonic-agnostic immediate check for values that reach the stack. Substance of the original claim verified: 10 retries x 250 ms.
- `PASS` [ok] `func` [PASS] imports SleepEx present
  - claim: the wait helper is SleepEx(ms, bAlertable=1) via IAT 0x13cc17c, NOT Sleep - a refinement the audit forced us to make. Verified by resolving the IAT slot through the PE import directory (KERNEL32.dll!SleepEx).

### refpack.decoder — UNKNOWN

- `UNKNOWN` [?] `func` [UNKNOWN] no CMP of absent from Ghidra's view, but its body has undischarged gaps [0x0092c62d..0x0092c62f (3 B); 0x0092c6ed..0x0092c6ef (3 B)]; seed those with kind=code before treating this as a refutation
  - claim: TRAP: (& 0x1FFF) == 0x10FB, so 0x10FB and 0x50FB are both valid signatures
- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_0092c5b0 (0x0092c5b0..0x0092c77f, 198 insns)
  - claim: the RefPack copy loop is a real function
- `UNKNOWN` [?] `func` check records no assertion
  - claim: TRAP: counters are test-before-decrement, so the byte count is reg+1

### spui.loading_path — UNKNOWN

- `UNKNOWN` [?] `func_exists` function 0x00810870 not in the export
  - claim: the cSPUILayoutResource factory
- `UNKNOWN` [?] `func` function 0x00810870 not in the export
  - claim: gated on the resource-manager request tag
- `UNKNOWN` [?] `func` function 0x00810870 not in the export
  - claim: the allocation tag names the class
- `UNKNOWN` [?] `func_exists` function 0x00810940 not in the export
  - claim: the layout loader
- `UNKNOWN` [?] `func` function 0x00810940 not in the export
  - claim: <<< THE POINT: the loader compares the resource's type word against 0x250FE9A2, so the spui type id is confirmed in code, not inferred from reg_type.txt + the index
- `UNKNOWN` [?] `func` function 0x00810940 not in the export
  - claim: the same request tag gates the loader too
- `UNKNOWN` [?] `func` function 0x00810940 not in the export
  - claim: the three named classes of the reader triple
- `UNKNOWN` [?] `func` function 0x00810940 not in the export
  - claim: TokenIOContext 0x38, MemoryStream 0x24, SharedPointer 0x18 - the object sizes, so our engine's model has known extents
- `UNKNOWN` [?] `func_exists` function 0x00903330 not in the export
  - claim: the TokenIOContext constructor
- `UNKNOWN` [?] `func` function 0x00903330 not in the export
  - claim: TRAP-AND-PAYOFF: 0x143a460 is stored into [esi] exactly where a vtable goes, but it is an ASCII STRING. Reading it as a vtable would have invented a fake 12-method class; reading it as a string names the reader family - XML/TokenReader.
- `UNKNOWN` [?] `func` function 0x00903330 not in the export
  - claim: the read/write callbacks the token reader installs over the MemoryStream
- `UNKNOWN` [?] `func_exists` function 0x00903100 not in the export
  - claim: the XML/TokenReader user
- `UNKNOWN` [?] `func` function 0x00903100 not in the export
  - claim: the reader's own name
- `UNKNOWN` [?] `func` function 0x00903100 not in the export
  - claim: THE NEXT STEP: the token reader's core. Recovering its token alphabet is what turns spui from a bit-packed mystery into a parsed layout.

## PASS (29)

| claim | sev | doc | verdict |
|---|---|---|---|
| `pe.shape` | load-bearing | analysis/startup.md:7 | **PASS** |
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
| `appmode.shaders` | medium | analysis/startup.md:85 | **PASS** |
| `window.class` | load-bearing | analysis/startup.md:132-135 | **PASS** |
| `main.loop` | load-bearing | analysis/startup.md:100-123 | **PASS** |
| `main.loop.caller` | load-bearing | analysis/startup.md:95-96 | **PASS** |
| `mouse.pump` | high | analysis/startup.md:125-128 | **PASS** |
| `window.created.in.appmode` | high | analysis/startup.md:136-140 | **PASS** |
| `appmode.state.names` | load-bearing | analysis/startup.md:144-156 | **PASS** |
| `statemgr.class` | load-bearing | VERIFY.md row 3-6, source/states/statemgr.h | **PASS** |
| `statemgr.verbs` | medium | source/states/statemgr.h:14-17 | **PASS** |
| `hash.fnv1` | load-bearing | MEMORY.md (FNV-1 mul-then-xor, mode 1 = tolower) | **PASS** |
| `config.lang` | high | VERIFY.md row 41 | **PASS** |
| `resmgr.spui` | high | analysis/ui.md, RESUME.md | **PASS** |
| `model.manager.blocked` | load-bearing | RESUME.md Blocked: the rw4 handler slots are MAP OPS, not a parser | **PASS** |
| `flags.device` | high | VERIFY.md: -w/-f/-r are real original flags | **PASS** |
| `allocators` | medium | analysis/startup.md:32,75,87 | **PASS** |
| `statemgr.id_override` | load-bearing | analysis/states.md, source/states/statemgr.cpp::find | **PASS** |
| `statemgr.chain_not_stack` | high | analysis/states.md, source/states/statemgr.cpp::request_goto | **PASS** |

### pe.shape — PASS

- `PASS` [ok] `func_exists` Ghidra has a function here: entry (0x011e11e1..0x011e11ea, 2 insns)
  - claim: CORRECTED: the `entry_point` check was replaced, not dropped. ProgramDB has no getEntrypoint() in this Ghidra version, so the export could only return None, and inventing a value from our own PE parse would have made the check CIRCULAR (we would be verifying our reading against our reading). Asking whether Ghidra has a FUNCTION at the documented entry is the same fact in a form Ghidra can answer independently. The raw header value stays verifiable with tools/pe.py.
- `SKIP` [skip] `subsystem` PE-header fact, not exposed by the Ghidra export; check with tools/pe.py
- `PASS` [ok] `image_base` Ghidra says 0x00400000
- `SKIP` [skip] `section_executable` PE-header fact, not exposed by the Ghidra export; check with tools/pe.py

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

### appmode.shaders — PASS

- `PASS` [ok] `func` [PASS] string 0x014142a8 ('dumpShaders') referenced | [PASS] string 0x01414284 ('dumpShadersForPIX') referenced | [PASS] string 0x01414254 ('dumpFragmentShaders') referenced | [PASS] string 0x01414230 ('dumpDirectShaders') referenced | [PASS] string 0x0141421c ('noScripts') referenced | [PASS] string 0x014141f8 ('noMaterialScripts') referenced | [PASS] string 0x014141d8 ('noEffectScripts') referenced

### window.class — PASS

- `PASS` [ok] `vtable_slot` [0x0141cc10+0x0] = 0x00849e80
  - claim: CORRECTED by the audit: slot 0 is the DELETING DESTRUCTOR (0x849e80), not the ctor. startup.md was right that the ctor is 0x849b20 - it is simply not a vtable slot, which is why reading slot 0 for it FAILed. The two slots that matter (+0x84 main loop, +0x88 mouse pump) both PASSED.
- `PASS` [ok] `vtable_slot` [0x0141cc10+0x84] = 0x008482f0
  - claim: +0x84 THE MAIN LOOP
- `PASS` [ok] `vtable_slot` [0x0141cc10+0x88] = 0x0084a050
  - claim: +0x88 the mouse pump
- `PASS` [ok] `func` [PASS] imports RegisterClassExW, CreateWindowExW present
  - claim: Window::Create

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

### main.loop.caller — PASS

- `PASS` [ok] `func` [PASS] calls 0x0067dcb0 present
  - claim: the window-object getter
- `PASS` [ok] `func` [PASS] reads global 0x015fd89c
  - claim: the window singleton
- `PASS` [ok] `insn_at` 0x00f48405: MOV EAX,dword ptr [EDX + 0x84]
  - claim: 0xf48405 LOADS the vtable+0x84 slot. CORRECTED by the audit: startup.md called this the 'call site'; the indirect call is the very next instruction. Whitespace/case tolerant because Ghidra's printer writes 'MOV EAX,dword ptr [EDX + 0x84]' (no colon, spaces inside the brackets).
- `PASS` [ok] `insn_at` 0x00f4840b: CALL EAX
  - claim: THE main-loop call site is 0xf4840b (`call eax`), not 0xf48405

### mouse.pump — PASS

- `PASS` [ok] `func` [PASS] imports PeekMessageA present
  - claim: TRAP: direct A-variant call, IAT 0x13cc5f0 - this is why the loop was missed
- `PASS` [ok] `func` [PASS] refs 0x013cc5f0 present

### window.created.in.appmode — PASS

- `PASS` [ok] `func_contains_insn` 0x007e6f21 inside 0x007e6850..0x007e7151: CALL 0x00849b20
  - claim: the window ctor call sits at 0x7e6f21
- `PASS` [ok] `func` [PASS] string 0x01413a2c ('flock') referenced | [PASS] string 0x014139d4 ('vSync') referenced | [PASS] string 0x014139c4 ('noVSync') referenced | [PASS] string 0x014139e0 ('Could not start the renderer.\nPlease ens') referenced

### appmode.state.names — PASS

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
- `PASS` [ok] `data_pointer` [0x01550498] = 0x01446c18
  - claim: NEGATIVE control: our doc called this address EventLoadScreenBufferPercentage. It is a pointer to 'EAReportUploader'.
- `PASS` [ok] `data_string` 'EAReportUploader'

### statemgr.class — PASS

- `PASS` [ok] `func` [PASS] calls 0x00932e50 present
  - claim: state identity is the hash
- `PASS` [ok] `func` [PASS] push of 0x811c9dc5 present
  - claim: seed 0x811c9dc5
- `PASS` [ok] `func` [PASS] string 0x01413600 ('A state with this ID already exists') referenced
- `PASS` [ok] `func` [PASS] no reference from 0x007e3fb0 to string 0x0141319c ('AppStateManager')
- `PASS` [ok] `vtable_slot` [0x01413260+0x0] = 0x007e3fb0

### statemgr.verbs — PASS

- `PASS` [ok] `data_string` 'state'
- `PASS` [ok] `data_string` 'gotoState'
- `PASS` [ok] `data_string` 'transition'
- `PASS` [ok] `data_string` 'entry'
- `PASS` [ok] `data_string` 'exit'
- `PASS` [ok] `data_string` 'endChain'
- `PASS` [ok] `data_string` '.eBT'

### hash.fnv1 — PASS

- `PASS` [ok] `func` [PASS] immediates 0x01000193 present
  - claim: the 0x01000193 prime is inside the hasher
- `PASS` [ok] `func` [PASS] push of 0x811c9dc5 present
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

### config.lang — PASS

- `PASS` [ok] `func_exists` Ghidra has a function here: os_007c8550 (0x007c8550..0x007c85df, 46 insns)
  - claim: the command registrar (0x7c85e7 is MID-function; the entry is 0x7c8550 - found by tools/gen_seeds.py walking back to the int3 gap)
- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_007c8e90 (0x007c8e90..0x007c8efb, 41 insns)
  - claim: setOption executor
- `PASS` [ok] `func_exists` Ghidra has a function here: os_007c7f50 (0x007c7f50..0x007c7f5e, 4 insns)
  - claim: setVariables executor; Ghidra had not carved this one (indirect reach only) - see seeds.tsv
- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_007c8f00 (0x007c8f00..0x007c9017, 97 insns)
  - claim: alert executor
- `PASS` [ok] `func_exists` Ghidra has a function here: os_007c90f0 (0x007c90f0..0x007c91c7, 87 insns)
  - claim: card/vendor executor; likewise uncarved before seeding
- `PASS` [ok] `data_string` 'setOption'
- `PASS` [ok] `data_string` 'boolProp'

### resmgr.spui — PASS

- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_008de4b0 (0x008de4b0..0x008de4b5, 2 insns)
  - claim: the resource manager request site
- `PASS` [ok] `func` [PASS] calls 0x008de4b0 present
  - claim: the UI layout request
- `PASS` [ok] `func` [PASS] push of 0x0510a95b present
  - claim: id 0x510a95b (written as hex: an earlier revision put a hand-computed decimal here and it was wrong)
- `PASS` [ok] `vtable_slot` [0x01413260+0x24] = 0x007e19e0
  - claim: +0x24 used by the spui request path

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

### allocators — PASS

- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_00f47240 (0x00f47240..0x00f47269, 15 insns)
  - claim: the tagged allocator used by app_main/run
- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_00951230 (0x00951230..0x0095128b, 41 insns)
  - claim: the allocator pair member used by the spui registrar
- `PASS` [ok] `func_exists` Ghidra has a function here: FUN_00920960 (0x00920960..0x009209e8, 28 insns)
  - claim: the 0x148 allocation ctor

### statemgr.id_override — PASS

- `PASS` [ok] `func` [PASS] push of 0x811c9dc5 present
  - claim: the hash path exists and pushes the seed
- `PASS` [ok] `func` [PASS] calls 0x00932e50 present
  - claim: it calls the FNV hasher
- `PASS` [ok] `func` [PASS] string 0x013fc8f0 ('id') referenced
  - claim: the ArgScript arg is literally "id" (the leading dash is the ArgScript layer's convention, which is why a search for the literal '-id' finds nothing in the binary)
- `PASS` [ok] `insn_at` 0x007e40d6: JZ 0x007e40ea
  - claim: THE BRANCH: `je 0x7e40ea` - when the `id` arg is ABSENT it jumps to the hashing block
- `PASS` [ok] `insn_at` 0x007e40e8: JMP 0x007e4100
  - claim: and `jmp 0x7e4100` - when the `id` arg is PRESENT the hash is SKIPPED entirely, so the explicit value is the identity
- `PASS` [ok] `insn_at` 0x007e4103: MOV dword ptr [ECX + 0x28],EAX
  - claim: BOTH paths converge on `mov [ecx+0x28],eax` - one field holds either the assigned id or the hash

### statemgr.chain_not_stack — PASS

- `PASS` [ok] `data_string` 'endChain'
- `PASS` [ok] `data_string` 'gotoState'
- `PASS` [ok] `data_string` "can't nest states or transitions"
- `PASS` [ok] `func` [PASS] string 0x014135dc ("can't nest states or transitions") referenced
  - claim: the no-nesting assert lives in the state-ADD path, which is why a chained gotoState must be QUEUED rather than called reentrantly

## SKIP (1)

| claim | sev | doc | verdict |
|---|---|---|---|
| `gmdl.stride` | load-bearing | MEMORY.md gmdl: stride = vertex_bytes / vertex_count | **SKIP** |

### gmdl.stride — SKIP

- `SKIP` [skip] `not_disasm_verifiable` data-derived: settled by measuring 11,041 real gmdl resources (6,979 parse), not by code. The trap it corrects: descriptor field 16 is a reserved code, not the stride.

