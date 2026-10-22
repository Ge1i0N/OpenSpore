# GGE projection output and per-record UI placement boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime projection, layout mount, renderer frame or
gameplay state was observed. Continuation of [startup-gge-viewer-matrix-refresh.md](startup-gge-viewer-matrix-refresh.md).

## `7C4250`: homogeneous projection arithmetic

`7C4250` reads the input point and viewer coefficients in the `+0xC0..+0xFC` range. It forms
three numerators and a denominator, then executes:

```text
scale = 1.0 / denominator
out0 = numerator0 * scale
out1 = numerator1 * scale
out2 = numerator2 * scale
```

The denominator divide at `0x7C433E` has no local zero/nonfinite guard in the bounded function.
The function writes three floats to the caller's output pointer and returns. `w`/depth is part of
the arithmetic contract, not a native visibility decision at this boundary.

## `DE3C20`: viewport mapping, not clipping

`DE3C20` obtains cViewer through GameModeManager slot `+0x58`, calls `7C4250`, obtains the
viewport/rectangle values through `805F90`, and maps the normalized values to two output floats.
The observed operations include the `(+1)` and half-scale terms, followed by multiplication by
viewport extents.

The bounded function has no branch that rejects negative `w`, offscreen normalized coordinates,
or nonfinite arithmetic. It also uses viewport extents in the mapping; a window-origin addition
is not present in this function. Therefore:

```text
matrix point
  → 7C4250 normalized/projected values
  → DE3C20 extent mapping
  → caller output floats
```

This is a projection result, not proof of visible geometry, renderer acceptance or a displayed
frame. Host-side finite/zero-denominator validation remains an explicit safety policy rather than
a claim about an observed native rejection branch.

## `DF6010`: record loop and dispatch

`DF6010` computes the record count from its `[+0x08,+0x0C]` span, iterates records with stride
`0xBC`, reads each position at `record+0x80`, and calls `DE3C20`. It then dispatches the two
projected values through the owning object's vtable slot `+0x64`, with the record pointer as the
receiver-side context. The concrete GGE owner maps this slot to `DF6110`.

The loop therefore separates projection production from per-record UI placement:

```text
record +0x80
  → DE3C20
  → projected x/y
  → owner vtable +0x64 / DF6110
```

## `DF6110`: layout-local centering and optional clamp

`DF6110` first loads the per-record UI owner from `record+0xB8`; a null owner returns without a
placement call. It looks up the positioning root using control ID `0x02E5A4A8`, obtains viewport
and root geometry through the UI object's virtual getters, and subtracts half the root dimensions
from the projected point.

The branch at `record+0xAC` selects whether the observed margin/clamp calculations are applied.
When that branch is active, the function combines current/root geometry with global margin values
at `0x016A0E98`, `0x016A0EA8`, `0x016A0EB8` and `0x016A0EC8`, then chooses bounded coordinates.
These are layout coordinates and serialized/runtime UI state, not camera visibility flags.

Finally, the function compares the desired coordinates with the current UI position using the
native threshold at `0x0148531C`. Only when the observed strict-difference tests pass does it
call the owner vtable slot `+0x64` with the two float coordinates. The call is a UI placement
operation; its success does not establish that the UI is mounted or rendered. Lookup versus the
separate layout attachment path is detailed in
[startup-gge-ui-lookup-attachment.md](startup-gge-ui-lookup-attachment.md).

## Boundary result

The static presentation route is:

```text
viewer +0xC0..+0xFC
  → 7C4250 homogeneous arithmetic
  → DE3C20 viewport extent mapping
  → DF6010 record iteration
  → DF6110 per-record UI owner
      → root centering
      → optional saved/layout clamp
      → strict movement threshold
      → vtable +0x64 placement call
```

No native clip/cull result, renderer visibility, UI mount success, stage/world construction,
Scenario launch conversion or playable gameplay tick follows from this chain.

## Status

Established statically:

- denominator-based projection arithmetic;
- absence of local clip/cull branches in `7C4250`/`DE3C20`;
- record stride/position source and vtable placement dispatch;
- per-record UI owner guard;
- layout root, optional margin/clamp branch and movement threshold before placement.

Still Unknown:

- runtime matrix/viewport values and projection success;
- native UI mount and callback success;
- renderer consumption or visible-frame production;
- world/stage creation, Scenario action execution and gameplay readiness.
