# The reverse-engineering toolchain (session 13)

> Environment/installation notes below are a retained session snapshot, not a request to reinstall
> or reconfigure anything. Discover the currently available REA tool schemas, reuse the persistent
> GA project, and diagnose only observed blockers. See [current hand-off](../RESUME.md).

Two instruments over the same Ghidra 12.1.4 engine, deliberately kept side by side.

## Primary: REA (`rea` CLI + its MCP server)

Installed globally (`npm install --global rea-agents`, v6.x) and registered as an MCP server for
OpenCode in that user's global `opencode.jsonc`/`opencode.json` (`rea setup --client opencode`).
Readiness:
`rea doctor --provider ghidra --json` → `healthy: true` (Ghidra 12.1.x, native `decompile.exe` for
`win_x86_64`, JDK 21 64-bit, Windows native Job Object / DACL authority verified).

**Why primary:** it is the maintained workflow, and its evidence contract is stricter than anything I
would write myself. Two of its rules directly govern claims in this project:

* `analyze_function` reports **`facet_availability`** per facet (`pseudocode`, `assembly`, `callers`,
  `callees`, `references`, `cfg`, …), each `{available: true}` or `{available: false, reason}`.
* **"An empty call list with partial or unknown coverage is not proof that the function has no
  calls."** Our `direct_callers == 0` finding on the registry dispatcher `0x6ac3e7` is exactly this
  shape of claim. Under REA it is stated as an observation with coverage, not as a proof.
* `list_unknowns` / `record_unknown` / `verify_unknown_resolution` track what is unresolved **explicitly**
  — which is the discipline our `VERIFY.md` rows approximate by hand.
* Target content is marked untrusted (`_meta.provenance.untrusted_content`), because strings and
  decompiled text come from the analyzed binary and can contain text written to look like
  instructions. Our data is EA's shipped game, so this is a formality here, but it is the right default.

### The settings this target needs

```powershell
$env:GHIDRA_INSTALL_DIR = "<path to an extracted Ghidra 12.1.x>"   # e.g. ...\ghidra_12.1.4_PUBLIC
$env:JAVA_HOME          = "<path to a JDK 21+>"                    # e.g. ...\jdk-21.0.x
$env:REA_ANALYSIS_PROVIDER   = "ghidra"
$env:REA_GHIDRA_STARTUP_TIMEOUT_MS = "5400000"   # default 330,000 ms CANNOT hold: see below
$env:GHIDRA_HEADLESS_MAXMEM             = "4G"                     # default is 2G
$env:REA_GHIDRA_SEED_FILE      = "<repo>\analysis\audit\seeds_rea.tsv"   # absolute path
```

All six are normally placed in the MCP registration's `environment` block (or the shell), so a
restarted client inherits them; two of them (`GHIDRA_INSTALL_DIR`, `JAVA_HOME`) are host paths, the
rest are REA settings.

**`REA_GHIDRA_STARTUP_TIMEOUT_MS` is not optional for this binary.** Our own headless import of the
25 MB `.text` took **1,197 s**; REA's default deadline is 330 s. The first CLI run died with
`failure_kind: "process"`, `failure_message: "Ghidra exited before bridge startup"` — and the MCP
client's own per-call timeout is a **separate** deadline that can fire even earlier, so raising only
one of the two does not help.

**`REA_GHIDRA_SEED_FILE` is the setting that unlocks this project.** `analysis/audit/seeds_rea.tsv`
seeds the seven functions Ghidra's auto-analysis never carves (see below). Per REA's docs a seed that
overlaps an instruction Ghidra already decoded **fails**, so the file lists only the uncarved seven,
not all 21 candidates — `tools/gen_seeds.py` prints both sets and why they differ.

### Workflow

```powershell
# once: import + analyze + retain a snapshot of exact query results
rea analyze <oracle> --provider ghidra --snapshot refs\ghidra\snap\oracle.json --json

# then, per question
rea decompile <oracle> 0xf47e90 --provider ghidra --json
rea function  <oracle> 0x8482f0 --provider ghidra --json
rea xrefs     <oracle> 0x6ac3e7 --provider ghidra --json
rea trace     <oracle> "uilayout" --provider ghidra --json
```

An exact CLI cache hit is read **before** the provider process starts, so repeat queries on a
snapshotted result are cheap. Note what a snapshot is **not**: it is a cache of eligible exact
observations, **not a saved provider database** — new or live queries still start the engine.

## Fallback: our PyGhidra bridge

`tools/ghidra_audit.py` (request/verify) + `tools/ghidra_export.py` + `tools/ghidra_seed.py` +
`tools/gen_seeds.py`, running from `refs/ghidra/venv` (CPython 3.11 + JPype, installed from the wheels
inside the Ghidra distribution). It audits against the **persistent** project
`refs/ghidra/proj/OpenSporeOracle`.

Kept for two reasons, both measured rather than assumed:

1. **REA cannot accumulate state.** Every REA session is an ephemeral
   `-readOnly -deleteProject` import, so seeds, names and comments are lost on close and each session
   pays the ~20-minute re-analysis. Our project retains them, which is what makes iterating on the
   seeding question cheap instead of expensive.
2. **Batch audit vs. investigation.** `analysis/claims.json` → one pass → `analysis/audit.md` is an
   audit of 104 checks at once. REA's tools answer one question at a time, which is the right shape
   for investigation and the wrong shape for "check everything we have ever claimed".

Ghidra 12 **removed embedded Jython**, so `-postScript *.py` fails outright with "Ghidra was not
started with PyGhidra. Python is not available". That is why ours is a CPython driver and not a
GhidraScript.

## The finding that decided the seeding

Seven addresses our claims cite had **no function at all** in Ghidra's 54,611-function inventory:

```
0x007e3fb0  statemgr add-state (vtable +0x00)     0x00849bf0  Window::Create
0x007e7160  app event dispatcher                  0x007c7f50  config: setVariables executor
0x007eaae0  AppMode +0x20 shader/script config     0x007c90f0  config: card/vendor executor
0x007e40f2  (mid-function: the call site that pushes the FNV seed)
```

`tools/gen_seeds.py` walks each back to the preceding `int3` (0xCC) gap and proves six are **real
function entries**. They were never reached because they are callable **only through a vtable**, and
six of the seven open with the MSVC SEH prologue `mov eax,fs:0x0`, which Ghidra left typed as data —
at `0x7e7160` it produced the datum `?? 64h`. The objdump listing of the same bytes:

```
7e7160  mov eax,fs:0x0            ; SEH prologue - Ghidra stopped here
7e7166  push 0xffffffff
7e7168  push 0x1217388            ; the SEH handler
7e717c  cmp eax,0x255abf5         <-- all three hashed event ids our docs cite,
7e718d  cmp eax,0xf62add               in plain sight, inside the function
7e7194  cmp eax,0x212d3e7                Ghidra refused to disassemble
```

**This reframes the rw4 / spui / prop blockers.** The recorded finding ("the model manager's handler
slots are map operations, follow the stream object") was *confirmed* by the audit — the
`direct_callers == 0` check PASSED outright. But the deeper reason we kept failing to find the parser
is now named: **in this binary the interesting functions are vtable-only, so neither auto-analysis nor
a text search for `call` will find them.** Seeding is the fix, in both toolchains.

## Two mistakes of mine, recorded because they are the transferable part

1. **I killed REA's working JVM.** A `java` process (pid 20092) was alive while our own seed run had
   crashed; I assumed it was a leftover holding our project lock and stopped it. It was the REA import,
   15 minutes in. `rea` then reported `Ghidra exited before bridge startup`, `exit_code 0xFFFFFFFF`.
   The lock file I removed was genuinely stale (its owner process no longer existed), so the fix was
   right — the diagnosis "this other java is the culprit" was not. **Identify a process by its command
   line before killing it**; `Get-CimInstance Win32_Process -Filter "Name='java.exe'"` and match on the
   project path, not the process name.
2. **I hand-rolled what REA already provides.** I wrote `tools/ghidra_seed.py` against the raw
   PyGhidra API and hit three signature walls (`createFunction(Address,String)` has no `SourceType`
   overload, `save(TaskMonitor)` takes no comment, `GhidraProgramUtilities.analyzeProgram` does not
   exist, and `analyzeAll` needs an enclosing transaction or every analyzer worker dies with
   `db.NoTransactionException`). REA exposes the same capability as `REA_GHIDRA_SEED_FILE`, documented,
   supported, and already correct. **Check whether the tool has a first-class option before
   implementing the feature underneath it.**

Both are the same class as the nine checker bugs in `analysis/audit-review.md`: a confident wrong
answer built on an unverified assumption about a tool's surface.
