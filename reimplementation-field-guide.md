# Reimplementing a shipped game from a binary — a field guide

> Distilled from a multi-session effort to take an existing "reverse-engineering + reimplementation"
> codebase (decompiled game logic, hooks into the original binary) and turn it into a **standalone
> executable that no longer needs the original game at all**, swapping its render middleware for an
> open reimplementation.
>
> Deliberately **generic**: no product names. Everything here is either (a) method, (b) tooling, or
> (c) "if you identify engine X, do Y". Use it as a briefing document for yourself *and* as the
> system/context prompt for an AI assistant doing the work.

---

## 0. The one-line strategy

**Keep the original binary as the oracle, and make every claim in your code falsifiable against it.**
The moment you have two sources of truth (your reimplementation, and "what the community says"), you
need a tie-breaker. That tie-breaker is the disassembly of the shipped executable plus its debug/PDB
symbols if you can get them. Everything else — wiki pages, other people's reimplementations, mobile
ports, leaked headers — is a *hint*, not ground truth.

Order of authority, memorise this:

1. **Disassembly of the original binary** (+ PDB/symbol data, + its `.data` contents).
2. **The original's own shipped data files** (they tell you what a parser must accept).
3. **Other reimplementations of the same engine/game** (great for *approach*, dangerous for *values*).
4. **Community wikis / SDK headers / plugin-SDK-style declarations** (good for names and types, often
   wrong on offsets and field meanings).
5. **Your own reasoning.** Last.

A surprising number of "obvious" bugs are #4 being subtly wrong. In this project the shipped headers
declared a field `int32` where the original stored a `float`, and an array as 12 entries where the
binary's own loop ran 13 times. Both were invisible until diffed against #1.

---

## 1. Making the work state survive context loss

You *will* run out of context. Plan for it from day one. Keep a small set of docs, updated **in the
same commit** as the code they describe, each with one job:

| File | Job | Test: if deleted, what hurts? |
|---|---|---|
| `PROGRESS.md` | One table: the boot/load chain, each step ✅/🚧//❌ | "where are we again?" |
| `MEMORY.md` | Durable facts: struct layouts, tool paths, traps, decisions | "why did that break?" |
| `VERIFY.md` | **Feature × address × disasm-status** matrix + ordered next actions | "what have we actually checked?" |
| `IDEAS.md` | Deferred work, stubs, bug *classes*, and things **explicitly disproven** | "don't redo that" |
| `HISTORY.md` | Session log: what changed **and how it was verified** | "why is this here?" |
| `RESUME.md` | Hand-off snapshot: TL;DR, current state, blockers, exact commands | "start here" |

Rules that saved this project repeatedly:

- **Rewrite `RESUME.md` before compaction, not after.** It's the only doc an AI will actually read
  first. Keep it under ~120 lines.
- Record **disproven hypotheses** explicitly, with "⚠️ don't repeat these". Without that, a future
  session re-runs the same week of dead ends. This is the single highest-value line in `IDEAS.md`.
- Every commit message ends with **how it was verified** ("built clean; ran; marker count 44 → 75").
  If you can't say, the commit is a hypothesis, not progress — label it as one.
- Maintain a **cheap numeric yardstick** for "are we further along". Best example from this project:
  count the loading markers in the log (`Loading the Game -> Loading <file>`). Progress became
  `1 → 44 → 75 → 77 → 123 → 175` — unambiguous, monotonic, machine-checkable, and it survives any
  amount of lost context. Pick one early.

---

## 2. Toolchain that worked

Adjust names to your platform; the *shape* is what matters.

### Build
- **CMake driven by Conan** (package manager) generating **Visual Studio 2022** solutions, MSVC
  toolset 194, targeting x86 (32-bit) because the original binary is 32-bit.
- Two configurations from one source tree, switched by a cache option (here: `standalone=True`),
  with **every behavioural difference behind `#ifdef`**. The mod/plugin build must keep working —
  it's your regression test that you haven't broken the shared logic.
- Wrapper quirks to write down immediately. Ours: the build wrapper prints `Exited with code 1`
  **even on success**; the real check is "a `-> ...exe` line exists and there are no `error C/LNK`".
  Also: always use Conan's cmake (a newer pip-installed cmake re-configuring the same build dir
  breaks compiler detection).
- Keep third-party sources **outside** any directory your build globs. We learned this the hard way:
  vendored a library inside `source/`, the glob pulled its `.hlsl` shaders into our target, and the
  shader compiler failed the build.

### Reading the binary
- **`objdump` (MinGW build) for disassembly**, always Intel syntax, always ranged:
  ```
  objdump.exe -d -M intel --start-address=0xADDR --stop-address=0xADDR2 refs/<game>.exe
  ```
  Cheap, scriptable, no license, no GUI to fight. Range it — a full dump of a large exe is
  hundreds of MB and you will regret it.
- **`pefile` (Python) for anything in `.data`**: strings, tables, struct contents, floats. This is
  how we recovered "what does this data table actually contain" without guessing:
  ```python
  import pefile, struct
  pe = pefile.PE(path, fast_load=True)
  BASE = pe.OPTIONAL_HEADER.ImageBase          # usually 0x400000
  def file_off(va):                            # VA -> RVA -> section -> file offset
      rva = va - BASE
      for s in pe.sections:
          if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
              return s.PointerToRawData + (rva - s.VirtualAddress)
  ```
  Two traps, both of which cost time here: (a) pefile stores **RVA**, not VA — subtract the image
  base first; (b) section headers truncate names, so `str(s.name).rstrip('\x00')` is the portable
  idiom, and `Misc_VirtualSize` can be smaller than the raw data.
- Verify a function's **size** by finding its first `ret`, and its **data dependencies** by grepping
  the disassembly for `mov ... ,ds:0x...` — every absolute address it touches is a global you must
  account for. To find *who references* a global, byte-search `.text` for the little-endian address
  (we did this to prove a table was only ever read by two functions).

### Running & observing
- Run the rebuilt exe **as its own process**, cwd set to the directory holding the game assets.
- **Automated input**: `PostMessage(WM_KEYDOWN)` does *not* work if the app polls input (SDL/DirectInput).
  Use `SendInput`/`keybd_event`. And mind `cbSize`: `sizeof(INPUT)` on x86 is **28** (the union is
  sized by `MOUSEINPUT`); get the managed struct wrong and `SendInput` silently returns 0.
- **Screenshotting a window you don't own**: `PrintWindow(hwnd, hdc, PW_RENDERFULLCONTENT)`. Trap:
  that captures the **whole window including the title bar**, while `ClientToScreen` speaks in
  **client** coordinates. Mixing the two put our synthetic clicks ~31 px off, which produced hours of
  "clicking doesn't work". Use `PW_CLIENTONLY | PW_RENDERFULLCONTENT` and coordinates stay honest.
- **Never let a modal dialog block you.** See §4 — this is a first-class requirement, not a nicety.
- Make log files **share-readable** (`_fsopen(..., "a", _SH_DENYNO)`) so you can tail them while the
  process is alive. `Get-Content` will fail on an exclusively-locked file; open with
  `[IO.FileShare]::ReadWrite` instead.

---

## 3. The RenderWare playbook

If you identify **RenderWare** (signatures: `Rw*`/`Rp*`/`Rs*`/`Rx*` symbols, "RenderWare x.y.z"
version strings, `.txd`/`.dff`/`.ifp`/`.map`/`.col` assets, chunk IDs like `rwID_CLUMP`/`rwID_ATOMIC`,
`RpWorld`/`RxPipeline` in the binary), then:

- **Use an existing open reimplementation rather than writing one.** `librw` is the practical one and
  it builds under MSVC/CMake with a D3D9 backend. **Treat your copy as a fork you own**: move it into
  the tree, document provenance + every local modification in a `FORK.md`, and edit it freely. Trying
  to keep it pristine and untouched is a false economy — you *will* need hooks in it.
- **Do not typedef librw's types onto the game's.** SA-era RW 3.7 struct layouts differ
  (`RwCamera`, `RwFrame`, `RwRaster`, `RwImage`, `RpAtomic`, `RpMaterial`, …). Instead:
  1. Put **exactly one translation unit** that includes `<rw.h>` (the "bridge"). Its header exposes
     only POD signatures (`void*`, `int`, `float`), because the game's RW headers and librw's headers
     define colliding macros/enums and cannot coexist in one TU.
  2. For each RW function the game calls, give the game's own thunk an `#ifdef STANDALONE` body that
     calls the bridge; the non-standalone branch keeps calling the original binary.
  3. For each **field-touching macro** in the game's RW headers, `#undef` and re-point it at the
     bridge in standalone. This is the boring, mechanical, essential part.
  4. Keep a **layout table** (SA type ↔ librw type ↔ verdict: identical / prefix-identical / differs)
     in your memory doc. Re-check it before every new subsystem. Some things *are* compatible:
     `RwV3d`, `RwMatrix`, `RwSphere`, `RpMorphTarget`, and `RpHAnimHierarchy`'s first 0x20 bytes.
     Assume nothing else.
  5. Watch for accessors used as **lvalues**. The game does `RpGeometryGetFlags(g) &= mask`, so the
     bridged version must return `unsigned&`, not a value.
- **Render states: translate the ID *and the value*.** The state enums mostly agree between RW and
  librw (blend, stencil, cull, filter, addressing, primitive type) — but check **every** one, because
  a mismatch is silent. Here, `AlphaTestFunc` was RW/D3D-numbered 1..8 vs librw's 0..2, so a value of
  5 indexed a 3-element map **out of bounds**, produced a garbage compare func, and made *every
  textured quad with alpha invisible*. That single OOB read cost days. Rules:
  - Diff the enums, don't trust that "order matches".
  - Add a defensive clamp/assert on the librw side so an out-of-range value can never again be
    silently absorbed. Cheap insurance, and it converts a mystery into a message.
- **Object plugins are compatible.** librw's `PluginList::registerPlugin` returns the in-object byte
  offset and `RWPLUGINOFFSET(type, obj, off)` means the same thing on both sides — so the game's own
  RW plugins (visibility, TXD-parent, collision, 2d-effect, breakable, node-name, pipeline, anim-blend)
  can be attached unchanged, **as long as you bridge the registration functions**
  (`Rw*RegisterPlugin`, `*RegisterPluginStream`, `*GetPluginOffset`, `*SetStream{Always,Rights}CallBack`)
  for every class that has `PLUGINBASE`: Frame, Raster, Texture, TexDictionary, Material, Geometry,
  Atomic, Clump, Light, Camera, World. Forget none, and note which classes genuinely have no
  counterpart (e.g. `RpWorldSector`).
- **Streaming**: librw has `Clump::streamRead`/`Atomic`/`Geometry`/`Material`/`MaterialList`/`Light`/
  `Camera`/`FrameList` — i.e. the whole DFF reader. Bridging `RpClumpStreamRead` to it is what turns
  "cannot load models" into "loads models". Also bridge the *image* readers (`readBMP`/`readPNG`) and
  the memory-stream initialiser used by streaming engines — but see §5 about "initialise the caller's
  buffer" semantics: RW's `_rwStreamInitialize` re-uses one global buffer, so back it with **one
  cached** librw stream and make your close-not-delete it (the game closes the same stream twice).
- **Immediate mode (im2d)** works, but it is where you'll blame things wrongly. Before suspecting the
  renderer, dump the actual D3D state at the draw (see §4).
- **RxPipeline** is the trap. RW 3.7's Rx/Pipeline system has **no** librw equivalent (librw has a
  much simpler pipeline model). Anything that depends on it — custom building renderers, car env-map /
  dirt FX — must be **stubbed with the exact guard conditions checked**: find the boolean the game
  uses to decide "is this a custom pipeline" (`IsCBPCPipelineAttached`-style) and return the honest
  `false` *because it is now unreachable*, and prove the consumers all guard on it. Write that proof
  in the comment; it's the difference between a stub and a landmine.
- **`RpSkin`/`RpHAnimHierarchy` are opaque** in the RW headers ("use the API functions") — so if the
  game really only uses the accessors, you can substitute librw's structs wholesale. Verify by
  grepping for `->field` on those types; if there *are* direct field reads, check the prefix layout
  byte-by-byte against disasm before relying on it.

### Engine NOT RenderWare
- **id Tech / Build / bespoke**: the same method applies, but the "swap the middleware" step is
  usually replaced by "port the renderer to a modern backend". Find the *seam* the original already
  has (a per-platform renderer directory, an abstract `Draw*` interface) and bridge there.
- **CriterionWare / RenderWare-derivative consoles**: expect the plugin/chunk model to be similar;
  the data files are your oracle.

---

## 4. Instrument before you experiment

This is the lesson that mattered most, and it's the one AI assistants (and humans) reliably skip.

**What happened:** text was invisible. Every texture with an alpha channel drew nothing; every opaque
one drew fine. Days went into A/B rendering experiments — swap formats, bake alpha, force render
states, write a whole alternative TrueType renderer. All of it failed, because each experiment
changed two things at once and the screenshots were ambiguous.

**What actually worked:** re-reading a D3D state log that had *already been captured*, which said
`ALPHATESTENABLE=1, FUNC=0x2, REF=2`. `0x2` is `D3DCMP_LESS`, so the alpha test was "pass if alpha < 2"
— discarding opaque glyph pixels and keeping transparent ones. Combined with a per-string colour log
showing `#ACCBF1**00**` (right RGB, **alpha 0**), two root causes fell out immediately, both of them
ours, neither in the renderer.

**Therefore, build the observation infrastructure first, and make it unattended-proof:**

1. **Assertions must not be fatal and must not block.** Three separate mechanisms:
   - `assert()` in the UCRT ends in `abort()`; the modal Abort/Retry/Ignore dialog's *Ignore* is the
     only thing that returns. So **suppressing the dialog without neutralising the assert turns every
     failed assert into a silent process death** — no exception record, no stack, log just stops. If
     you suppress the dialog, also make the assert non-fatal (log + continue), i.e. click "Ignore"
     automatically.
   - The game's own "unreachable" helper may pop **its own** modal box (not a CRT report), so a CRT
     report hook won't silence it. Handle it separately: log + `exit(distinct_code)`. Keep it
     `[[noreturn]]` — callers rely on that (many end their last branch with it and have no `return`).
   - Route all CRT debug reports to a file (`_CrtSetReportHook` + `_CrtSetReportMode` without
     `MODE_WNDW`). Keep the hook **plain C stdio under a critical section**: it runs inside the CRT's
     own report path, so re-entering your logger or allocator risks recursion/deadlock.
2. **Dedupe and cap.** Per call-site counters (say 128 sites, 25 reports per site, 4000 total) so an
   assert storm fills the disk but you still see the *first* occurrence with its exact expression and
   `file:line`. Counting continues past the cap; reporting doesn't.
3. **Log the state, not the symptom.** Colour, format, dimensions, pointer values, counts — at the
   point of use. "Nothing appeared" is not data; `col=#ACCBF100` is.
4. **Consider the D3D9 debug runtime** (`D3DCREATE_DEBUG` / the SDK layers) when the GPU is lying to
   you. It reports exactly why a draw produced nothing. Cheaper than 20 experiments.
5. **One variable at a time, and never a one-shot `static` latch in a diagnostic.** A latch that
   fires once makes every later screenshot ambiguous about which build/branch it came from. We hit
   this repeatedly.

**Also: the "shadowed drafted body" gold mine.** A common upstream pattern is to *write* the
reversed body but leave `return plugin::Call<addr>(...)` at the top so the mod build still calls the
original — making the body dead code. Scan for it mechanically (≥3 lines of real code after a
`plugin::Call` inside the same function) and you get a list of free reversals. Here that found 61
sites, the largest 617 lines. Enabling one at a time, as the load path reaches it, is the fastest
forward motion available. **But label them**: a shadowed body is a *draft*, unverified by definition,
so give each a "needs a disasm pass" marker that feeds your `VERIFY.md`.

Two more bug classes worth a standing scan:
- **`std::format` throws where `sprintf` didn't.** Any `std::format("{}", ptr)` with a possibly-null
  `char*` is a crash (unhandled `std::format_error`). The original forwarded the pointer to
  `AsciiToGxtChar` and nothing happened. Grep for `std::format` around pointers/array elements,
  especially where the data comes from a table that legitimately contains holes.
- **A dereferenced value masquerading as a pointer**: `*reinterpret_cast<Enum>(intValue)` — instant
  access violation at address `intValue`. It survives in dead code indefinitely.

---

## 5. The static-globals cliff (the #1 source of mystery bugs)

A reimplementation that *replaces* the original binary loses something invisible: **the original's CRT
used to construct its global objects and lay out its `.data`.** If your globals live at fixed
addresses backed by fresh zero-filled storage, you inherit two failure modes.

**(a) Objects with constructors / vtables are never constructed.**
In the injected (plugin/ASI) build the original exe has already run its static initialisers, so every
global is properly built. In a standalone build you get zeros. Symptoms, in increasing nastiness:
- POD-looking globals read as 0 → wrong colours, sizes, counters. (Here: a HUD colour table of all
  zeros → drop-colour alpha 0 → **all text invisible**.)
- **Polymorphic** globals have a null vtable pointer, so the first *virtual* call jumps to ~0x18 and
  faults. (Here: model-info stores; `mi.Init()` is virtual.)

Fix: an explicit `InitGameGlobalStatics()` that placement-constructs each one before the game loop,
via a tiny helper:

```cpp
template<typename T> T& ConstructStatic(T& g) { return *::new (std::addressof(g)) T(); }
```

**Enumerate the candidates mechanically**, don't guess: scan every `StaticRef<T>` site, group by `T`,
and flag types that have a `virtual` or a user-declared constructor. Expect a few dozen. Here: 2352
sites, 641 distinct types, **39** polymorphic-or-ctor.

**(b) "Tabular statics": data tables baked into `.data` that nothing ever initialises at runtime.**
These are the worst, because they fail *silently and plausibly*. Examples from here:
- A 210-entry vehicle-name table. Zero-filled, the lookup used `strncmp(name, entry, strlen(entry))`
  and `strlen("")==0`, so **every name matched slot 0** — and the loader cheerfully reported "loaded
  259 handlings for 210 vehicles" while all 259 landed in one slot. Meanwhile the *other* lookup,
  using `strcmp`, matched nothing and died.
- A 118-entry animation-group table. The original's `Initialise` sets the count to 118 and only
  *appends* extras from a config file — so the 118 are pure static data. Every lookup failed.

Recovery recipe that worked, and it generalises: **if the table is derivable from a config/data file
the game already parses, rebuild it while parsing** — intern each name on first sight — then
**verify the resulting order against the original binary's `.data`** (read it with `pefile`, compare
element by element). That reproduced the 210-name table exactly, including which entries are bikes and
which are boats, and that the trailing 49 config lines are *redefinitions* reusing an existing id.
For tables you can't derive, an honest, loudly-logged fallback (`return DEFAULT;` + one WARN) keeps
the game bootable while you reverse the real thing. Say plainly what is degraded.

**(c) Your static storage is exactly `sizeof(T)`; the original's was a contiguous `.data`.**
So code that *deliberately* reads past the end of an array — harmless in the original, because it
landed in the neighbouring global and the value was never used — now runs off the end of your
allocation and faults. We hit this with a model-info table read using resource IDs 25000+ before the
caller narrowed the range. Fix by clamping to a sentinel (`nullptr`) **in the standalone build only**,
and leave the mod build byte-identical. Then assume there are more, and grep for any indexed access
whose index can be a resource/ID-space value.

---

## 6. Bridging a subsystem: the repeatable loop

For each wall the load path hits, the same 8 steps, in this order:

1. **Get the exact stop.** Crash stack from your exception handler, or the exit code + last log lines
   for an assert/unreachable. Never guess from a screenshot.
2. **Disassemble the function.** Range it. Note: size, every absolute global it touches, every call
   it makes, every string/format it uses. Read the format strings out of `.data` — they tell you the
   parser's contract.
3. **Check what the data file actually contains** (the original's own assets are the second oracle).
   A parser's expected shape is visible in 10 lines of its input.
4. **Look for an existing implementation** in your replacement library, or a sibling reimplementation
   of the same engine. Steal the *approach*, never the *numbers*.
5. **Decide: bridge, reverse, or stub.** Stub only when you can *prove* it's inert, and write the
   proof inline (who reads this output? are they all guarded? what's visibly degraded?).
6. **Implement, guarded by the standalone macro**, mod build untouched.
7. **Verify by running**, and record the yardstick delta.
8. **Commit with the verification in the message**, and update `VERIFY.md` (status + address) and
   `MEMORY.md`/`IDEAS.md` if you learned a *class* of thing.

Steps 2 and 3 together catch most "the drafted body is wrong" bugs. Real example: the drafted parser
read a section *header* line with a 2-token format (`"%s %s"`) when the original used `"%s"` — so the
whole file parse desynchronised by one line and produced garbage that faulted downstream. Disasm plus
10 lines of the data file made it obvious in minutes; the symptom (a crash in matrix code far away)
had made it look like a renderer bug.

---

## 7. Prompts that worked (for driving an AI on this)

Copy-adapt these. The pattern is: **constrain the authority order, forbid guessing, demand evidence.**

- *"Verify every feature against the disassembly of `<binary>` at `<address>`, not against the wiki or
  other reimplementations. Addresses are for version X; we build against version Y — match by function
  identity, not by address. Report: same / differs (how) / unverifiable (why)."*
- *"Before changing anything, read `<file>` at `<lines>` and quote the disassembly instructions you are
  basing the change on, inline in the code comment."*
- *"Do not run new rendering experiments. Read the logs we already captured and tell me what state the
  draw happened in."* (Use this whenever you feel the urge to A/B test visuals — it's the tell that
  you've stopped gathering evidence and started guessing.)
- *"This function's body is dead code after an early `plugin::Call`. Enable it for standalone only, and
  mark it UNVERIFIED in the docs — it is a draft, not a reversal."*
- *"Find every `StaticRef<T>` whose type has a vtable or a constructor. Do not fix them; list them,
  with counts, so we can prioritise."*
- *"When you stub something, answer in the comment: who reads this output, and are all of those reads
  guarded? If you can't prove it, say so instead of stubbing."*
- *"Write the disproven hypotheses down. If we already ruled out X, put it in `IDEAS.md` with a
  'don't repeat' note."*
- *"Update `RESUME.md` now, before context compaction. Checklist: RESUME, PROGRESS, HISTORY, MEMORY,
  IDEAS, git clean with verification in the messages, and nothing essential living only in chat."*
- *"Measure progress as a number I can compare across sessions."*

Anti-patterns to call out immediately, all of which occurred here:
- Changing render state / textures / formats blindly and re-screenshotting. (Changes several variables;
  screenshots are not evidence.)
- Trusting plugin-SDK-style headers for offsets or field types.
- Assuming "the mod build works, so the logic is right" — the mod build delegates the very function
  you're trying to fix back to the original binary.
- One-shot `static` latches in diagnostics.
- Treating a vendored library as untouchable when the fix is one line inside it.

---

## 8. Checklist for starting a new title cold

1. Get the **original binary + any PDB/symbols + the shipped data files** into `refs/` (gitignored).
2. Identify the **engine** from strings/symbols/assets. Then go to §3 (RenderWare) or the equivalent.
3. Decide the **middleware replacement** (renderer/audio/filesystem) and vendor it as **your fork**,
   with `FORK.md` (provenance, commit hash, license, list of local mods).
4. Stand up **two build configs from one tree** (plugin/injected vs standalone), all deltas behind one
   macro. Keep both building — the plugin build is your regression harness.
5. Build the **observation infrastructure first** (§4): non-fatal asserts, no modal dialogs anywhere,
   file-backed reports, share-readable logs, a screenshot + auto-input harness, an exception handler
   that prints a symbolised stack.
6. Create the six docs (§1) with a **marker-count yardstick** and the **authority order** written at
   the top of `VERIFY.md`.
7. Walk the **boot chain**, then the **load chain**, one wall at a time via the §6 loop. Commit per fix.
8. Every session end: rewrite `RESUME.md`, commit clean, and record what was *disproven*.

---

## 9. Things that will waste your week (each one actually did)

| Trap | Symptom | Real cause |
|---|---|---|
| Enum **values** not translated, only IDs | All alpha-textured draws invisible, opaque ones fine | OOB map index → garbage compare func → alpha test discarded opaque texels |
| Suppress assert dialog, forget it aborts | Process vanishes, log cuts off, no stack | `assert` → `_wassert` → `abort()`; "Ignore" was the only escape |
| Game's own `unreachable()` shows a modal | "C++ dialog, clicked Ignore dozens of times" | Not a CRT report — hook can't see it |
| Fixed-address global never constructed | Wrong colours; or jump to ~0x18 on a virtual call | Original's CRT used to build it; you only zero-fill |
| Zero-filled **data table** | Loader reports success, everything lands in slot 0; or "can't find X" | Table was `.data`; `strlen("")==0` matches everything |
| Exactly-sized static storage | Fault reading a table at index 25001 | Original read past the end into the next global, harmlessly |
| Dead drafted body | Parser desyncs, crash far downstream | Header line read with a 2-token format instead of 1 |
| `std::format("{}", ptr)` | Unhandled `format_error`, exit code = C++ exception | Throws on null `char*`; `sprintf` printed `(null)` |
| `*reinterpret_cast<Enum>(intValue)` | AV at address = the value | Typo in code that never executed (was shadowed) |
| `PrintWindow` vs `ClientToScreen` | Synthetic clicks land ~31 px low | Whole-window vs client-area coordinates |
| `PostMessage` for input | Keys/clicks ignored | App polls input; use `SendInput`/`keybd_event` |
| Wrong `sizeof(INPUT)` in a harness | `SendInput` returns 0, nothing arrives | Union must be padded to the largest member (24 on x86 → 28 total) |
| Bridging a "private" RW fn wrongly | Silent garbage in one field | Read the disasm: some are 2 instructions (`and [eax+0xC], ~0x20003`) |
| Close = delete, but the game closes twice | Heap corruption later | RW re-initialises one global stream; cache it, don't free it |

---

*End of guide. The recurring meta-lesson: nearly every "impossible" bug in this project was not in the
renderer, the engine, or the data — it was in the seam where the reimplementation stops inheriting what
the original binary used to do for free. Enumerate the seams (render states, globals, static storage,
streams, plugins, dead code) and audit each one explicitly.*
