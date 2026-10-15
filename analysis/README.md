# Analysis — navigation and evidence status

Reviewed **2026-10-15**. Start at the [project README](../README.md), [runtime status](../PROGRESS.md)
and [current verification ledger](../VERIFY.md). Not every analysis document is a current capability claim.

## Active implementation references

| Topic | Document | Scope / caution |
|---|---|---|
| SPUI binary framing / graph | [spui-binary](spui-binary.md) | current parser authority; 748-layout corpus |
| Atlas / drawable subset | [spui-drawables](spui-drawables.md) | current implemented branches and GA instruction extracts |
| UI overview | [ui](ui.md) | current navigation, links to deeper evidence |
| AppMode registrations | [modes](modes.md) | ten recovered registrations, not implemented driver behavior |
| GalaxyGameEntry | [galaxy-entry](galaxy-entry.md) | lifecycle/UI-loading evidence and implemented default primary UI subset, not a playable galaxy |
| GGE Intro | [gge-intro](gge-intro.md) | post-title guard/latch model, MoviePlayer and DBCheck evidence; runtime dependencies still missing |
| DBCheck / Play Spore | [gge-dbcheck-play](gge-dbcheck-play.md) | explicit-backend DBCheck branch executor tested with real dialog fixtures; state-2 game-list/effect/UI dependencies, no live scene yet |
| Play Spore catalogue | [gge-catalog](gge-catalog.md) | saved/new source tracing and typed field projection tests; not stars.db parsing or a live galaxy catalogue |
| stars.db serializer | [starsdb](starsdb.md) | bounded prefix, wire references/grid and 14 StarRecord fields; wire-only mode does not resolve identities, no real-save validation or live producer |
| stars.db context | [starsdb-context](starsdb-context.md) | decoded context index/class stream, envelope CRC and structural reference lookup; fixups/runtime absent |
| stars.db physical adapter | [starsdb-dbpf](starsdb-dbpf.md) | read-only exact-key DBPF 3.0 subset into root/context/class readers; constructed tests only, no real-save corpus/fixups/runtime |
| StarRecord post-load | [starsdb-postload](starsdb-postload.md) | corrected empty/BAD_DATA equality decision model; no canonical-name comparison, actual generation/profile/setter services disabled |
| Saved-game file header | [save-header](save-header.md) | actual file version fields from explicit root resource, unsigned probe guard; not StarRecord saved versions or a complete live save producer |
| State script | [states](states.md) | recovered state data, not proof of gameplay |
| Startup | [startup](startup.md) | GA execution chain and addresses |
| Resource loading | [content-loading](content-loading.md) | resource-manager observations, unresolved virtual boundaries |
| Config | [config-scripts](config-scripts.md) | implemented script subset |
| Locale | [text-locale](text-locale.md) | per-table keys, nested packages, precedence caveats |
| Asset format inventory | [formats](formats.md) | supported subsets vs unresolved formats |

## Generated reports

| Output | Source / generator | What it measures |
|---|---|---|
| [features.md](features.md), [dashboard](shots/reverse_map.png), [stats](features_stats.json) | `features.json` / `tools/feature_map.py` | bounded checklist counts, **not** game completion |
| [functions.md](functions.md), `functions_stats.json` | `functions_named.json` / `tools/functions.py` | manual catalogue and heuristic starts, **not** exhaustive code coverage |
| [audit.md](audit.md) | `claims.json` / Ghidra audit tools | retained selected-claim snapshot, not every current feature |
| [arg_vocab.md](arg_vocab.md) | `tools/arg_vocab.py` | GA argument-helper callers |
| [pdb_symbols.md](pdb_symbols.md), [pdb_topology.md](pdb_topology.md) | PDB tooling | beta names/topology, never GA addresses |
| [name_guesses.md](name_guesses.md) | `tools/name_guess.py` | ranked hypotheses, not confirmed function identities |

```powershell
python tools/feature_map.py
python tools/feature_map.py --check
python tools/docs_check.py
```

The review date is stored in the feature source. `--check` compares artifacts in the same rendering
environment; PNG bytes can differ between operating systems/fonts. Generated text is not hand-edited.

## Identity, toolchain and historical investigations

- [Build identity](build_identity.md), [plaintext builds](plaintext_text.md),
  [GA-unpacked verification](unpacked_verification.md), [retail recon](recon.md).
- [REA / persistent Ghidra workflow](toolchain-rea.md), [audit review](audit-review.md).
- [SPUI loading experiments](spui-loading.md) and [schema experiments](spui-schema.md) retain
  superseded token/anchor models; their correction notices and `spui-binary.md` take precedence.
- [Decision-API experiment](jev-decisions-api.md) is a historical external-service note, not a
  required build/runtime dependency or permission to make new network requests.
- [Archived root notebooks](../docs/archive/README.md) and [HISTORY](../HISTORY.md) preserve earlier
  conclusions. Do not execute an old "next step" just because it appears in a snapshot.

## Evidence contract

`audit/spui-binary/` and `audit/spui-drawables/` retain GA target hashes, instruction ranges and
decompiler limitations. Static analysis does not observe execution. A capture proves our renderer's
output; it does not by itself prove original runtime parity. Game artwork shown in screenshots is
not a replacement for owning the original game data.
