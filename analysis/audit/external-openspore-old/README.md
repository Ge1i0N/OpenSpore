# `openspore-old` reference pass

Reviewed **2026-10-18**. This is a read-only review of
[`OpenSporeLabs/openspore-old`](https://github.com/OpenSporeLabs/openspore-old) at:

```text
970236558184bd88cd5c356c39b5b57fb5636afd
```

The local reference is the user-provided ZIP extraction at
`refs/thirdparty/openspore-old-main`; it has no `.git` directory. Its generated `build-clang` tree
contains Windows-hostile long paths, so a complete Git checkout is intentionally not required.
Seven key files (`README.md`, `AGENTS.md`, the workflow, dossier schema, semantic bridge, roadmap and
handoff) were normalized for line endings and matched the pinned revision byte-for-byte through the
temporary metadata clone. The extracted tree is therefore accepted as the local representation of
that revision.

## What this repository actually contributes

### 1. A stronger evidence workflow

`docs/RE-WORKFLOW.md` describes a seven-stage loop:

```text
target/dossier
  -> static Ghidra evidence
  -> runtime probe
  -> asset correlation
  -> contract + frozen fixtures
  -> replacement implementation
  -> recorded status and gate
```

The useful part for our project is not the exact tool names. It is the requirement that every stage
retains a bounded artifact and an evidence grade. In particular:

- decompiler output is **evidence, not truth**;
- a missing runtime trace is recorded as `runtime: NOT OBTAINED`, not as success;
- asset presence is separated from semantic identity and runtime use;
- differential tests against a decompilation reference do not promote a replacement to live-oracle
  verified;
- `UNKNOWN`, `APPROXIMATION`, `INFERRED`, `SUPPORTED`, `OBSERVED`, `CONFIRMED` and `VERIFIED` are
  deliberately different claims.

This maps cleanly onto our `analysis/audit`, `VERIFY.md`, `Unknown` statuses and GA-unpacked
authority rule. No code copy is needed.

### 2. A useful dossier schema

`docs/RE-DOSSIER-SCHEMA.md` treats one behavior/function cluster as the unit of work. A dossier
contains functions, callers/callees, constants, runtime observations, resource references, asset
relationships, hypotheses, known unknowns, the OpenSpore counterpart and the promotion gate.

The highest-value idea is the explicit runtime section:

```text
obtained: false
reason: <why no trace exists>
```

This prevents static Ghidra results from being accidentally reported as runtime behavior. Our
existing ScenarioMode/GGE audits already follow this rule; future broad dossiers should use the
same shape rather than inventing a second readiness vocabulary.

### 3. Semantic bridge / knowledge graph shape

The semantic bridge uses address-keyed function passports with separate facts for identity, ABI,
vtable, graph, globals, types, semantics, reconstruction and evidence. A missing fact is typed
`Unknown`; the bridge does not infer or validate it on behalf of the consumer.

This is a good design lead for a future local index of GA evidence, especially for:

- one function → many independently graded facts;
- target hash and address as mandatory identity;
- coverage/limitations attached to each query result;
- optional snapshots that do not become runtime dependencies.

It is **not** a reason to import the external knowledge graph wholesale: the GA address oracle and
our current audit artifacts remain authoritative, and a second address database would need an
explicit provenance/identity bridge first.

### 4. Cell-stage prioritization

The roadmap ranks Cell, asset pipeline, App core, animation, UTFWin and Terrain as high-leverage
systems. Its Cell slice is a useful product lead because it joins real package data, a deterministic
simulation contract, a renderer and a stage driver.

For our GA path, only the following are actionable leads:

- identify the GA CellGame package/resource families when those packages are available locally;
- compare `populate`, `structure`, `cell`, `world`, `look_table`, `prop`, `backgroundMap`, `advect`
  and `density` layouts against our exact shipped data;
- freeze a small data contract before adding stage behavior;
- keep positions, balance, AI, camera and tuning values separate unless GA evidence supports them.

The fork's Cell runtime is not evidence that those gameplay values are native GA behavior.

## Semantic passport identity check

The extracted tree contains `knowledge/semantic/function-passport-v1.jsonl` (about 98 MB,
58,757 function records). Its metadata identifies the analyzed input as:

```text
program:       SporeApp.exe
version:       3.1.0.22
architecture:  x86:LE:32
image_base:    0x00400000
binary_sha256: 25d42a7a5c4d438fb155233230f57d29e2849bfdff5c889a5d0847f0469d914e
```

Our selected GA-unpacked authority is a different byte identity:

```text
refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe
binary_sha256: 086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7
```

The passport must therefore **not** be joined to our GA facts by VA alone. For example, the
passport contains a named `Simulator::cStarManager::Get` row at `0x00b3d3a0`, but that is only a
semantic lead until the same address/body is corroborated against our selected GA binary. The
passport's ABI, vtable, call graph, globals, promotion and reconstruction fields remain facts about
its own binary identity.

This is exactly why the external exchange specifies `binary_sha256 + canonical VA` as its join key.
The safe use here is:

```text
passport with mismatched SHA -> candidate name/term/probe lead
our GA bytes + REA/Ghidra     -> address/body/ABI/vtable decision
```

No passport record was imported into `analysis/audit/`, `source/` or the runtime.

## Semantic leads for our current Unknowns

The extracted research corpus is still useful when read as a question generator. These are the
highest-value leads found during the bounded search; none imports the external VA.

### Scenario and game-mode axes

The external conflict-adjudication and gameplay-transition notes explicitly keep these separate:

```text
App::cGameModeManager active index
Simulator::cGameModeManager / strategy commit
SpaceContext
App::cScenarioMode::mMode
cGameInputManager current mode
```

That is a useful corroborating design constraint for our ScenarioMode work. It supports keeping
the selected-star → ScenarioMode handoff as an unresolved boundary instead of treating an App state,
Simulator strategy, input mode and scenario mode word as one state variable.

The external corpus also records a semantic-only `cScenarioPlayMode` layout lead:

```text
size                    264 bytes
mCurrentPlayModeState   +0x90
mCurrentActIndex        +0xB8
mTimeLimitMS            +0xBC
mCurrentTimeMS          +0xC0
mAdventurePoints        +0xC4
```

These are **not** our GA offsets. The next valid use is to inspect the current GA SDK contract and
then corroborate fields against the GA ScenarioPlayMode body/vtable after the selected-star handoff
is real.

### UTFWin/Button boundary

The external event map gives a useful narrow seam:

```text
Window::SendMsg
  -> IWinProc priority/event flags
  -> IWinProc::HandleUIMessage
  -> event-specific InteractiveWinProc callback
```

It leaves callback consumption, bubbling, focus, tie ordering and thread affinity unresolved. This
matches our current GA boundary: serialized SPUI identity and the UTFWin message offsets are known,
but the generic Button capture/release/checked producer and CommandID route are not. The external
`IButton::Create` and Standard/Radio drawable names are useful search terms only; no complete native
Button event contract was promoted from them.

### StarManager / planet regeneration

The external `star-system-regen` dossier describes a bounded policy lead: a star with no planets,
after a timing guard, records observation time, removes a pending entry, resets manager state and
calls a planet-generation routine. This may guide a later GA probe around the existing
StarManager/refill work, but it is not the GGE candidate refill contract and does not establish
that `BB61C0` or the GGE refresh ran.

### Resulting probe order

```text
1. Use the GA-confirmed ScenarioMode registrar/vtable, plus the external field names only as
   candidate labels for the post-selection ScenarioPlayMode pass.
2. Continue GA UTFWin Button/WinProc static tracing around the exact GA Window message boundary;
   do not import external addresses or assume a button click posts the ScenarioMode tag.
3. Keep cStarManager planet-generation leads separate from GGE candidate refill and save-backed
   host preview statuses.
```

## What we must not import as a claim

```text
external Rust/Bevy architecture      not a requirement for our D3D9/x86 path
external addresses or offsets        not GA ReleaseGA evidence
external gameplay tuning              not native balance
external runtime success              not our runtime success
external generated counts             not our package corpus
```

## Decision

```text
methodology lead       adopted in REFS.md / existing audit practice
dossier schema         adopted as a future format lead, not copied wholesale
semantic bridge        backlog idea; no second authority database yet
Cell roadmap           backlog lead; blocked on local CellGame package availability
passport address facts rejected for direct merge (binary SHA mismatch)
source code            not imported
```

Next use is to apply the dossier discipline to the unresolved Button/WinProc and
selected-star→ScenarioMode questions, not to migrate the engine architecture.
