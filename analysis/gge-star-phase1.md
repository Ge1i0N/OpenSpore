# GGE phase-1 — ordered bindings, not native service completion

Reviewed **2026-10-15**. Authority: GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static instructions + two real `GGEGameStarUIv4` fixtures; no original-runtime replay.

## Implemented scope

`engine/gge_star_phase1.*` builds an ordered **binding plan** for the phase-1 case body of
`0xDF6D80`. It uses an active [owned host instance](gge-star-instance.md), its entry metadata and
explicit observations for external branch inputs. It does not execute the current-phase guard,
native operations, optional-control skip/error policy, phase latch, resource lookups or listeners.
No image is loaded, no graph is mutated, no visibility is forced, and GGE appearance is unchanged.

Structural targets are **Found / Missing / Ambiguous**, with candidate object indices retained.
Unknown is the default/unobserved status, not absence. Found means a unique structural target or
a validated explicit object choice, **not** successful native interface/image/plugin resolution.
Every request's resource status stays Unknown; carrying an image ResourceKey never turns it Found.

Unobserved dynamic branch inputs produce the safe inspected prefix and `blocked_on`, not the
absent branch. Invalid choice lists/inactive instances return false with output unchanged.
`inspected_branches_observed=true` means only that the planner reached the end of this inspected
case body; `native_services_executed` and `native_mount_complete` always remain false.

## Correction: phase-1 return is not a dependency validator

Earlier reports said phase-1 could return false/fail and prevent listener attachment. That was an
overinterpretation of the caller's test. **All inspected normal `0xDF6D80` returns set AL=1**, including
the missing-root skip (`0xDF700B → 0xDF765B`) and already-current/default exits.
`0xDF7690` does test that return before attaching listeners, but no retained false phase-1 return
path establishes a real dependency check. It returns the layout-load result, not interface readiness.
Missing unchecked controls may instead be dangerous native assumptions; no crash behavior is emulated.

The host plan deliberately makes missing/ambiguous/unknown dependencies explicit instead of using
either return value as a success signal. Original return behavior is retained as evidence, not
reconstructed as a synthetic native completion event.

## Request order and branches

| Inspected order | Targets / inputs |
|---|---|
| Root visible flag | `0x02E5A4A8`, mask 1=true; missing root skips native body but still returns 1 |
| Related panel reset/centering | `0x05B31710` mask1=false; `0x03753F40`, `0x04656452`, `0x046A9CA7` mask1=false + center in root |
| Thumbnail branch | entry native `+0x3C.instance` nonzero: image requests `0x02E71340/0x046EA568`; zero: hide both, show `0x068DAC20/21` |
| Main image | entry `+0x30` ResourceKey → `0x046A9A21/0x046EA4B6` |
| Tooltip userdata | `0x046EA4B6`, key `0x037AC490`; native value is game-ui-data+0x18, no host pointer invented |
| Stage description | `0x0594A640/0x05FF9352`, see separate locale table below |
| Profile branch | entry `+0x48` pointer nonnull → profile name `0x05FF9AD2/0x05FF9D44` |
| Layer relation | `0x05B31710` related to `0x05B347BC` if saved, otherwise `0x04656452`; `0x80DE40` is not approximated as a flag write |
| Selected-star branch | `0xDE45C0` result saved byte controls saved terrain layer operations; distinct from GGE entry.saved |
| Stage-image source | control `uint32(stage + 0x03744FF0)`; current drawable image obtained through `0xDE3DB0` |
| Stage-image targets | `0x02E713A8`, `0x046EA523`, `0x026E0001`, `0x026E0002` through `0xDE3EB0` |
| Entry name | `0x02E712E4/0x046EA550` |
| Selected-star timestamp | separate observed selected-StarRecord presence → `0x046A9A9B/0x046EA507` timestamp and `0x047E3CD3` colour requirement |

Thumbnail input is optional because the typed `GameEntry` currently omits native `+0x3C`. An
observed zero-instance key is not Unknown, regardless of its other two words. Profile-pointer
presence is also not a profile-service result. Selected-StarRecord saved/presence observations
are separate from the UI entry and each other (the native lookups occur at different points).

`0xDE3F50` phase-1 description uses table `0x2B7A0311` and stages 0/1/2/4/5 map to keys
`0x05B08702/03/04/05/06`. Other stages have no selected locale key in this switch; the plan preserves
that distinction without inventing a stage label. These are **not** `0xDF9200`'s smaller stage
caption keys `0x067735D4/D5`. Locale/string-builder services are not executed by the planner.

## Two distinct image mechanisms

- `0xDE3DB0/0xDE3EB0`: window slot `+0xA8` obtains its fill drawable; QI **`0xEF3C47CF`** exposes
  image getter `+0x18` and setter `+0x14`. Setter path invalidates the window via `+0x90`.
  A supplied nonnull shared image bypasses ResourceKey lookup. If null, it tries `0x806320` with
  the given key; stage-target calls pass a zero triple. A missing stage image is therefore not
  permission to choose an arbitrary reachable template image or pretend an asset loaded.
- `0xDF9200`: window QI **`0x0106F146`** returns AnimatedIconWin itself; the caller then takes
  its embedded interface at `+0x20C`, slot `+0x14`, which receives the stage index. This is not the
  image-drawable interface above. [Recovered selector/ABI correction](gge-animated-icon.md).

Real ControlID `0x026E0002` matches two objects, 45 and 55; one is Animated Icon class `0x0106F14B`,
the other a regular window. The planner does not use class type alone to guess native recursive
lookup order or select a hidden/visible match. Explicit choices must actually belong to the target
control in this instance, and duplicate/conflicting/unrelated choices are rejected. All candidates
remain recorded even after an explicit selection.

## Verification / continuation

`gge_star_phase1_verify`: **247 checks PASS**. Both real layouts, ordered prefixes, Unknown versus
observed zero/false, thumbnail/main separation, locale variants, profile/selected-star branches,
distinct interfaces, duplicate choices, missing stage sources including uint32 wrapping, no graph
mutation, invalid inputs and transactional failure. No native actions or service completion tested.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_phase1_verify
.\build\Debug\gge_star_phase1_verify.exe <SPUI-fixture-directory>
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --phase1
python tools/gge_evidence.py --static --phase1
```

[Evidence](audit/gge-star-phase1/facts.json) retains eight functions plus six focused extracts.
Follow-up [image application](gge-image-bindings.md) now implements one resolved image operation
with an actual PNG provider, per-instance overrides and resolver path. Other phase-1 actions/lifecycle,
native-like callbacks and scene/camera integration remain open. This plan
does not by itself move the original Intro gate, attach a live catalogue or display a galaxy.
The [host lifecycle](gge-star-lifecycle.md) now applies available independent requests and reports
unsupported services separately; that retry policy does not implement native phase/dependency order.
The [case2..5 transition follow-up](gge-star-transition.md) now explains how separate explicit caller
requests reveal particular panels. Its current-phase guard is an independent observation, never
inferred from this phase1 planner or a host apply pass returning true.

Final Debug x86 warning-free; 247 new plan checks and all prior instance/projection/parser/catalogue/
DBCheck/Intro/binding/SPUI/package/scissor suites PASS. Six Python tests, 52 maintained Markdown
link checks and generated dashboard --check PASS. Main → GGE and visible launcher → CreatureEditor
with `-w -devDirs` exit 0, aborts/asserts 0; GGE still explicitly waits on MoviePlayer Unknown.
