# GGE star UI instances — owned host graph and native mount boundary

Reviewed **2026-10-15**. Static GA evidence, controlled tests and real SPUI fixtures.
No original-runtime replay, native UI mount completion or rendered galaxy claimed.

## Implemented host mechanism

`engine/gge_star_instance.*` owns a decoded-window graph per caller-issued nonzero instance ID.
Each entry retains private metadata/template copies; moving/changing one cannot mutate the source
or a sibling with the same star ID. The class is non-copyable. It owns no native listeners/effects.

Supported operations: root/descendant movement, effective ancestor visibility/enabled state,
explicit object-index flag and locale-caption-reference mutations, cleanup/reinstantiation and
pixel-space picking with inherited Clip. Mutations are failure-preserving; a failed reinstantiate
does not discard the prior graph. Picked-window pointers expire on successful mutation or clear.

`states::draw_spui_pixel_layer` is a renderer hook: it uses the launcher's shared paint path at
1:1 pixels inside an already active D3D/Overlay frame. No clear/begin/end, design-resolution
rescaling, click dispatch or state transition; per-layer scissor resets afterward. The full-screen
presenter retains its scaling and behavior. **GGE does not create these instances or call this hook
yet.** Full-screen renderer regression is tested, not a new native/per-star scene pixel-readback.

Raw `Layout::objects` remain original provenance. Window mutations are **host overlays**, not
rewritten SPUI serialization or a claim that native WinProcs updated every raw property.

## Real duplicate ControlIDs

Both `GGEGameStarUIv4` fixtures (152 objects / 58 windows) contain **two distinct objects** for
ControlID `0x026E0002` (serialized object indices **45 and 55**), plus other repeated IDs. Positioning
root is object **177**; sparse object indices are not `windows` vector offsets or game IDs.
[Corpus provenance / duplicate map](audit/gge-star-mount/control-identity-corpus.json).

`control_objects` returns all matches, hidden included. Mutators require an explicit **object index**;
they never collapse duplicates or choose the first ControlID. Native `0x8102A0` enumerates loaded
objects then calls recursive window slot `+0xF0`; its dynamic ordering/WinProc changes are not
established merely by serialized order. No native-priority assumption is introduced.

## Native mount trace — not implemented as ready

1. `0xDF7690` resets local phase, detaches tooltip userdata `0x037AC490` where present, and unloads
   prior UI via `0x811BC0`.
2. `0x812250` requests the layout with tag **`0x0510A95B`**, group **`0x40464100`**, parent lookup
   true, mount **`0x05B598FA`**. Native service tag is distinct from package SPUI type `0x250FE9A2`.
3. `0x8121C0 → 0x812100 → 0x811C20` loads native objects and parent-attaches their roots.
4. `0x80F7D0` allocates/retains a layered object, stored in game-ui data at `+0x8C`.
5. A true load invokes `0xDF6D80` phase1: more controls, thumbnail/main images, stage image plugins,
   locale/name/profile/current-planet state and notifications. Some pseudocode stack/loop recovery
   is incomplete; instructions, not analyst names, govern this dependency trace.
6. Listeners for `0x03753F40`, `0x04656452`, `0x046A9CA7`, `0x0638491D` attach after the caller's
   phase1 return test. **Correction:** all inspected `0xDF6D80` normal returns set AL=1, even when
   root lookup skips its body; no false/error dependency-validation path is established. `0xDF7690`
   returns load result; neither return proves native readiness. [Detailed corrected bindings](gge-star-phase1.md).

`native_mount_complete()` therefore always returns **false** in the host mechanism. Instantiation
does not execute phase1, callbacks, plugins, tooltip linkage, camera/effects or force hidden branches.
The [image follow-up](gge-image-bindings.md) now supports explicit CF3C4EAE image get/set overrides
inside one instance, including shared drawable identity. It does not mark phase1 or native mount complete.
The [Animated Icon follow-up](gge-animated-icon.md) adds separate per-window static frame/atlas bindings,
with explicit object selection, constructor1 and invalid-index no-op results; no playback/mount completion.
The [host lifecycle owner](gge-star-lifecycle.md) now joins independent action application/retry with
generation tickets, exact UTF text and floor/local centering; it still does not mount a native card.

## Separate scene/camera setup

`0xDE46D0 → 0xDDD920` allocates a `0x74` Simulator object via `0xDDA8C0`, global `0x16A0B08`,
then calls `0xDDD150`. Factories come from `0x67DD40/0x67DD50`; render stages register through
`0x67DD10`. Exact factory semantics remain partly unresolved; no host substitutes are synthesized.

`0xDE46D0` separately requests effects world **`0x02DDA976`, "GGE effects world"** through
`0x67DD90`, controller `+0x15C`. Controller `+0x198` is acquired through renderer `0x67DCD0`
slot `+0x50`, then that returned interface's slot `+0x38`. It is not assumed identical to the active
projection camera at renderer slot `+0x58`.
The [scene follow-up](gge-star-scene.md) retains those distinct service globals/call paths and now
joins explicit catalogue/camera observations to host placement/layers/pick. Native factories/effects
and automatic GGE mounting remain unresolved; no hidden panel is forced visible by the collector.

## Validation / reproduction

Host limits: one root, 4096 windows, 65,536 retained objects, 4096 UTF-16 units per entry text.
Window object indices must be unique/non-null; duplicate **ControlIDs are legal**. Parent/Children
must agree, every node is reachable once, and coordinates/extents remain finite. Supported host
flags are visible/enabled/IgnoreMouse/Clip (`0x413`); unknown bits are rejected. These safety and
forward-paint/reverse-pick policies do not reproduce all native malformed-input behavior or dynamic Z.

`gge_star_instance_verify`: **167 checks PASS**. Two real assets, every descendant translation,
source/sibling isolation, duplicate-ID targeting, captions, move suppression, clear/reinstantiate,
pixel picking without rescaling, overlapping/disabled/IgnoreMouse/Clip/viewport cases, invalid
graphs/host limits and atomic failures. Controlled graphs are not original-runtime observations.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_instance_verify
.\build\Debug\gge_star_instance_verify.exe <SPUI-fixture-directory>
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --mount
python tools/gge_evidence.py --static --mount
```

[Evidence](audit/gge-star-mount/facts.json): 17 GA functions, scene-init extract, service boundaries
and actual duplicate IDs. Next: phase1 image/plugin/callback ports, scene factory contracts and
source-to-instance lifecycle, then real GGE drawing/selection. Unknown is not empty or ready.

Final Debug x86 build warning-free; 167 instance checks and all prior projection/parser/catalogue/
DBCheck/Intro/binding/SPUI/package/scissor suites PASS. Six Python tests and 51 maintained Markdown
link checks PASS. After the shared-painter refactor, a new 1280x720 launcher capture is pixel-identical
to `analysis/shots/spui_e3launch_uv.png` (Pillow RGB diff bbox=None). This verifies the existing
full-screen path, not per-star/native-scene rendering. Launcher capture, Main → GGE and visible
CreatureEditor click with `-w -devDirs` all exit 0, aborts/asserts 0; MoviePlayer stays Unknown.
