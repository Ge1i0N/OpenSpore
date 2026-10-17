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
source code            not imported
```

Next use is to apply the dossier discipline to the unresolved Button/WinProc and
selected-star→ScenarioMode questions, not to migrate the engine architecture.
