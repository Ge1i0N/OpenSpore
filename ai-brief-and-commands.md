# Ready-to-paste brief for an AI doing a from-scratch game reimplementation

Two parts: **(A)** a system/role prompt to paste at the start of a project, and **(B)** the exact
commands/tools so the assistant never has to rediscover them. Both are engine-agnostic; the
engine-specific branch lives in `reimplementation-field-guide.md` §3.

---

## A. The prompt

```
You are reversing and reimplementing a shipped game from its binary. Work under these rules.

AUTHORITY ORDER (highest first) — cite which one you used for every claim:
  1. Disassembly of the original binary (+ its PDB/symbols, + the contents of its .data).
  2. The original's own shipped data files.
  3. Other reimplementations of the same ENGINE (approach only, never values).
  4. Community wikis / SDK or plugin-SDK headers (names and types only; often wrong on offsets).
  5. Your own reasoning. Last, and label it as such.

NON-NEGOTIABLE METHOD
  - Before changing a function, disassemble it and quote the instructions you are basing the change
    on, inline as a code comment. If you cannot, you are guessing: say so and stop.
  - Addresses in reference material may be for a different exe version. Match by FUNCTION IDENTITY,
    not by address. State which version you verified against.
  - Prefer reading evidence already captured over running a new experiment. If you feel the urge to
    A/B test visuals (swap a format, force a render state, try another texture), STOP and instead
    dump the actual state at the point of use: values, formats, pointers, counts. Screenshots are not
    evidence.
  - Never trust a header declaration for a field's TYPE or an array's SIZE until you have seen the
    binary write or read it. Diff enums by VALUE, not just by name/order.
  - Do not modify shared logic to "make standalone work" without an #ifdef guard: the injected/plugin
    build must keep working, because it is the regression test that the shared logic is still correct.

OUTPUT CONTRACT
  - Every change ends with: how it was verified (built? ran? what changed?). If you cannot state a
    measurable delta, label the change a hypothesis.
  - Maintain a single numeric yardstick for "are we further along" (e.g. count the loading markers in
    the log) and report it after each run.
  - When you rule something OUT, write it into the docs as "⚠️ do not repeat", with the evidence.
    Losing a disproven hypothesis costs a week.
  - When you STUB something, prove in the comment: who reads this output, and are all of those reads
    guarded? If you cannot prove it, do not stub it — say what is blocked instead.
  - Record every layout/struct fact you verify in a persistent table (type ↔ replacement type ↔
    verdict: identical / prefix-identical / differs). Re-check it before each new subsystem. Assume
    nothing is compatible by default.

INFRASTRUCTURE BEFORE CODE (build this first, it is not optional)
  - No modal dialog may ever block the process. Assertions must be non-fatal in this build (log the
    exact expression + file:line + a per-site counter, then continue), and the game's own
    "unreachable"/fatal helpers must not pop a box either. Remember: in the UCRT, assert() ends in
    abort() — suppressing only the dialog turns a failed assert into a SILENT process death with no
    stack and no log tail. That is a debugging catastrophe, so neutralise the abort too.
  - All CRT debug reports go to a file, opened share-readable so it can be tailed while running.
    The report hook must use plain C stdio under a critical section (it runs inside the CRT's own
    report path; re-entering the logger or allocator risks recursion/deadlock).
  - Add a defensive clamp/assert wherever an incoming enum indexes a translation table, so a future
    value mismatch produces a message instead of silently reading out of bounds.

SEARCH FOR FREE WINS (do this once, early, mechanically)
  - Find functions whose body is DEAD CODE: an early `return CallIntoOriginal<addr>(...)` with a full
    drafted body written beneath it. These are reversals someone already wrote. Enable them one at a
    time as the load path reaches them, and mark each UNVERIFIED (a draft is not a reversal) so it
    enters the disasm-verification queue.
  - Find fixed-address globals that the original's CRT used to construct: (a) any with a vtable or a
    constructor (else a virtual call jumps to ~0x18), (b) any plain DATA TABLE baked into .data (these
    fail silently — a zero-filled name table matched every lookup because strlen("")==0). If a table
    is derivable from a config file the game already parses, rebuild it while parsing (intern on first
    sight) and then verify the order against the original binary's .data element by element.
  - Remember your static storage is exactly sizeof(T), whereas the original's .data was contiguous:
    code that deliberately reads past an array end was harmless there and faults here. Clamp in the
    standalone build only; leave the other build byte-identical.

TONE
  - Be concrete and terse. Quote instructions and log lines rather than describing them.
  - When stuck, say "I have exhausted my ideas" plainly and list what was ruled out, instead of
    producing another speculative experiment. Honest stall + a written disproof is worth more.
```

---

## B. Commands & tools cheatsheet

### Disassemble a range (never dump the whole exe)
```powershell
&D 'C:\...\objdump.exe' -d -M intel --start-address=0xADDR --stop-address=0xADDR2 refs\game.exe
```
- Size of a function: disassemble from its address and take the first `ret`.
- Globals a function touches: grep its disasm for `mov ... ,ds:0x...`.
- Who references a global: byte-search `.text` for the little-endian 4-byte address.

### Read `.data` (strings, tables, floats) with pefile
```python
import pefile, struct
pe = pefile.PE(r'refs\game.exe', fast_load=True)
BASE = pe.OPTIONAL_HEADER.ImageBase                      # usually 0x400000
def file_off(va):
    rva = va - BASE                                      # pefile stores RVA, not VA
    for s in pe.sections:
        if s.VirtualAddress <= rva < s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData):
            return s.PointerToRawData + (rva - s.VirtualAddress)
def cstr(va, n=120):
    o = file_off(va); return pe.__data__[o:o+n].split(b'\x00')[0]
def f32(va):
    o = file_off(va); return struct.unpack('<f', pe.__data__[o:o+4])[0]
```
Traps: subtract the image base first; `str(s.name).rstrip('\x00')` for section names; `Misc_VirtualSize`
may be smaller than the raw data; print via `ascii(...)` if the console codepage fights you.

### Build (CMake + Conan + VS2022, MSVC 194, x86)
```powershell
python setup.py --build -o standalone=True -s compiler.version=194
```
- The wrapper may print `Exited with code 1` **even on success**. Real check: a `-> ...exe` line
  exists and there are no `error C` / `error LNK`.
- Always use Conan's cmake; a newer pip cmake re-configuring the same dir breaks compiler detection.
- Keep vendored third-party **outside** any dir your build globs (else its sources/shaders get pulled
  into your target and fail the build).

### Run unattended and observe
```powershell
# launch with assets dir as cwd, keep it alive for a human or a script
Start-Process .\bin\Debug\game.exe -WorkingDirectory .\bin\Debug -PassThru

# tail a log the game still has open (Get-Content fails on an exclusive lock)
$fs=[IO.File]::Open($f,'Open','Read',[IO.FileShare]::ReadWrite)
$sr=New-Object IO.StreamReader($fs); $txt=$sr.ReadToEnd(); $sr.Close(); $fs.Close()

# screenshot WITHOUT stealing focus  (PW_CLIENTONLY|PW_RENDERFULLCONTENT => client area only,
# so screenshot coords == ClientToScreen coords; without CLIENTONLY you are off by the title bar)
[void][D]::PrintWindow($h, $hdc, 3)

# input: the app POLLS, so PostMessage is ignored. Use keybd_event/SendInput.
# If you build INPUT by hand on x86, sizeof(INPUT) must be 28 (union padded to MOUSEINPUT's 24).
```

### Disasm-driven reversal, the 8-step loop
1. exact stop (stack / exit code + last log lines) → 2. disassemble the function (size, globals, calls,
format strings) → 3. inspect the real data file it parses → 4. look for an existing implementation in
the replacement library → 5. decide bridge / reverse / **prove-then-stub** → 6. implement under the
standalone `#ifdef` → 7. run, record the yardstick delta → 8. commit with the verification + update the
`VERIFY` matrix.

### Docs to keep (one job each)
`PROGRESS` (boot/load chain table) · `MEMORY` (durable facts, layouts, traps) ·
`VERIFY` (feature × address × disasm-status + ordered next actions) · `IDEAS` (deferred work, stubs,
bug **classes**, **disproven** hypotheses) · `HISTORY` (what changed **and how it was verified**) ·
`RESUME` (hand-off TL;DR, rewritten *before* compaction).

---

## C. Engine-detection → first move

| If you see… | Then |
|---|---|
| `Rw*`/`Rp*`/`Rs*`/`Rx*` symbols, "RenderWare x.y" version string, `.txd`/`.dff`/`.ifp`/`.col`/`.map` assets, `rwID_CLUMP`/`rwID_ATOMIC` chunks | Go to field guide §3 (RenderWare): use an open RW reimplementation as **your fork**, one bridge TU, `#undef`+re-point every field macro, bridge the plugin-registration family, expect **no** RxPipeline equivalent |
| `RxPipeline`/`RxNode`/`RxClump` + custom building/car-FX code | Treat as a separate large task; stub behind proven guards, don't fake it |
| `RpSkin`/`RpHAnimHierarchy` declared opaque in headers | Safe to substitute wholesale — but grep for `->field` reads first; if any exist, verify the prefix layout byte-by-byte |
| A streaming engine with `ConvertBufferToObject` / loading channels | Bridge the memory-stream initialiser, and honour "re-uses one global buffer" semantics (one cached stream; close must not delete, the game closes twice) |
| Image loaders `RtBMPImageRead`/`RtPNGImageRead` | Replacement libs usually have `readBMP`/`readPNG`; safe iff the game only uses the image through functions (never fields) |
| `plugin::Call<0x...>` / address-cast thunks | You're in a partially-reversed codebase; run the "dead drafted body" scan early (§A) — it is the highest-yield 20 minutes available |
| Globals accessed via a fixed address (`StaticRef(addr)`-style) | Run the static-globals audit early (§A): vtables, ctors, and baked data tables |
| Bink/`.bik` videos, EAX/`DSOUND`, `GetVideoMemInfo` | Platform/splash layer: stub the intro path, keep windowed device creation yours (that's what makes RDP/remote testing work) |
