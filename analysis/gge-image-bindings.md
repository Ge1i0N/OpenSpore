# GGE images — actual provider, instance overrides and renderer path

Reviewed **2026-10-15**. Authority: GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Original code is static evidence; actual shipped PNG/SPUI checks and controlled GPU tests are host
observations, not original execution or a working GGE galaxy.

## Implemented outcome

`engine/gge_image_provider.*` implements an explicit PNG provider and applies **one resolved image
request** to an owned [star instance](gge-star-instance.md). This is actual mutation, not only a
[phase-1 plan](gge-star-phase1.md). The drawable resolver and pixel-layer painter consume the
replacement pixels. It does not run the rest of phase-1, native listeners, Animated Icon stage
plugins, camera factories, catalogue discovery or original state transitions. **No live GGE caller
instantiates/updates these images automatically yet; the normal GGE screen is unchanged.**

## Original setter/getter boundary

- `0x97D030` implements QI `0xEF3C47CF` for class **`0xCF3C4EAE` ImageDrawable**, returning `this+0xC`.
- Constructor `0x97D840` installs image-interface table **`0x14431E8`**.
- Its setter slot `+0x14 → 0x97D070` compares the image pointer, retains the replacement, releases
  the old image and assigns interface-local `+0x18` (= drawable `+0x24`).
- Getter slot `+0x18 → 0x6C0230` returns that stored image, not an arbitrary reachable image.
- `0xDE3EB0` checks the window's fill drawable/interface, uses a supplied shared image or loads
  the explicit ResourceKey through `0x806320`, then invokes setter + window invalidation.
  Failure does not itself clear the previous image. Native refcounts/invalidation are not executed
  by the host, which uses shared immutable pixel ownership and derives a fresh quad each draw.

Only the plain ImageDrawable path is implemented by this executor. Other classes, including Animated Icon
`0x0106F14B`, explicitly return **Unsupported**. No attempt is made to treat Animated Icon's
`0x0106F146` stage-index interface as the image-drawable interface or to select a different duplicate
control by class. [Evidence](audit/gge-images/facts.json) retains table entries and exact setter calls.
The separate [static Animated Icon selector](gge-animated-icon.md) now handles explicit index and
atlas-image state through a different API; the image executor's supported class set is unchanged.

## Explicit provider and availability

`ExplicitImageProvider` opens **one caller-selected DBPF**, reusing the existing strict
[DBPF 3.0/index3 adapter](starsdb-dbpf.md): no installation/profile search, directory scan, priority
merge or writes. A different package is not silently searched on failure. PNG resource identity is
exact `(instance,type,group)` with type `0x2F7D0004`; zero-instance/non-PNG keys are Unsupported.

Results distinguish **Unknown, Found, Missing, Ambiguous, Unsupported, ReadFailure**. Found requires
actual PNG decoding to BGRA, matching dimensions and byte count, not just an index hit. The provider
checks PNG signature/IHDR and dimensions **1..4096 per axis before GDI+ decode**. Existing package
256 MiB/resource64 MiB policies remain; this provider does not claim the whole installed asset corpus
fits that deliberately narrow explicit-package subset.

The returned `spui::ImageBinding` owns a shared const `ImagePixels` object: exact resource triple,
dimensions, BGRA and explicit package/record provenance, plus atlas UV and logical dimensions.
The provider itself is not a resource cache or an implementation of original package precedence.
The separate [file-backed provider](gge-streaming-images.md) now supports bounded direct reads from
large original packages; it shares this decoder without changing the old memory adapter's limits.
The helper reuses the existing GDI+ decoder; it is not a complete hardened PNG-format validator.

## Application, isolation and sharing

`apply_star_image` accepts only ImageResource/StageImageTarget operations and checks caller-issued
instance identity, target object/control mapping and supported fill drawable **before provider I/O**.
An unresolved duplicate target/source never becomes first-match. Errors leave the previous instance
binding unchanged; no blank texture or first-reachable template fallback is installed.

The override is stored in `Layout::runtime_images`, keyed by **drawable object identity**. Thus windows
sharing a drawable share its replacement within one instance, consistent with native setter scope;
other instances and the serialized template remain unaffected. Raw `Layout::objects` are not rewritten
and the host map is not serializable native state. Reinstantiation/clear drops runtime overrides.

Stage-image transfer supports two observed cases:

1. Getter descriptor already has decoded pixels: share the image, preserve its UV/logical size and
   **do not call the provider**.
2. Getter has a supported serialized Image/resource descriptor but no pixels: load that exact resource,
   then preserve the original atlas UV/logical size instead of overwriting them with PNG dimensions.

Absent/unsupported getter shape is **blocked**, not an observed native null image. The native
zero-key fallback for null image is retained in the request but not synthesized as a loaded image.
This conservative boundary remains until the original runtime getter/null state is represented.
Resource Found lacking matching valid pixels is rejected even if a backend incorrectly reports it.

Replacing an image does not replace tiling/scale/alignment. `spui::drawable_quad` reuses the original
None-tiling ImageDrawable geometry and fixed-axis rules; unsupported modes remain unsupported.
`ImageQuad::pixels` bypasses external-table/global first-key lookup. No runtime override may fall
back to the old template art just because its new pixel payload is unavailable.

## Rendering and ownership

The shared SPUI painter uploads runtime pixels directly, caching by immutable image identity rather
than ResourceKey alone. Entries hold weak pixel references, retire expired entries on runtime-image
use, and own their D3D textures with RAII. Different explicit packages using the same key cannot
accidentally share the old first-key texture. The existing static-resource texture path is unchanged.
No per-frame decode occurs when already supplied pixels are transferred/shared.

This does not establish native invalidation/WinProc ordering, GPU-budget policy across arbitrarily
many active images, or device recreation behavior beyond existing renderer contracts. Current image
limits are per-provider/asset, not a claim of globally bounded total asset memory.

## Verification

`gge_image_verify`: **80 checks PASS** on both actual 152-object/58-window layouts and an actual
PNG from explicitly selected `PatchData.package` (record94 in this corpus, **128×128**).

- Actual PNG → GDI+ BGRA → real CF drawable override → resolver; exact provenance and isolation.
- Constructed DBPF failure cases: duplicate/missing key, corrupt/truncated PNG and oversized IHDR.
- Controlled backend Unknown/failures/dishonest Found, wrong instance/object/interface operation,
  unresolved duplicates, unsupported Animated Icon, invalid UV/pixel count/logical size, provider
  reentry clearing the instance safely (no borrowed-window use across a provider call).
- Shared/unloaded getter transfer, atlas preservation, shared-drawable scope, absent raw image link,
  fixed-axis alignment and reinstantiate cleanup.
- Real D3D9 readback draws a **controlled solid-red2×2 replacement** through the same resolver quad
  and overlay; pixels inside/outside the scissor are asserted. This proves the host pixel path,
  **not that the sampled actual PNG, a whole star UI or a GGE galaxy was rendered in that readback**.

```powershell
cmake --build build --config Debug --target OpenSpore gge_image_verify
$capture = Join-Path 'C:\Users\DartPower\AppData\Local\Temp\opencode' ('GGE-image-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\gge_image_verify.exe <SPUI-fixture-dir> .\build\bin\Debug\Data\PatchData.package $capture
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --images
python tools/gge_evidence.py --static --images
```

Evidence: seven functions (C + instructions), interface table/facts, two bounded extracts and
[setter seed](audit/gge-image-seeds.tsv). Next: join explicit native-like image results and other
phase-1 operations into a real instance lifecycle, implement Animated Icon semantics where needed,
then camera/scene/catalogue integration. Original dependency gates remain explicit; no fake ready
scene, synthetic save or forced Intro transition is introduced by this image path.

Final Debug x86 build warning-free; 80 image checks and all prior phase1/instance/projection/parser/
catalogue/DBCheck/Intro/binding/SPUI/package/scissor suites PASS. Six Python tests, 53 maintained
Markdown link checks and dashboard --check PASS. New launcher capture is pixel-identical to the
README launcher (RGB diff bbox=None). Launcher/Main → GGE/visible CreatureEditor smoke runs with
`-w -devDirs` exit 0, aborts/asserts 0; GGE continues to wait on MoviePlayer Unknown. Shared runtime
uploader is wired but not invoked by GGE in these smoke runs; the controlled GPU test uses the
resolver output + real Device/Overlay, not the native mounted card or a galaxy scene.
