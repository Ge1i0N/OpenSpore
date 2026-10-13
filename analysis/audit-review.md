# Audit review: did we start reversing correctly? (session 13, Ghidra 12.1.4)

The user's request: check **everything we have reversed, starting from WinMain**, to see whether we
started correctly at all. This is the human reading of the machine-generated `analysis/audit.md`
(which `tools/ghidra_audit.py verify` overwrites on every run).

## Headline: the startup chain is CORRECT

`analysis/claims.json` now holds **30 claims / 104 checks** covering the whole reversed surface from
the PE entry point forward. Ghidra's independent verdict:

| verdict | claims | what it means |
|---|---|---|
| **PASS** | 17 | Ghidra agrees with our doc, on its own analysis |
| **FAIL** | 1 (was 10) | see below — 9 of the 10 were bugs in MY checker |
| **UNKNOWN** | 11 | Ghidra could not answer; still only as good as our objdump reading |
| **SKIP** | 1 | not a disassembly question (the gmdl stride is data-derived) |

Every load-bearing link of the chain we built our engine on came back **PASS**:

```
entry 0x11e11e1 -> 0x11e1588 (security cookie) -> 0x11e0f21 (__tmainCRTStartup)   PASS
0x11e0f21 -> WinMain 0xf489e0, hInstance 0x400000, GetStartupInfoA                PASS
0xf489e0 -> app_main 0xf48810, ret 0x10 (stdcall, 4 args)                          PASS
0xf48810 -> run 0xf47e90, the arg lookup 0x92b570, all five guard strings          PASS
run is ONE function 0xf47e90..0xf4880c, ret 0x4, all NINE arg names                PASS
run -> 0x687bd0 (dev/ship fs mode), 0x6b4eb0, 0x81d530, 0x8055e0                   PASS
run -> CreateAppMode 0x7e8a00, alloc 0x1f0 = 496, ctor 0x7e76e0                    PASS
AppMode +0x1c create device: push 640, push 480, "Could not create the Graphics
  Device.\nDirect X is unavailable."                                                PASS
AppMode +0x18 plugins: noPlugins / safe / 0x7e8810                                 PASS
main loop 0x8482f0: PeekMessageW + TranslateMessage + DispatchMessageW,
  WM_QUIT == 0x12, PM_REMOVE == 1, SetForegroundWindow,
  and NEGATIVELY: PeekMessageA is NOT in this function                              PASS
mouse pump 0x84a050: PeekMessageA IS here (IAT 0x13cc5f0)                           PASS
window ctor call sits at 0x7e6f21 inside 0x7e6850..0x7e7151                         PASS
flock / vSync / noVSync / "Could not start the renderer."                           PASS
-w/-f/-r span 0x7e74a0..0x7e7637 is ONE function (0x7e7430..0x7e76d0)               PASS
```

So: **yes, we started correctly.** The boot chain, the AppMode dispatch, the window class, the main
loop and the arg surface all survive an independent decompiler. The two `TRAP` notes we wrote for
ourselves (the W-vs-A PeekMessage split, and `screenWindowed` being a RenderWare metric rather than a
CLI flag) were confirmed, including the negative form of the first one.

## The one real doc error the audit found

`analysis/startup.md:95-96` said `run` reaches the main loop at **`0xf48405`**. It does not:

```
f48401  mov edx,[eax]        ; the window object's vtable
f48403  mov ecx,eax          ; this = the window object
f48405  mov eax,[edx+0x84]   ; LOAD the +0x84 slot   <-- what we cited
f4840b  call eax             ; THE CALL SITE
```

The distinction matters because it is the pattern used everywhere in this binary: the slot load and
the call are separate instructions, so a search for `call [reg+0x84]` finds nothing and a search for
the load finds the right neighbourhood. Corrected in `startup.md` and pinned by two `insn_at` checks
so the difference cannot regress.

Second, smaller: window vtable `0x141cc10` **slot 0 is `0x849e80`, the deleting destructor**, not the
constructor. `startup.md` was right that the ctor is `0x849b20` — it simply is not a vtable slot, which
is why reading slot 0 to prove it failed. The two slots that actually matter (`+0x84` main loop,
`+0x88` mouse pump) both PASSED.

## Nine of the ten initial FAILs were my checker, not our reversal

This is the more transferable result. The first audit run reported `FAIL 10`, and the temptation would
have been to believe it and rewrite ten documented facts. What they actually were:

1. **Six — byte order.** `Memory.getInt(addr, True)` does **not** mean "unaligned OK", its second
   parameter is `bigEndIndexOrder`. Every vtable word came back reversed:
   `[0x01413b50+0x14] = 0x50687e00` against our documented `0x007e6850` — the same four bytes
   backwards. The giveaway was that the "wrong" values were recognisably our values permuted, which
   is why reading the evidence instead of the verdict mattered.
2. **Three — my own string/regex artifacts.**
   - `0x013ebc48` "reads `0x41`, claimed `App`": Ghidra typed that datum as the *integer* `0x41`; the
     bytes are literally `41 70 70 00` = `"App"`. My code stringified a non-string datum instead of
     falling through to the raw read.
   - Four "no readable string" reports came from `_raw_bytes` using `JArray("b")`, which raises
     `ClassNotFoundException('b')` — the element type must be the `JByte` **class**. The exception was
     swallowed, so the scan silently returned nothing.
   - `insn_at` regexes written against objdump's spelling (`mov eax,DWORD PTR [edx+0x84]`) cannot match
     Ghidra's (`MOV EAX,dword ptr [EDX + 0x84]`). Whitespace and case have to be tolerated.
3. **One — a genuinely wrong expectation I had introduced that same session.** I had written
   `fnv1_lower("locale") == 0x02fac0b6` into a check. It is `0xe31a5c1b`. The audit was right to fail
   it, and it **re-confirms the existing rule** (type ids are assigned, never hashed — `IDEAS.md`
   5.20). The claim now says so explicitly, with `AppModePluginLoader` as the group/instance example
   instead.

**Lesson, and it is the same one the field guide keeps teaching: a FAIL from a tool is a hypothesis
about the tool as much as about the code. Read the evidence, never act on the verdict alone.**

## The structural finding: Ghidra does not carve vtable-reachable functions

Seven cited addresses had **no function at all** in Ghidra's 54,611-function inventory:

```
0x007e3fb0  statemgr add-state (vtable+0x00)     0x00849bf0  Window::Create
0x007e7160  app event dispatcher                0x007c7f50  config: setVariables executor
0x007eaae0  AppMode +0x20 shaders               0x007c90f0  config: card/vendor executor
0x007e40f2  (mid-function: the seed push site)
```

`tools/gen_seeds.py` walked each back to the preceding `int3` gap and proved **six of them are real
function entries** — they were never reached because they are callable *only* through a vtable. Six of
the seven begin with the MSVC SEH prologue `mov eax,fs:0x0`, which Ghidra left typed as data (at
`0x7e7160` it produced the datum `?? 64h`). The objdump listing shows what is actually there:

```
7e7160  mov eax,fs:0x0            ; SEH prologue - Ghidra stopped here
7e7166  push 0xffffffff
7e7168  push 0x1217388            ; the SEH handler
7e716e  mov eax,[esp+0x10]
7e717c  cmp eax,0x255abf5         <-- our documented hashed event id
7e718d  cmp eax,0xf62add
7e7194  cmp eax,0x212d3e7
```

All three of the event ids we documented are sitting in plain sight in the function Ghidra refused to
disassemble. **This reframes our three standing blockers (rw4, spui, prop).** They were recorded as
"the handler slots are map operations, follow the stream object" — which the audit *confirmed* (the
`direct_callers == 0` check on the registry dispatcher PASSED outright). But the deeper reason we kept
failing to find the parser is now visible: the interesting functions in this binary are reached through
vtables, and **neither auto-analysis nor a text search for `call` finds them.** The fix is seeding, in
both toolchains: `analysis/audit/seeds.tsv` (21 entries) feeds our `tools/ghidra_seed.py`, and the same
file is what `REA_GHIDRA_SEED_FILE` wants.

## What the audit does NOT cover

Stated so nobody reads 17 PASS as "verified end to end":

* **`gmdl.stride` is SKIP by design.** It was settled by measuring 11,041 real resources (6,979 parse),
  not by reading code. No decompiler can confirm or refute it.
* **11 claims are still UNKNOWN.** Most are "Ghidra carved no function here" (now being fixed by
  seeding) or "this check records no assertion" (checks I wrote as notes rather than assertions —
  `config.lang`, `allocators`, `resmgr.spui` each carry several). Those are unfinished work, not
  passes.
* **`pe.shape`'s entry point is UNKNOWN**: `ProgramDB` has no `getEntrypoint()` in this version, so the
  export returns None rather than a value we computed ourselves — a self-computed entry would have made
  the check circular. Verify with `tools/pe.py` instead.
* **The whole data half of the project is outside this audit**: package indexing, RefPack output,
  raster/gmdl/spui content, locale text. Those are checked by running our engine against the shipped
  files, which is a different kind of evidence.
* **`refpack.decoder`'s counters** (test-before-decrement, so the byte count is reg+1) are still
  UNKNOWN — that is an ordering claim, and it needs the pseudocode now that the function is seeded.

## Toolchain decision: REA is primary, our bridge is the fallback

REA 6.4.0 is installed globally and registered as an MCP server in
`C:\Users\DartPower\.config\opencode\opencode.json` (`rea doctor --provider ghidra --json` →
`healthy: true`: Ghidra 12.1.4, native `decompile.exe` for win_x86_64, JDK 21 64-bit, Windows native
authority verified). Its 170 tools are reachable here as `tools.rea.*`.

Why both remain, rather than one:

* **REA is the everyday instrument.** 25 read-only Ghidra operations, an evidence contract that
  reports `facet_availability` per facet, and `list_unknowns`/`record_unknown` for explicitly tracking
  what is unresolved. Its rule that "an empty call list with partial or unknown coverage is not proof
  that the function has no calls" is precisely the discipline our `direct_callers == 0` claim needed.
* **Our PyGhidra bridge is the persistent-project path.** REA deliberately uses an ephemeral
  `-readOnly -deleteProject` session per run, so it **cannot accumulate** seeds, names or comments
  across invocations — every session re-imports and re-analyzes (about 20 minutes for this 25 MB
  `.text`). Our project at `refs/ghidra/proj/OpenSporeOracle` retains them, which is what makes the
  seeding above cheap.
* Ghidra 12 **removed embedded Jython**, so REA's own docs are right that a `-postScript *.py` does
  not work; ours runs under real CPython + JPype (`refs/ghidra/venv`, installed from the wheels inside
  the Ghidra distribution).

Two settings that matter for a target this size, both from REA's docs and both verified here:
`REA_GHIDRA_STARTUP_TIMEOUT_MS` must be raised well above the 330,000 ms default (the client's own
per-call timeout is a *separate* deadline and can fire first), and `GHIDRA_HEADLESS_MAXMEM` defaults to
`2G`.

## Corrections applied to the docs in this pass

| file | correction |
|---|---|
| `analysis/startup.md` | the main-loop call site is `0xf4840b`, not `0xf48405`; window vtable slot 0 is the deleting destructor |
| `analysis/startup.md` | `.data 0x153f858..` is a table of **pointers** into `.rdata`, not of strings |
| `MEMORY.md` | the FNV seed `0x811c9dc5` is a **parameter** pushed by the caller at `0x7e40f2`, not a constant inside the hasher |
| `MEMORY.md` | `EventLoadScreenBufferPercentage` is at `0x0148227c`; `0x01550498` is `EAReportUploader` |
| `MEMORY.md` | `UIWorld_Loading` is stored **UTF-16** at `0x01418274` |
| `analysis/text-locale.md` | the `locale` type id is not a hash of its name (it never was) |
