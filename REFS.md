# OpenSpore reference registry and evidence pipeline

Reviewed **2026-10-18**. This file is the short, durable answer to two questions:

1. **Where do we look next?**
2. **Which source is allowed to decide a claim?**

This is a research registry, not a list of runtime dependencies. External source trees are
reference material only; proprietary game code/assets are never copied into the repository.

## The rule in one paragraph

Use the SDK and community tools to find the likely semantic name, class, field or format. Use
persistent Ghidra/REA against the selected GA-unpacked executable to decide whether that semantic
mapping is true for this build. Use shipped GA data and explicitly selected saves to check the
format and values. Use runtime observation only when the original service is actually observed.
Use other reimplementations to find approaches, missing probes and useful test cases, never to
import an address, offset, balance value or readiness claim. Preserve `Unknown`, `Absent`, `Empty`,
`Missing`, `Ambiguous` and `Failed` as different outcomes.

## Authority order

The order below is about **authority**, not convenience. A lower row can be very useful without
being able to overrule a higher row.

| Priority | Source | What it may establish | What it may not establish by itself |
|---:|---|---|---|
| 0 | Exact target identity and local evidence manifest | Which GA build, file, hash and evidence snapshot are being discussed | That a similarly named binary is equivalent |
| 1 | `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe` + persistent Ghidra/REA | GA addresses, instructions, xrefs, vtables, constants, call shape and observed static guards | Runtime execution, guessed types, or semantics absent from the bytes |
| 2 | GA shipped packages, explicit GA saves and controlled original-runtime observations | Accepted data shapes, resource identities, byte values, execution/capture results | A complete parser from one record, or native readiness from a host capture |
| 3 | Version-matched community SDK semantics: Spore-ModAPI / SDK XML | Names, method order, interface/vtable hypotheses, field vocabulary and target selection | GA addresses, exact offsets, field meaning, or successful service calls |
| 4 | SporeModder-FX, SporeMaster, local `refs/_Also`, MiniWiki and beta/PDB material | Historical names, registries, format leads, command vocabulary and comparison cases | GA retail behavior, GA addresses, or a replacement for binary evidence |
| 5 | OpenSpore reimplementations and forks listed below | Alternative implementations, test strategy, missing-question lists and reproducible leads | Native parity, exact GA values, or permission to copy code/assets |
| 6 | Our own inference and host policy | A clearly labelled approximation that moves a bounded harness forward | Promotion to observed/verified/native-ready status |

**Tie-breaker:** when two sources disagree, retain both observations, report the disagreement and
return to the GA binary/data. Do not silently average them.

## Operational pipeline

Every new subsystem or `Unknown` should go through this sequence. The output of each stage is a
committed, bounded artifact rather than a conclusion in chat.

1. **State the question.** Add the exact unknown, target identity, desired evidence level and a
   stop condition to `VERIFY.md`, `RESUME.md` or the relevant analysis dossier.
2. **Semantic pass.** Search the SDK headers/XML and SporeModder-FX registry/source for candidate
   class names, fields, vtable order, resource keys and terminology. Record these as hypotheses.
3. **GA static pass.** Query the persistent Ghidra project or REA, then byte-check the selected
   addresses against the GA-unpacked file. Retain assembly/decompilation, xrefs, target hash and
   coverage/limitations under `analysis/audit/`.
4. **Shipped-data pass.** Check the relevant package index/record and, where authorized, an
   explicitly selected save. Correlate identities and byte shapes; do not turn a missing record
   into an empty collection.
5. **Runtime pass.** Run a narrow original/runtime probe only if the required service and input
   are reachable. A negative or unavailable observation is evidence too; it does not become a
   timeout-based success.
6. **External comparison pass.** Read the pinned OpenSpore repos/forks for implementation shape,
   tests and leads. Compare the claim against steps 2–5 before using it.
7. **Minimal implementation pass.** Port only the contract supported by the evidence. Keep host
   policy, structural decoder, native claim and gameplay behavior in separate types/statuses.
8. **Regression and record pass.** Build/test/capture, update the evidence ledger and ideas list,
   commit the code and docs together. The commit message should say how the result was verified.

### Promotion rule

`SDK name` → `GA static match` → `shipped-data correlation` → `runtime observation` is a useful
promotion path, but stages are not interchangeable. A green test of our own fixture proves our
implementation contract, not original-game parity. A screenshot proves our renderer drew pixels,
not that the original mounted the same UI.

## Local primary and secondary references

| Location | Role | Current use |
|---|---|---|
| `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe` | GA address oracle | Exact static evidence; hash is recorded in `MEMORY.md` and `VERIFY.md` |
| `refs/Spore-ModAPI` | SDK semantic reference | Interfaces, fields, vtables and versioned names; never copy addresses blindly |
| `refs/SporeModder-FX` | Format/registry reference | Resource keys, type names, property framing and tooling comparison |
| `refs/_Also` | User-supplied historical corpus | Beta/final package comparisons, SporeMaster and command/debug leads |
| `refs/MiniWiki` | Historical notes | Console, cheats and command-line vocabulary only |
| `refs/ghidra` / persistent REA project | Analysis machinery | Reusable GA analysis state, seeds, snapshots and bounded queries |
| `analysis/audit/` | Evidence ledger | Retained instructions, reports, target identity and limitations |
| `analysis/` | Topic dossiers | Current interpretations, boundaries, next probes and corrections |

The detailed local clone revisions and licensing notes are maintained in
[`analysis/community-references.md`](analysis/community-references.md). The additional local
corpus is described in [`analysis/additional-references.md`](analysis/additional-references.md).
The first actual external-clone reproduction is recorded in
[`analysis/audit/external-openspore/README.md`](analysis/audit/external-openspore/README.md).
The methodology review of `openspore-old` is recorded in
[`analysis/audit/external-openspore-old/README.md`](analysis/audit/external-openspore-old/README.md).

## External OpenSpore repositories

The following revisions were inspected on **2026-10-18** with `git ls-remote` and targeted raw-file
reads. They are deliberately pinned because GitHub `main` moves. Findings below are leads until
the relevant code/data is independently checked against our GA target.

| Repository | Inspected revision | Classification | Highest-value material |
|---|---|---|---|
| [OpenSporeLabs/openspore-old](https://github.com/OpenSporeLabs/openspore-old) | `970236558184bd88cd5c356c39b5b57fb5636afd` | Research corpus + Rust/Bevy direction; secondary | `docs/RE-WORKFLOW.md`, `docs/RE-DOSSIER-SCHEMA.md`, semantic bridge/KG, Cell-stage roadmap, package/asset staging boundaries |
| [OpenSporeLabs/OpenSpore](https://github.com/OpenSporeLabs/OpenSpore) | `92ae75db64de577cf4a220d82ee64bac2c35d75c` | Current upstream-style implementation reference; secondary | Bevy-free asset layer, typed provenance, deterministic `--info` reports, package-priority rules, creator-document boundaries |
| [Ge1i0N/OpenSpore](https://github.com/Ge1i0N/OpenSpore) | `51ba127f9da0f57fdb01a9b71b8bd557fcd8f965` | Fork with useful GA-save/Cell-stage leads; secondary | fresh-save structural reports, `stars.db`/planet-record observations, CellGame data decoder, first playable Cell slice and explicit seed boundaries |

### Findings worth bringing into our backlog

#### `openspore-old`: process and evidence infrastructure

- The seven-stage `RE-WORKFLOW` is a good template: target → committed static evidence → runtime
  probe → asset correlation → frozen fixture → replacement contract → status record.
- `RE-DOSSIER-SCHEMA` makes missing runtime traces and missing optional inputs explicit instead of
  turning them into fabricated empty results. This matches our Unknown-preserving policy.
- The semantic bridge/KG is useful as a query/index design: address-keyed passports, provenance
  on facts, and `Unknown` when a fact is absent. It does **not** replace our GA-unpacked evidence.
- The Cell roadmap identifies `UTFWin`, `cResourceManager`, property serialization, Terrain and
  Simulator as high-leverage shared systems. These are useful prioritization signals, not proof
  that their values or call paths match our GA build.

#### `OpenSporeLabs/OpenSpore`: clean asset/runtime boundaries

- Keep decoders and typed asset resolution renderer-agnostic; attach them to a renderer only at
  the final staging boundary. This is relevant when our current D3D9 host path grows toward RW4.
- Preserve typed evidence grades and partial decode stop reasons. The external GMDL example is a
  useful model for “geometry decoded, later material tail Unknown” rather than guessing the tail.
- Package priority is a product decision and must be tested on real duplicate records. This
  reinforces our existing rule that package precedence is never inferred from filename order.
- Creator-document composition, per-block unresolved rows and `scene_stable` are useful ideas for
  future editor/stage work, but none is native GGE evidence.

#### `Ge1i0N/OpenSpore`: highest-priority technical leads

- `analysis/real-saves.md` reports a fresh GA `stars.db` with DBPF 3.0/index3, the root/context
  keys expected by our structural adapter, and `planetRecords.pkp` rows in the expected
  `mKey + i*0x01000000` space. This is the most direct external lead for the remaining real-save
  catalogue gap. Re-run the decoder or compare its audit JSON before promoting any field.
- `analysis/starsdb-real.md` reports CRC-clean envelopes and full structural StarRecord shape on
  that corpus. The immediate audit target is the exact reader, fixtures and independent byte
  comparison—not the headline count.
- `analysis/cell-game.md` and `analysis/game-stages.md` describe the CellGame package families
  (`cell`, `structure`, `populate`, `world`, `look_table`, `prop`, `backgroundMap`, `advect`,
  `density`, `xls`) and the first playable Cell slice. The shipped resource identities are strong
  leads; positions, AI, balance, camera and several stat meanings are explicitly marked as seeds,
  so they must not be imported as GA behavior claims.
- The fork's save-stage sequence and structural class observations can guide which explicit files
  to request/inspect next, without scanning profiles or modifying user data.

### External-source audit order for this project

1. Compare the fork's real-save decoder and audit JSON against our `starsdb_*`, `save_header`,
   `planet_record` and `gge_saved_adapter` contracts on one caller-selected save.
2. Compare the CellGame resource decoder against our GA `CellGame` package, then promote only
   byte-correlated fields to the first playable stage; retain seed gameplay values as host policy.
3. Extract the semantic-bridge/dossier ideas into our existing `analysis/audit` format only where
   they reduce evidence loss; do not import a second address database without a provenance plan.
4. Revisit UTFWin/Button and `cResourceManager` leads when they unblock Main → first playable stage.
5. Defer online/Sporepedia, multiplayer, broad creator and full Bevy migration work until the
   forward GA path needs them.

## Cloning policy for `refs/`

Yes, needed repositories may be cloned under `refs/`, but only with a stable, non-duplicating
layout. This is the policy for future additions:

```text
refs/thirdparty/<owner>-<repo>/       # one clone per repository
refs/thirdparty/manifest.tsv          # URL, pinned commit, date, purpose, license note
```

- If the target directory is absent, make one shallow/filtered clone and check out the pinned
  commit detached. If it already contains a Git clone of the same URL, fetch/check out the new
  pinned commit instead of cloning a second copy.
- If a non-Git directory already occupies the proposed path, do not overwrite or merge it; choose
  a new explicit name and record the relationship in `manifest.tsv`.
- Keep third-party trees ignored, read-only for our build, and outside CMake source globs. Do not
  copy their source into `source/`, `upstream/` or generated evidence. Record license/provenance
  before considering any clean-room reimplementation of an idea.
- Because the repository intentionally ignores the whole `refs/` tree, the local
  `refs/thirdparty/manifest.tsv` is a convenience for the working machine, not the tracked
  authority. Keep the pinned URL/SHA/purpose in this file (and in an analysis audit when the clone
  is actually used).
- Never clone game installs, user saves or proprietary assets into the repository. Use explicit
  caller-selected paths and read-only probes.
- A clone is not evidence by itself. Every imported lead still needs a source path, commit, local
  reproduction and GA corroboration before changing `VERIFY.md` or a native readiness status.

## Quick decision checklist

Before writing “ready”, ask:

- Is this the exact GA-unpacked file, not beta/PDB/fork output?
- Do we have bytes/instructions or only an SDK name?
- Is the result static, shipped-data, runtime-observed, host-policy or inferred?
- Did we distinguish Unknown from Empty/Missing/Failed/Ambiguous?
- Can the claim be reproduced from a committed audit, fixture or capture?
- Did an external repository only suggest the probe, or did we accidentally treat it as proof?
- Did the implementation advance Main → playable without quietly widening scope?

For current project status use [`RESUME.md`](RESUME.md), [`VERIFY.md`](VERIFY.md),
[`PROGRESS.md`](PROGRESS.md) and [`IDEAS.md`](IDEAS.md). This registry defines how those files
should be updated, not a replacement for them.
