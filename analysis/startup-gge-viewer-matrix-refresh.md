# GGE viewer matrix refresh: view-side preparation, raster projection and publication

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime matrix, renderer frame or gameplay state was
observed. Continuation of [startup-gge-viewer-transform.md](startup-gge-viewer-transform.md).

## `7C4A10` has separate intermediate products

The bounded instruction order is:

```text
7C4A10(viewer)
  → 45E460(view-side matrix helper, viewer-related source, local output)
  → copy 16 dwords into viewer +0x40
  → 7C4370(viewer, local projection destination at +0x80)
  → adjust/swap projection rows using viewer +0x90..+0xAC
  → 45EF90(matrix product)
  → 45E460(helper pass over the resulting viewer-side block)
  → copy 16 dwords into viewer +0xC0
```

The names “view-side”, “projection” and “product” describe their bounded data roles and caller
ordering. They are not claims about a specific SDK class implementation or about the values
being valid at runtime.

## View-side block at `viewer+0x40`

The first `45E460` call is made before the raster projection helper. Its result is copied with
`rep movs` into the viewer-owned 16-dword block beginning at `+0x40`. This is the matrix used as
the left-side input for the later product path; the source/helper's exact inversion and failure
semantics are not promoted beyond the inspected instructions.

`45E460` computes a scalar through `45F100`, checks that scalar for the finite/nonzero branch,
and uses `45E170` in the ordinary branch. The fallback branch performs explicit scalar/vector
arithmetic. This is enough to establish a guarded matrix helper, but not enough to claim that
every runtime input produces a valid inverse or orthonormal camera basis.

## Raster projection block at `viewer+0x80`

`7C4370` receives `viewer+0x80` as its output destination. It reads raster-camera fields at
`viewer+0x170`, including projection kind, window dimensions, offsets and near/far-related
values. It writes a 16-float projection-shaped block and has separate branches for observed
projection kinds `2` and `3`, with a remaining branch for other kinds.

After the helper returns, `7C4A10` reads viewer fields `+0x90..+0xAC`, stores them in temporary
slots, and writes adjusted values back before the matrix product. This row adjustment is a
distinct step from both the initial view-side block and final `+0xC0` publication.

## `45EF90` matrix product

`45EF90` decomposes its two matrix inputs into four rows and calls `45F2C0` for each output row.
`45F2C0` performs the observed four-term multiply/add pattern. In the `7C4A10` caller, the
inputs are the viewer `+0x40` block and the adjusted projection block, while the product is held
in a local 16-float temporary before the next helper pass.

This confirms the product boundary and operand separation. It does not establish a renderer API
submission or a displayed frame.

## Final `viewer+0xC0` publication

The product/helper result is copied into the viewer-owned block beginning at `+0xC0`. Later
`DE3C20 → 7C4250` reads viewer coefficients in the `+0xC0..+0xFC` range for point projection.
Thus the static data route is:

```text
Transform
  → 7C4DD0 / 6B93F0
  → 7C4D10
  → 7C4A10
      → viewer +0x40 view-side block
      → viewer +0x80 raster projection block
      → 45EF90 product
      → viewer +0xC0 projected-view coefficients
  → DE3C20 / 7C4250
```

The existence of a final matrix block is not evidence that its source values were populated by a
live camera, that the viewport is valid, that the renderer consumed it, or that a world/stage
exists.

## Status

Established statically:

- intermediate viewer matrix storage at `+0x40`;
- raster projection-shaped storage at `+0x80`;
- four-row matrix product through `45EF90`/`45F2C0`;
- final coefficient publication at `+0xC0`;
- later projection consumer reads the `+0xC0..+0xFC` range.

Still Unknown:

- exact runtime values and helper failure behavior for all matrix inputs;
- camera-manager acceptance and update scheduling;
- renderer/window validity and frame presentation;
- UI attachment, world/stage creation, Scenario launch and playable gameplay.
