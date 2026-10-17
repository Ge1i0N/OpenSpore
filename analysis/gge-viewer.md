# GGE viewer projection and camera-controller handoff

Reviewed **2026-10-17**. GA authority SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static instructions/tables, SDK-assisted names and controlled host tests; no native runtime capture.

## cViewer is not ICamera

The pinned [ModAPI reference](community-references.md) supplies names corroborated by actual GA
vtable entries, constructor/factory writes and getter instructions:

| Edge | GA target / storage | SDK-assisted meaning |
|---|---|---|
| `67DCD0` | global `015FD8B0` | `IGameModeManager::Get`, not renderer getter |
| mode table `01412608 +50` | `967D90`: return `[this+2C]` | `GetCameraManager` |
| mode table `+54` | `7D8510`: store `[this+30]`, forward to CameraManager slot18 | `SetViewer` |
| mode table `+58` | `5AADB0`: return `[this+30]` | `GetViewer` |
| camera table `01410718 +38` | `7C6240`: index array+80 by active index+A8, null for negative index | `GetActiveCamera` |
| camera table `+2C` | `7C6440` | update active camera with viewer |

Mode factory `7D8E80` installs `01412608` and initializes separate fields+2C/+30; camera constructor
`7C7680` installs `01410718`. Getters such as `967D90` are shared by unrelated classes too, so their
addresses alone do not establish type; table/constructor/consumer checks are used together.

```text
DE3C20 ->67DCD0 ->GameModeManager.GetViewer(+58) ->cViewer*
                                                   ->7C4250 reads viewer+C0..FC
DE46D0 ->67DCD0 ->GameModeManager.GetCameraManager(+50)
                ->CameraManager.GetActiveCamera(+38) ->ICamera* retained at controller+198
```

The earlier phrase "not proven the same camera" was too vague: these are **different object roles
and storage paths**, not alternate acquisitions of one raw camera pointer. The controller may retain
an ICamera that drives the viewer, but ICamera+C0 is not the projection matrix.

`7D8510` forwards the viewer to CameraManager slot18; `7C61A0` stores it at manager+B0.
`7C6440` requires active index>=0 and a non-null viewer, obtains the active ICamera, checks its
property-list revision, then calls **ICamera slot20 `(delta,viewer)`**. SDK names this `Update`.
This resolves the static handoff, not the live GGE camera type/ID, its chosen properties or scheduling.

## Implemented: explicit viewer matrix composition

`compose_viewer_projection` in `source/engine/gge_star_present.*` implements the arithmetic boundary
before `7C4250`, without copying SDK implementation code:

1. `7C4370` builds projection from raster-camera+6C kind, +5C/+60 view window, +54/+58 offsets,
   and +70/+74 near/far. The raster-camera is referenced by **viewer+170**, not controller+198.
2. `7C4A10` exchanges the second/third four-float rows, calls `45EF90` with **viewer+40 on the
   left and adjusted projection on the right**, then copies the result to **viewer+C0**.
   `45F2C0` corroborates row-vector multiplication terms/order.
3. Existing `project_star` consumes the resulting memory-order matrix and maps to pixels.

The helper receives viewer+40 explicitly. Native `7C4A10` first obtains it by inversion of viewer+00;
inversion and concrete ICamera transform generation are not ported here. Neither matrix nor FOV/aspect
is synthesized. The controlled preview does not use this helper to fabricate production state.

Let `sx=1/wx`, `sy=1/wy`, `n=near`, `f=far`, offsets `ox,oy`. Before exchanging rows1/2 (zero-based),
the zero-initialized projection has these four-float rows in memory order:

```text
Perspective (kind1):               Parallel (kind2):
 sx    0     0          0           sx    0     0          0
  0   sy     0          0            0   sy     0          0
 sx*ox sy*oy f/(f-n)    1            0    0    1/(f-n)     0
  0    0    -f*n/(f-n)  0           sx*ox sy*oy -n/(f-n)   1

PerspectiveInfinite (kind3):
 sx    0     0          0
  0   sy     0          0
  0    0     1          1
  0    0    -n          0
```

The infinite branch **drops offsets**, rather than reusing finite perspective's terms. The actual
implementation computes `q=f/(f-n)` then `-q*n`, preserving that recovered division/multiplication
order. No bit-identical SSE/x87/FMA rounding parity is claimed.

Host policy requires every input, supported kind, finite matrices/offsets, positive windows, near>0
and far>near, even though native infinite projection does not use far. These are conservative bounds,
not recovered native rejection rules. Invalid/overflowing input leaves output unchanged. A valid
matrix is not proof of viewer identity, native update completion or scene readiness.

`SceneCameraSource::ViewerProjection` is the clearer host provenance name. `ActiveProjection` remains
a source-compatible alias with the same value, not an observed native pointer. `ControllerRetained`
still cannot substitute for viewer projection; failed updates still retire prior draw/pick frames.

## Verification and remaining work

- Presentation **77 checks PASS**: original projection/placement plus literal matrices for all three
  kinds, noncommuting translation/asymmetric product order, two-stage projection, unknown/invalid
  inputs, infinite-offset distinctions, overflow and atomic failure.
- Scene **212**, input **29** checks PASS on actual SPUI/PNG with controlled metadata/camera inputs.
  Scene tests explicitly publish a frame from composed viewer coefficients while keeping native
  readiness false; the legacy provenance name remains a checked alias.
- Follow-up [camera-controller investigation](gge-camera-controller.md) identifies the **static** GGE
  factory/type/requested ID and one complete shipped prop. It adds a bounded FOV-step helper;
  runtime selection success, pose/mode state, native viewer lifetime/update scheduling,
  render/effects services and complete real catalogue remain unresolved.

[Audit facts](audit/gge-viewer/facts.json) retain tables, routes and limitations; accompanying
instruction files retain original byte columns. Reproduce:

```powershell
python tools/gge_evidence.py --static --viewer
cmake --build build --config Debug --target OpenSpore gge_star_present_verify gge_star_scene_verify gge_star_input_verify
```

Each objdump subprocess has a 15-second timeout. This pass uses retained Ghidra views and bounded
file-byte disassembly, not a Ghidra import/reanalysis or new REA native Evidence IDs. No target ran.
