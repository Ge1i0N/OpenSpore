# GGE per-star UI — projection, placement and configuration subset

Reviewed **2026-10-15**. Static GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No original runtime observed. Controlled camera inputs + real SPUI corpus, not a rendered galaxy.

## Implemented boundary

`engine/gge_star_present.*` ports the bounded calculations immediately before per-entry UI placement:

- `0x7C4250 / 0xDE3C20`: world position + observed camera matrix → screen point;
- `0xDF6110`: screen point + observed root size/current position → centered/saved-clamped position;
- `0xDF9200`: saved stage/header status → stage-icon index, locale-caption and raw flag requests.

All observations are explicit. Missing matrix/window/root/current position/saved margins/global
stage-text flag blocks the relevant helper. There is **no** implicit identity camera, fabricated
viewport/margins, forced Intro completion or source-readiness signal. Identity matrices are used
only in controlled tests. The host does not instantiate per-record layouts, run original image-list
plugins, apply flag requests or create camera/galaxy/effect objects yet.

**Screen appearance is unchanged.** The visible GGE primary layout is still diagnostic. This moves
the implementation toward the visual layer without presenting an artwork-only/synthetic galaxy
as recovered runtime behavior.

## Confirmed default asset and controls

Initializer `0x136BC90` assigns wide string `GGEGameStarUIv4` at `0x147D3C0` to runtime BSS string
`0x16A0D08`. Default instance is **`0x21DF01F2`**, distinct from the sparse primary GlobalUI.
Runtime configuration may override the name; that override/mounting path is not implemented.

Both real corpus resources (`PatchData.package`, `Spore_Graphics.package`) parse as **24,812 bytes,
152 objects, 58 windows**. Their payload SHA-256 values differ; the test checks both, not an assumed
byte identity or original priority rule. The verifier finds:

| Role at the inspected call sites | ControlID |
|---|---|
| positioning root (`0xDF6110`) | `0x02E5A4A8` |
| two stage image-list controls | `0x026E0001`, `0x026E0002` |
| stage caption | `0x03C4925E` |
| status-3 flag targets | `0x060A1102`, `0x060A1103`, `0x0375408C`, `0x05944088` |

Controls are lookup targets within each mounted instance, not globally unique objects. Their
presence does not establish event behavior or reproduce the native mount `0xDF7690`.

## Projection formula

The matrix is copied from camera memory **+0xC0..+0xFC**, in memory order `m[0..15]`:

```text
w = m11*z + m7*y + m3*x + m15
ndcX = (m8*z  + m4*y + m0*x + m12) / w
ndcY = (m9*z  + m5*y + m1*x + m13) / w
ndcZ = (m10*z + m6*y + m2*x + m14) / w
X = window_width * (ndcX + 1) * 0.5
Y = (1 - (ndcY + 1)*0.5) * window_height
```

`0xDE3C20` uses **extents only**, not window-origin addition. It does not clip offscreen values or
cull negative `w` here. The host follows those facts, but rejects zero/nonfinite `w`, nonfinite
inputs/results and nonpositive/invalid extents as explicit safety policies. It does not claim
instruction-level x87/SSE rounding parity. `ndcZ`/`w` remain diagnostic values, not visibility flags.

## Root placement and stage configuration

`0xDF6110` subtracts half root width/height from the projected point. Saved entries additionally
clamp X to `[observed_left, window_width-observed_right]` and Y to
`[observed_top, window_height-observed_bottom]`. New entries are not clamped as saved entries.
The margins reside in BSS (`0x16A0E98/EA8/EB8/EC8`); no raw-file reads or guessed defaults are used.
Movement is requested only when **either axis differs by strictly more than 4 pixels**, constant
`0x148531C`. Inverted/nonfinite clamp bounds are rejected by host policy.

For saved entries, stages **1,2,4,5 → indices 1,2,3,4**; other stage values use index 0.
If observed global `0x16A0F88` is false, caption table is `0x2B7A0311`; stage 5 uses key
`0x067735D5`, otherwise `0x067735D4`. Unknown global does not silently choose either branch.

Save-status **3** alone requests these raw native flag calls:
`(060A1102,1,true)`, `(060A1103,1,true)`, `(0375408C,2,false)`, `(05944088,1,false)`.
The helper deliberately does not relabel those masks as guessed enabled/visible semantics.
Successful calculation is a plan, not executed UI/plugin/locale mutation or gameplay readiness.

## Checks and continuation

`gge_star_present_verify`: **50 checks PASS**, binding both real layouts and all needed controls;
projection center/axes/nonzero rectangle origin/asymmetric matrix/perspective/negative-w/offscreen,
invalid/unknown/atomic failure, saved/new clamp distinctions, exact 4px deadband, stage/caption/status
branches and raw flag operations. It does not render a live scene or replay the original game.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_present_verify
.\build\Debug\gge_star_present_verify.exe <SPUI-fixture-directory>
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --present
python tools/gge_evidence.py --static --present
```

[Evidence](audit/gge-star-present/facts.json): eight GA functions, three instruction extracts,
constants/limits and real layout provenance. Next: recover camera/scene creation and per-entry mount
ports, then use genuinely available [catalogue sources](planet-record.md) to draw/select entries.
Candidate generation and MoviePlayer/DBCheck ports remain prerequisites of original state-2 flow.

Final Debug x86 build warning-free; all calculation/parser/catalogue/DBCheck/Intro/binding/SPUI/
package/scissor suites PASS, six Python tests and 50 maintained Markdown link checks PASS.
Live `-w -devDirs` Main → GGE and launcher → CreatureEditor both exit 0, aborts/asserts 0.
MoviePlayer stays Unknown; no star instances or galaxy scene were synthesized during these runs.
