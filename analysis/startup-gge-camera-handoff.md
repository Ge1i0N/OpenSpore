# GGE camera handoff: selected mode, cViewer and active ICamera

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime camera, viewer, renderer or gameplay state was
observed. Continuation of [startup-gge-viewer-projection.md](startup-gge-viewer-projection.md).

## Three separate camera boundaries

The bounded camera route has three distinct stages:

```text
DE46D0 activation
  → request camera ID 780786FD through B5CEF0
  → CameraManager slot +34 / SetActiveCameraByID
  → obtain active ICamera through CameraManager +38
  → retain pointer at controller +198 when empty

DDFB90 selected-camera Update
  → current mode object at global 16A10C8
  → mode slot 0(delta, viewer)
  → mode slot 4(transform builder)
  → viewer transform setter 7C4DD0

CameraManager table 01410718
  → slot +18 / 7C61A0 stores viewer at manager +B0
  → slot +2C / 7C6440 checks active index and viewer
  → active ICamera slot +20(delta, viewer)
```

These are not interchangeable proofs of camera readiness. The first is selection/retention, the
second is camera-controller pose production, and the third is the manager's update handoff.

## Activation selection and retained pointer

`DE46D0` reaches the camera selection boundary as follows:

```text
push 780786FD
call B3D330
mov ecx,eax
call B5CEF0
```

`B5CEF0` obtains GameModeManager, calls its camera-manager getter at `+0x50`, then calls camera
manager slot `+0x34` with the requested ID. At the observed caller, the return from that setter is
not tested. Consequently the static fact is only that the requested ID is submitted to the
manager.

`DE46D0` then checks `controller+0x198`. If it is null, it obtains the GameModeManager camera
manager (`+0x50`), obtains its active camera (`+0x38`), and replaces the retained pointer with
the returned object, releasing the previous pointer when it differs. This establishes a retained
active-camera role, not a proof that it is the requested `780786FD` camera or that the manager
accepted the request.

## Viewer publication and update gate

The camera-manager table at `0x01410718` contains:

| slot | target | bounded role |
|---:|---:|---|
| `+0x18` | `7C61A0` | store supplied viewer at manager `+0xB0` |
| `+0x2C` | `7C6440` | update active camera with manager viewer |
| `+0x38` | `7C6240` | return active camera for nonnegative active index |
| `+0x50` | `7D8510` | mode-side viewer setter/forwarder |
| `+0x58` | `5AADB0` | mode-side viewer getter |

`7C6440` has explicit guards:

```text
activeIndex = manager +0xA8
if activeIndex < 0: return
viewer = manager +0xB0
if viewer == null: return
active = manager->array[activeIndex]
property/revision refresh if needed
active->vtable[+0x20](delta, viewer)
```

The active-camera array begins at manager `+0x80`; `7C6240` returns null for a negative active
index. The update function also consults the active camera property-list/revision fields before
the slot `+0x20` call. This is a native scheduling/update contract, not a rendered-frame result.

## `DDFB90` is pose production, not selection

The GGE camera-controller vtable at `0x0147DA38` stores `DDFB90` at slot `+0x20`. Its bounded
prefix first loads the current mode pointer from `0x016A10C8` and, when non-null, invokes mode
slot `0` with the update arguments. Later it:

1. updates FOV/near/far-related controller/viewer properties;
2. builds a transform input from shared pose/mode state;
3. invokes the mode's Transform-builder slot `+4`;
4. passes the resulting transform to `7C4DD0`.

The mode tables and transform arithmetic are documented in
[gge-camera-pose.md](gge-camera-pose.md). `DDFB90` therefore produces/forwards pose state; it
does not call `SetActiveCameraByID` and does not itself establish that the manager's active
camera update ran.

## Relation to projection

The later projection path remains:

```text
DF6010
  → DE3C20
      → GameModeManager +0x58 / cViewer
      → 7C4250 matrix arithmetic
      → 805F90 viewport values
  → DF6110 UI placement
```

`cViewer` publication, active `ICamera` update, and projection are statically connected by the
manager's fields and calls, but none of these instructions proves that a live viewer was
produced, that the requested camera was accepted, that a renderer supplied a valid viewport, or
that a stage/world exists.

## Status

Established statically:

- requested GGE camera ID and selection call path;
- separate controller-retained active-camera pointer;
- viewer storage at CameraManager `+0xB0`;
- active-index/viewer guards before `ICamera` slot `+0x20`;
- selected-mode update and Transform-to-viewer path.

Still Unknown:

- resource registration and runtime camera-manager population;
- selection return value and actual active-camera identity;
- update scheduling and successful `ICamera`/viewer mutation;
- renderer/window validity and projection-frame production;
- native UI attachment, world/stage construction, Scenario launch and playable gameplay.
