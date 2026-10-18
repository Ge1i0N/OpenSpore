# GGE viewer transform publication: Transform input to cViewer matrices

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime viewer mutation, renderer frame or gameplay state
was observed. Continuation of [startup-gge-camera-handoff.md](startup-gge-camera-handoff.md).

## `7C4DD0` is a viewer-data mutation boundary

The Transform-to-viewer helper has this bounded call sequence:

```text
7C4DD0(viewer, transform)
  → 6B93F0(transform, viewer-owned matrix destination)
  → 7C4D10(viewer, converted matrix)
      → copy/normalize matrix data into viewer storage
      → 7C4A10(viewer)
          → projection/view coefficients and viewer +C0 matrix refresh
```

`7C4DD0` is therefore more than a pointer setter. It converts the supplied Transform into a
matrix-shaped 16-float block and then invokes the viewer's matrix/projection refresh path. The
manager's `+0xB0` field still identifies which viewer is handed to the camera; this function
mutates that viewer's data rather than publishing a new manager pointer.

## `6B93F0`: Transform expansion

`6B93F0` reads the Transform's scalar/vector fields at offsets `+0x10..+0x38` and writes four
four-float rows to the supplied destination. The bounded instructions show scaled basis values,
translation-related words and explicit zero/one terms being materialized. The exact native
Transform type name and semantic handedness are not promoted from this helper alone.

This is an observed conversion operation, not proof that the input Transform came from a live
camera mode. In the GGE path its input is the local output produced by the selected mode's slot
`+4` in `DDFB90`.

## `7C4D10`: viewer matrix storage and refresh

`7C4D10` copies a 16-dword matrix block and then uses the viewer's raster/projection helper at
`viewer+0x170`. It writes derived values into that helper, calls `7C4A10`, and returns. The
bounded code establishes storage and refresh ordering; it does not establish that any renderer
consumed the resulting data.

The call to `7C4A10` is important because projection refresh is inside the Transform handoff,
not a later proof that a frame was rendered. `7C4A10`:

1. obtains an intermediate view/projection product through `45E460`;
2. calls `7C4370` using the raster-camera/projection fields at `viewer+0x170`;
3. combines the resulting data through `45EF90`;
4. stores the resulting matrix at `viewer+0xC0` and related viewer fields.

The broader coefficient layout and matrix order are documented in
[startup-gge-viewer-projection.md](startup-gge-viewer-projection.md) and [gge-viewer.md](gge-viewer.md).
The distinct intermediate blocks and final publication are bounded in
[startup-gge-viewer-matrix-refresh.md](startup-gge-viewer-matrix-refresh.md).

## Relation to camera-manager update

The full static order is now:

```text
CameraManager +0xB0 = cViewer
  → active ICamera slot +0x20(delta, cViewer)
      → camera/controller update
          → selected mode slot 0
          → Transform builder slot +4
          → 7C4DD0(cViewer, Transform)
              → matrix conversion and projection refresh
  → later DE3C20 projection consumer
```

This order describes available native call edges, not a guaranteed per-frame schedule. In
particular, `7C4DD0` success is not a renderer submission, a valid viewport observation, a UI
mount, a world/stage construction result or a playable gameplay tick.

## Status

Established statically:

- Transform input reaches a viewer-owned matrix conversion;
- conversion writes a 16-float matrix-shaped block;
- viewer matrix/projection refresh follows the conversion;
- derived viewer projection data is stored before later `DE3C20` reads.

Still Unknown:

- live mode/Transform values and update scheduling;
- whether the requested camera was accepted and supplied this viewer;
- renderer consumption or displayed frame production;
- native UI attachment, world/stage creation, Scenario launch and gameplay readiness.
