# GGE viewer projection boundary: cViewer arithmetic versus active ICamera

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no runtime camera, viewer, mount or gameplay state was observed.
Continuation of [startup-gge-presentation-dispatch.md](startup-gge-presentation-dispatch.md).

## `DE3C20` acquisition path

`DE3C20` performs the projection boundary used by `DF6010`:

```text
67DCD0 → GameModeManager global 015FD8B0
  → vtable +0x58
  → cViewer*
7C4250(cViewer, world position, temporary output)
805F90(viewport/rectangle output)
  → normalized screen transform
  → write three output floats
```

The first call is visible at `DE3C23..2F`: getter `67DCD0`, then object slot `+0x58`. This is
the viewer path, not the controller-retained active ICamera at controller `+0x198`.

## `7C4250`: matrix arithmetic

`7C4250` reads the input world point and cViewer fields:

```text
viewer +0xC0..+0xFC     matrix/coefficients
viewer +0xD0/+0xD4/+0xD8, +0xE0/+0xE4/+0xE8, +0xEC..+0xFC
```

It computes three numerators from the viewer coefficients and the input point, computes a
homogeneous denominator, divides by it and writes three output floats. This is the native
arithmetic immediately before `DE3C20`'s viewport mapping. It does not acquire a camera,
choose a camera ID or prove that cViewer is populated.

## `805F90`: viewport state

`805F90` obtains a viewport-like object through `67CA60`, copies its four float values through
virtual `+0x34`, and writes them to the caller's 16-byte output. `DE3C20` then uses those values
to map the normalized result. The function's output is an observed viewport contract, not a
runtime proof that a window or renderer supplied valid dimensions.

The arithmetic in `DE3C20` adds the native `1.0` term, multiplies by the normalized coordinates
and writes the result through the caller's two output pointers. `DF6010` passes those outputs
to its vtable `+0x64` consumer, resolved separately as `DF6110`. The projection-to-placement
boundary is documented in [startup-gge-projection-placement.md](startup-gge-projection-placement.md).

## Separate ICamera handoff

The camera-controller path is distinct:

```text
DE46D0
  → GameModeManager +0x50: CameraManager
  → CameraManager +0x38: active ICamera
  → retain at controller +0x198

CameraManager +0x2C / 7C6440
  → active ICamera slot +0x20(delta, cViewer)
```

That path may update the cViewer before projection, but `DE3C20` itself does not call ICamera
or CameraManager. Therefore the static projection edge must not be reported as proof of active
camera selection or successful viewer update.

## Final bounded presentation route

```text
records +0x80
  → DF6010
  → DE3C20
      → cViewer through GameModeManager +0x58
      → 7C4250 coefficient arithmetic
      → 805F90 viewport values
  → DF6110 via vtable +0x64
      → record +0xB8 UI owner
      → root bounds / saved clamp / SetLocation path
```

This is a geometry/UI presentation chain. Unknown remains explicit for viewer production,
active camera selection, renderer/window readiness, native layout attachment, live catalogue
records, Scenario launch and playable gameplay.
