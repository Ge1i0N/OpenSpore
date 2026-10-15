# PROGRESS — current capabilities and validation

Reviewed **2026-10-15**. This is a runtime/status summary, not a completion percentage.
Detailed counts and the dashboard are generated from [features.json](analysis/features.json):
[full ledger](analysis/features.md) · [stats JSON](analysis/features_stats.json).

![Feature dashboard](analysis/shots/reverse_map.png)

## Main → playable

```text
entry / CRT / WinMain → boot → packages / config → states → real-art launcher → visible selection
                                                                            ↓
                                                     CreatureEditor state entered
                                                     editor & game simulation NOT implemented
```

| Layer | Current result | Boundary |
|---|---|---|
| Boot / platform | MSVC Win32 Debug, warning-free; window/D3D9/console/log/capture | x64 untested; not release-ready |
| Packages | 57 DBPF files / 150,945 records; index invariants pass; RefPack implemented | full resource priority and write/repack open |
| Locale | 265 distinct tables / 23,933 distinct table-key texts in the recorded en-us corpus | original text/font pipeline incomplete |
| SPUI framing | 748/748 exact EOF in Python/C++; code 19 references, code 20 nested structs | framing is not full widget behavior |
| SPUI graph | 93,726 objects / 22,384 windows / 20,659 child links; shared ancestor Clip for paint/picking | dynamic Z/layout still host policy |
| Drawables | atlas UV/logical sizes; None-tiling ImageDrawable; non-tileable Standard/Radio state slices | tiling, shadow/outline, dialog pieces open |
| Input | visible `(311,550)` -> CreatureEditor `0x022D9E01` | control/state bridge, not original command bus |
| GGE primary UI | Main -> GalaxyGameEntry presents the GA-default `GlobalUIGGE-9` | sparse serialized visibility; galaxy scene, zoom panels, properties/controller events not implemented |
| GGE Intro gate | phase-6/movie-idle predicate and DBCheck notification/one-shot requests modelled and tested | runtime explicitly waits for unimplemented MoviePlayer observation; no actual DBCheck/state10 transition |
| Models | real gmdl/raster subset and textured diagnostic model gallery | full RW4, animation, materials and game world open |
| Gameplay | mode registration table recovered | no playable stage or implemented editor |

## Repeatable regressions

| Check | Recorded result | Command / evidence |
|---|---|---|
| Build | 0 compiler warnings/errors, x86 Debug | `cmake --build build --config Debug --target OpenSpore spui_verify pkg_lifetime_verify overlay_clip_verify` |
| Boot | 10/10 markers | `-w -devDirs`, newest section of `OpenSpore.log` |
| Independent SPUI parse | 748/748 through EOF | `python tools/spui_objects.py --parse --all ...` |
| C++ corpus + malformed inputs | 748 matches; 486 rejected atomically | `spui_verify <fixtures>` |
| Drawable subset | atlas, standard/radio states, fixed scale, three alignments, unsupported modes | same `spui_verify` target |
| Package lifetime | 544 open/read/destruct cycles at CRT limit 512 | `pkg_lifetime_verify <package>` |
| Ancestor Clip | nested/empty/boundary geometry, shared picking; D3D9 quad/text/state pixel readback PASS | `spui_verify <fixtures>` and `overlay_clip_verify <new-capture.bmp>` |
| Index invariants | 57 packages / 150,945 records / 0 violations | `python tools/pkg_kinds.py` |
| Live launcher | real atlas-aware art, exit 0, aborts/asserts 0 | [capture](analysis/shots/spui_e3launch_uv.png) |
| Visible transition | CreatureEditor entered, exit 0 | `-state:DevLauncher -quitAfter 18 -exec spui:-click,311,550` |
| GGE UI binding | exact mode/name/hash test; Main -> GGE capture, exit 0 | `mode_ui_verify`; [static/live boundary](analysis/galaxy-entry.md) |
| GGE post-title guard | 2040 controlled-signal checks; no time-based auto-completion | `gge_intro_verify`; [Intro evidence/limits](analysis/gge-intro.md) |
| Historical RefPack corpus | 30,000 exact decoded lengths | `python tools/refpack.py --verify --limit 30000`; not re-run by every doc refresh |

Full setup and fixture commands: [README](README.md#сборка-и-запуск).
Hardware-specific FPS and historical export counts are **not** current performance promises.

## How to interpret progress

Done/partial/todo describe bounded items with unequal scope. Data observations and harness features
do not equal implemented gameplay. Partial items do not count as done; the optional half-credit index
is merely `(done + 0.5 × partial) / total`. The manual [function catalogue](analysis/functions.md)
is separate and is not exhaustive binary-code coverage.

## Immediate work

1. Needed drawable and event behavior, tied to GA evidence.
2. GalaxyGameEntry AppMode behavior and the first playable stage.

See [RESUME](RESUME.md), [VERIFY](VERIFY.md), [analysis index](analysis/README.md).
Earlier tables, obsolete hashes and the hidden GGE demo are retained only in
[the archive](docs/archive/PROGRESS-2026-10-15.md) and [HISTORY](HISTORY.md).
