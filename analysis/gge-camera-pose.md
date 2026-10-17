# GGE camera pose — mode tables and explicit orbit transform

Reviewed **2026-10-17**. Authority: selected GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static file-byte instructions/PE imports, community layout corroboration and controlled host tests;
no native current-mode, initial-pose or camera-update runtime capture.

## Mode objects behind `16A10C8`

`DDFB90` calls the selected mode's slot0 with `(delta,viewer)`, then slot4 with a Transform output.
Static initialized object words and their two-entry dispatch tables establish:

| Object storage | Initial table | Slot0 | Slot4 (Transform builder) |
|---|---|---|---|
| `15A30B4` | `0147DA0C` | `DDE810` | `DDF2B0` |
| `15A30B8` | `0147DA14` | `DDFEA0` | `DDF480` |
| `15A3110` | `0147DA1C` | `DDE300` | `DDF610` |

"Normal", "transition" and "timed" below are host analytical labels, not recovered Maxis symbols.
Constructor/Initialize paths set `16A10C8` to `15A30B4`. `DDFB00` prepares a transition via `DDE260`
and selects `15A30B8`; `DDE8B0` sets up and selects `15A3110`. `DDE300` advances a timer, dispatches
message `04360E14` on its completion branch, resets its timer word and returns to `15A30B4`.
This is static dispatch/transition evidence, not a live observation of which mode GGE is in.

`DDE810` calls the substantial dynamics helper `DDE580` with milliseconds converted to seconds,
then moves shared offset `15A3048..50` toward `15A3054..5C` with factor0.1. `DDE580` uses `_CIpow`
(`0.1^dt`) to decay velocity, adds `(target-current)*2.0*dt`, advances current by velocity*dt,
and clamps current between explicit mode fields. It also contains directional constraints and wrap
behavior outside this one-axis subset. `step_camera_dynamics_axis` ports this bounded arithmetic;
mode input, shared BSS, wrap and viewer update remain unported.

## Sources and transform builder

`DDF2B0` and `DDF480` both initialize a scale1 Transform, copy an explicit nine-float basis from
`16A10E0`, take pivot from `16A10CC..D4`, rotate two row pairs, displace along the rotated second
row, then add a separate offset:

| Input | Normal builder `DDF2B0` | Transition builder `DDF480` |
|---|---|---|
| first angle, negated before helper | global `15A308C` | mode `+30` |
| second angle | global `15A3090` | mode `+20` |
| displacement distance | global `15A3094` | mode `+28` |
| additional offset | globals `15A3048..50` | mode `+34..3C` |

Normal builder also clamps global `15A3088` before construction; that is **not** permission to use
it instead of the separately loaded displacement `15A3094`. The new helper accepts inputs after
any caller-selected clamping, and does not guess clamp limits or a selected native mode.

Basis and pivot reside in BSS, with no file-backed runtime values. Initializer **136CF30** explicitly
writes identity to the basis; this is a recovered initialization operation, not proof that the live
basis is immutable or that the host may default missing observations to identity. The property-reader
angle assignments use sibling scratch/global values in nontrivial ways; no initial pose is inferred
from the camera prop's -33/0 entries alone.

### Recovered finite-input arithmetic

For original basis rows `a,b,c`, let `s1=sin(-angle1), c1=cos(-angle1)`,
`s2=sin(angle2), c2=cos(angle2)`. Helpers `4099B0` then `5A2D70` give:

```text
rotation.row0 = c1*a + s1*b
intermediate  = -s1*a + c1*b
rotation.row1 = c2*intermediate + s2*c
rotation.row2 = -s2*intermediate + c2*c
position      = pivot - distance*rotation.row1 + additional_offset
scale         = 1
```

The first helper's imports are verified through PE IAT: `11E0772 -> _CIsin`,
`11E076C -> _CIcos` in MSVCR90.dll. `41DE00` corroborates scalar-times-vector operations.
This preserves actual row order/sign, not an assumed Euler convention. Host `std::sin/cos` is not
claimed bit-identical to native x87/imported trig, nor are intermediate rounding/flags/count words.

## Implemented host boundary

`compose_camera_orbit` in `source/engine/gge_star_present.*` requires optional basis, pivot,
additional offset, both angles and distance explicitly. Missing, nonfinite or overflowing input
leaves output unchanged. Negative/zero distances remain explicit values; the helper does not invent
the native mode's clamp/generation policy. Output is a value pose, not native Transform allocation.

`camera_pose_world_to_view` adds a restricted rigid inverse for that scale1 pose: transpose the
rotation, then translation component `i = -dot(position,rotation.row_i)`. It accepts only finite
orthonormal rows (host tolerance1e-4), not scale/shear/singular inputs. This is a **host mathematical
bridge**, not a port of GA's general matrix inversion or its error behavior. Reflections are not
relabelled as native camera rotations by this validation.

`step_camera_dynamics_axis` is a value step, not a stateful camera. It requires current, target,
velocity, min/max, delta seconds, damping base and stiffness; no missing field becomes zero. The
observed constants are `damping=.1`, `stiffness=2.0`, `update factor=.1`, and the host uses `std::pow`
as the counterpart to MSVCR90 `_CIpow`. Native x87 rounding, mode-field mapping, wrap and the other
axis are not claimed identical.

The exact final wrap branch is also exposed as `wrap_camera_angle_once`: it subtracts one explicit
period only when `value > period`, adds one period only when `value < 0`, and preserves equality.
It is intentionally not modulo normalization and does not infer the BSS period. Directional
constraint branches immediately before it remain unported because their `+24/+28/+64` relationships
are mode-state dependent and have not been named from native semantics.

The pose feeds [viewer composition](gge-viewer.md) and the original pixel projection in controlled
tests, including an actual card scene. Normal GGE and controlled preview behavior are unchanged:
no default pose, current-mode pointer, viewer ownership, input dispatch or readiness is fabricated.

## Verification / reproduction

Presentation **130** checks and scene **218** checks PASS: zero/nonzero angles, two rotation signs,
nonidentity basis, separate pivot/offset, negative distance, missing fields, nonfinite/overflow,
rigid inverse order/translation, dynamics decay/acceleration/clamps/wrap/atomic failures and full pose→viewer→card chain. Fixtures supply all camera values;
scene native readiness remains false. [Audit facts](audit/gge-camera-pose/facts.json) include initial
object/table words, inputs, imports and explicit limits; adjacent files retain instruction bytes.

```powershell
python tools/gge_evidence.py --static --camera-pose
cmake --build build --config Debug --target OpenSpore gge_star_present_verify gge_star_scene_verify
```

No target or Ghidra worker ran. Each new objdump subprocess has a 15-second timeout. No new REA
Evidence ID, full camera-controller completion, real catalogue/refill success or playable GGE claim.
