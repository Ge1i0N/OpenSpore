# GGE camera controller — factory, exact prop and bounded FOV step

Reviewed **2026-10-17**. Static GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`;
one exact shipped property resource, SDK-assisted interface names, controlled tests. No live capture.

## Factory and selection are now identified statically

`DDDF80` calls GameModeManager slot50 (`GetCameraManager`) and CameraManager slot20
(`AddCameraType`) with **type `043C6E06`, factory `DDFE70`**. The historical community registry
names the type `kGGECameraControllerType~`; this name is secondary, while the operands are GA bytes.

`DDFE70` allocates **0x44** bytes and calls constructor `DDF940` with its property-list argument.
The constructor retains that list at +10, installs table **0147DA38**, and invokes `DDE9F0` when
the list is non-null. This table has Cast at +0C (`DDE390`), Update at +20 (`DDFB90`), property-list
getter +4C (`7F55C0`), and Initialize +50 (`DDE9F0`). Cast accepts `07BA65F3` and `EE3F516E`.
Do not substitute the factory type for a Cast/interface ID.

During GGE activation, `DE477A` requests **camera ID `780786FD`** through `B5CEF0`.
`B5CEF0` forwards it to CameraManager slot34 (`SetActiveCameraByID`). `DE478B` then obtains the
active ICamera and retains it at controller+198 if that field was null. **The selection return is
not checked at this call site.** Therefore static code establishes the requested camera, not proof
that a live manager accepted it or that the retained object has this exact type.

The generic camera parser `7C6C20` reads property **00ED3928**, finds its factory, optionally reads
name **00ED3929**, calls that factory with the property list and puts its result into CameraManager
through slot24. This connects registration to the resource, rather than inferring a factory from a
similar filename. Runtime resource priority, camera parsing/order and overrides remain unobserved.

## Exact shipped GGECamera property table

The independent exact-resource reader found this resource in explicitly selected
`build/bin/Debug/Data/Spore_Graphics.package`:

- record **22953**, key `(instance,type,group)=(780786FD,00B1B104,40410100)`;
- decoded bytes **531**, **39 fields**, exact consumption;
- SHA-256 **`432605863c03b04a90bda5d5d9f3dc4d58fcdcf2dc64245f5f6dfefa2c2fabbb`**;
- description **`GGE camera`**, camera name **`GGECamera`**, camera type **`043C6E06`**.

The source's scalar floats are BE; composite values retain their own original byte order. Some
particularly useful observations:

| Property | Value | Corroborated consumer |
|---|---:|---|
| `01102B20` (registry: cameraNearClip) | 5 | `DDF0AF -> controller+38`, `DDFD27 -> viewer near setter7C4C70` |
| `01102B2F` (cameraFarClip) | 22000 | `DDF0DF -> controller+3C`, `DDFD4A -> viewer far setter7C4C90` |
| `4A2F63FE` | 3 | `DDEB1D -> controller+18` (do not label it a FOV) |
| `00C7C4FB` | 1090 | read twice for shared state in `DDEBxx..DDECxx`; full pose interpretation pending |
| `00C7C4FC` | -33 | initial orientation-related shared state; no direct viewer matrix claim |
| `00C7C4FD` | 0 | separately read orientation-related shared state |

The camera asset is **not** a ready projection matrix. `DDE9F0` consumes many fields into shared
globals and object fields; not all roles, defaults and mode transitions are reconstructed. In
particular, the selected prop is not evidence of an already-observed runtime FOV/pose.

The first inventory tool rejected the >512MiB EP1 package, so a tool-local index-first reader was
added: `tools/gge_camera_property.py`. It bounds index/record reads, checks exact key uniqueness,
version/index/flags/size and RefPack declared decoded size, and reads only the selected payload.
The explicit PatchData/Game/EP1_Data directories had no instance/type match during the focused
comparison; Graphics supplied the exact match above. This is not an all-package priority claim.
No recursive package discovery or profile/save access is performed by the new reader.

## Implemented arithmetic: FOV request before smoothing

`step_camera_fov` accepts **explicit** current(+28), target(+2C) and snap threshold(+34). The
recovered finite-input branch from `DDFB90` is:

```text
if OLD current > 0:
    request viewer FOV = OLD current * 57.295780181884766

if current == -1 and target > 0: next_current = target
else if target == -1 and current > 0: next_current = current
else if abs(target-current) < threshold: next_current = target
else: next_current = current + (target-current)*0.10000000149011612
```

The **strict** `<` threshold and old-current request order matter. The smoothing factor is not
multiplied by deltaTime in this final branch; other camera work earlier in Update does use delta.
`-1` is an observed sentinel, not an omitted/Unknown value. Zero is neither Unknown nor a positive
request. Nonfinite/overflow and negative-threshold rejection are explicit host safety policy, not
native error semantics. Failure leaves the caller's output unchanged.

The helper returns a request/value plan only. It does not mutate viewer, controller, BSS, input,
audio or scene state and is not automatically attached to normal GGE/preview. Full Update also calls
the current global mode object through `16A10C8`, works with pose globals `16A10CC..`/`16A10E0..`,
invokes service/audio calls, constructs a Transform and passes it to viewer setter `7C4DD0`.
That pose/mode/transform path remains the next substantive reconstruction boundary.
Follow-up [pose investigation](gge-camera-pose.md) now identifies the three dispatch tables and ports
the common orbit-transform arithmetic plus an explicit rigid inverse bridge. Full mode dynamics,
runtime state/selection and automatic normal-GGE camera publication remain unresolved.

## Verification and reproduction

- Presentation **85 checks PASS**: prior viewer/projection subset plus old-current ordering,
  sentinel branches, strict threshold, zero, unknown, invalid and overflow/atomic FOV cases.
- Python **48 tests PASS**, including four index-first exact camera reader tests.
- Application/changed verifier Debug x86 build passes without warnings.
- No Ghidra worker/import or original target process was started. Primary static bytes were read
  through bounded objdump subprocesses (15s each); retained analysis is not new REA execution evidence.

```powershell
python tools/gge_evidence.py --static --camera-controller
cmake --build build --config Debug --target OpenSpore gge_star_present_verify gge_star_scene_verify
```

[Audit](audit/gge-camera-controller/facts.json) retains factory/table facts, constants, all39 parsed
property fields with raw bytes and original instruction extracts. No native camera/render/catalogue
readiness, terrain/refill result or MoviePlayer/DBCheck completion follows from this step.
