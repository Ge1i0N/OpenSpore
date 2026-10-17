# Analysis — navigation and evidence status

Reviewed **2026-10-15**. Start at the [project README](../README.md), [runtime status](../PROGRESS.md)
and [current verification ledger](../VERIFY.md). Not every analysis document is a current capability claim.

## Active implementation references

| Topic | Document | Scope / caution |
|---|---|---|
| Community SDK / format references | [community-references](community-references.md) | pinned ModAPI/SporeModder-FX clones; secondary names/offsets, not native execution or automatic readiness |
| Additional local references | [additional-references](additional-references.md) | user-supplied `_Also` beta/readme/package/SporeMaster corpus; secondary evidence and leads |
| FLARE-FLOSS strings | [floss](floss.md) | pinned source/release reference and reproducible GA string JSON dump |
| Bounded Ghidra runs | [ghidra-runner](ghidra-runner.md) | persistent PyGhidra probes with heartbeat, deadline and graceful/hard-stop policy |
| MiniWiki references | [miniwiki-references](miniwiki-references.md) | local historical console/cheat/command-line leads, explicitly secondary to GA evidence |
| MiniWiki command comparison | [miniwiki-command-compare](miniwiki-command-compare.md) | bounded historical-token vs GA-string report; no string-only readiness claims |
| SDK ↔ Ghidra bridge | [sdk-ghidra-bridge](sdk-ghidra-bridge.md) | cross-checked SDK method order, GA bodies/vtables and next focused probe ideas |
| Local launch modes | [launch-modes](launch-modes.md) | Debug batch launchers for Loader, DevLauncher, controlled GGE preview and Debug Cheat Console |
| OpenSpore Debug Cheat Console | [debug-menu](debug-menu.md) | Debug-only Ctrl+Shift+C command-entry state; uses host registry, not recovered EA ConsoleWindow |
| GGE placement-window identity | [gge-window-lookup](gge-window-lookup.md) | actual record+B8 receiver and root177; corrects earlier global-service interpretation, not native mount/camera readiness |
| GGE viewer versus ICamera | [gge-viewer](gge-viewer.md) | vtables/getters/update handoff and explicit projection composition; no live camera parameters or viewer producer |
| GGE camera factory / property | [gge-camera-controller](gge-camera-controller.md) | requested ID780786FD, factory type043C6E06, complete shipped prop and bounded FOV step; no live selection/pose readiness |
| GGE camera pose / modes | [gge-camera-pose](gge-camera-pose.md) | exact mode tables, explicit orbit transform and rigid inverse bridge; no default/live pose or full mode dynamics |
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
| PlanetRecord / saved adapter | [planet-record](planet-record.md) | sequential nested prefixes + six fields, counted planet selection and typed saved metadata; no native services, real corpus or live GGE scene |
| GGE per-star presentation | [gge-star-present](gge-star-present.md) | projection/root placement/stage configuration calculations + real asset controls; no camera creation, mounting, effects or live scene |
| GGE owned star instances | [gge-star-instance](gge-star-instance.md) | per-entry host graph, object-index mutations/pixel picking, renderer hook; native phase1/plugins/listeners/scene still absent |
| GGE phase-1 bindings | [gge-star-phase1](gge-star-phase1.md) | ordered branch/control/image requirements, Unknown/Missing/Ambiguous retained; corrected always-true native return, no service execution |
| GGE image application | [gge-image-bindings](gge-image-bindings.md) | explicit DBPF PNG provider + per-instance plain ImageDrawable overrides/resolver/GPU test; not Animated Icon or full phase/scene execution |
| GGE Animated Icon selector | [gge-animated-icon](gge-animated-icon.md) | separate static frame/index/atlas bindings, QI offset correction, actual five-frame assets; no auto playback or scene readiness |
| GGE star host lifecycle | [gge-star-lifecycle](gge-star-lifecycle.md) | owned generation-scoped action application/retry, names/centering/image/frame bindings; host-independent policy, not native phase/listener completion |
| GGE direct package images | [gge-streaming-images](gge-streaming-images.md) | bounded file-backed PNG provider + 64-bit resource/index seeks; actual large original atlas feeds lifecycle without a test carrier |
| GGE host scene boundary | [gge-star-scene](gge-star-scene.md) | explicit Available catalogue + active camera → owned placement/layers/pick; stale-frame suspension, not native factory/effects or visible galaxy completion |
| GGE explicit phase/show cases | [gge-star-transition](gge-star-transition.md) | observed guard + requested2..5, narrow visible/name/rect subset; actual panel hit without IgnoreMouse workaround, no native latch/services/dispatch |
| GGE input boundary | [gge-star-input](gge-star-input.md) | separate world-ray vs UI-window picking, active-camera guard, native F8/FC/DF7800 boundaries; no callback/CommandID dispatch |
| GGE controlled clickable preview | [gge-host-preview](gge-host-preview.md) | Debug-only 3-card host screen with real SPUI/atlas, explicit camera and normal input latch; clearly not native GGE/catalogue |
| GGE catalogue producer boundary | [gge-catalog-producer](gge-catalog-producer.md) | manager vectors/DF95A0 ordering/refill/save persistence recovered; live producer and original stars.db corpus still absent |
| GGE catalogue snapshot input | [gge-catalog-snapshot](gge-catalog-snapshot.md) | caller-selected strict UTF-8 capture loader + atomic Unknown/Available statuses; no discovery/generation/profile scan |
| GGE terrain correlation | [gge-real-save](gge-real-save.md) | explicit starter-world/property matches across selected packages; duplicate package matches stay ambiguous, no native readiness claim |
| GA PropManager terrain lookup | [audit/gge-terrain-prop](audit/gge-terrain-prop/README.md) | vtable/getter/lookup contract cross-checked with SDK; no runtime package priority or refill claim |
| GA StarManager candidate refill | [audit/gge-refill](audit/gge-refill/README.md) | BB61C0 quota/filter/fallback/append/write order cross-checked with StarManager field layout; static only |
| Refill event versus GGE refresh | [audit/gge-refill-event](audit/gge-refill-event/README.md) | 0x062D91C2 consumer is StarManager-side; no direct DF95A0/GGE refresh edge claimed |
| GGE message-to-state dispatch | [audit/gge-message-dispatch](audit/gge-message-dispatch/README.md) | DE99B0 event IDs update internal state fields; 0x036C4A10 matches SDK IStateManager +0x28 contract |
| GA StateManager command dispatch | [audit/state-manager-dispatch](audit/state-manager-dispatch/README.md) | 0x007E5050 directly corroborates SDK StateIDFromName +0x38 and SwitchToState +0x28; distinct from GGE DF9740 |
| GGE internal state → UI | [audit/gge-state-ui](audit/gge-state-ui/README.md) | DE48C0 maps internal state 5/13 to UI control IDs; no direct DF9740/catalogue claim |
| State script | [states](states.md) | recovered state data, not proof of gameplay |
| Startup | [startup](startup.md) | GA execution chain and addresses |
| Main → playable roadmap | [main-to-playable-roadmap](main-to-playable-roadmap.md) | current layer status, priority order and parked console work |
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
