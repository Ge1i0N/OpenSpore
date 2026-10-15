# GGE Animated Icon — static frame selector, not playback

Reviewed **2026-10-15**. Authority: GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static GA instructions + shipped XML/SPUI/PNG; host tests are not original-runtime observation.

## Implemented outcome

Owned `StarUiInstance` now has a separate AnimatedIconWin **frame/index and image binding**.
Its explicit object-index API can consume the existing `configure_star_ui` stage-index result;
`spui::animation_quad` derives row-major frame UV within the serialized Image's atlas rectangle.
The shared painter uses those supplied pixels through the existing runtime-image upload path.

This implements only **Auto Play=false, explicit positive frame/grid counts, obsolete Frame Width=0**.
It does not execute timers, animation end modes, native grid normalization, callbacks or mount events.
Nothing attaches it automatically to the diagnostic GGE screen or invents an available catalogue.

## ABI correction and original behavior

- Class **`0x0106F14B`**, native `cSPUIAnimatedIconWin` (`0x7F5A30`).
- **Correction:** QI **`0x0106F146` returns the original object**, not its `+0x20C` interface.
  `0x7F5500` returns `+0x20C` for **`0x0105A93D`**, and `+0x210` for `0x010EDF11`.
  `0xDF9200` queries `0106F146`, then explicitly takes `+0x20C` and calls that table's `+0x14`.
  Thus the old shorthand “0106F146 interface at +20C” conflated QI result with the next call step.
- Embedded table **`0x14152F4`**, setter `+0x14 → 0x7F5550`.
- Constructor `0x7F5D50` sets current frame **1** (`object+0x228`), count0 (`+0x230`), rows1
  (`+0x23C`) and columns0 (`+0x240`). This is **constructor-only state**, not native loaded state:
  `0x7F5E30` later resets current to **0** after preparing image/grid and installs the animation backend.
- Setter checks **signed `0 <= frame < count`** and changes current only if different, then calls
  window invalidation. **All normal paths return AL=1**, including invalid indices and equal values.
  Host results explicitly distinguish `Changed`, `Unchanged`, `IgnoredOutOfRange`; true is not proof
  that the requested frame was assigned. Unsupported target/descriptor returns false, output unchanged.
- Paint `0x7F5B50` uses image `+0x248`, columns/rows and current frame. It takes quotient/remainder
  by columns, forms `(col/columns,row/rows,(col+1)/columns,(row+1)/rows)` and draws into window extents.
- `0x7F5E30` derives/normalizes geometry from image dimensions/obsolete frame width, updates the special
  fill drawable, resets current0 and connects the backend. This lifecycle is retained but **not
  implemented**. Host pixel attachment is explicitly not a native rebind and does not run its reset.
  The host accepts only an already explicit valid grid whose count
  fits its cells, each value <=4096 (host safety limit). Zero/negative/oversized grids are unsupported,
  not guessed from unrelated PNG dimensions. Playback/notifications remain separate unknown work.

The image interface **`0xEF3C47CF`** and class `0xCF3C4EAE` are still separate. The
[phase-1 PNG image executor](gge-image-bindings.md) does not write AnimatedIconWin frame state or
reclassify its image as a normal fill drawable. The index path belongs to `DF9200` configuration,
not a newly invented `DF6D80` phase-1 index request.

## Real asset binding and isolation

Both actual `GGEGameStarUIv4` layouts have Animated Icon windows **45** (`0x026E0002`) and **93**
(`0x026E0001`). Object55 is a regular window also carrying `0x026E0002`; it remains a separate
candidate, never silently selected/discarded by class or visibility.

Both icons link serialized **Image44**, logical size **150×30**, **5 frames, 1 row × 5 columns**,
Auto Play=false, obsolete Frame Width=0. Their exact PNG key is
`(0xB89D2CD2,0x2F7D0004,0x31A44893)`; sampled original `Spore_Graphics.package` record **15591**
decodes to **256×512**. Its serialized Image UV is an atlas subrectangle, not the whole PNG.

`set_animation_pixels` accepts only decoded pixels matching that serialized resource triple and byte
count; it retains the serialized UV/logical dimensions instead of replacing them with PNG dimensions.
`set_animation_frame` requires an explicit window object. Runtime state is keyed by **window object**,
not Image44: the two windows can select different frames while sharing decoded atlas pixels.
It is independent of `runtime_images` (plain fill-drawable overrides), sibling instances and raw
serialized Objects. A frame can be assigned before pixels arrive; that is not drawable readiness.
Late attachment preserves its index. Reinstantiate/clear drops the map and restores constructor1,
**not** the post-normalization native loaded state. Callers should explicitly consume the stage-index
plan; simply attaching pixels does not reproduce original image-load ordering/reset/backend behavior.

No phase latch, native refcount/invalidation event, plugin registration, active animation timer or
native scene success is synthesized. With explicit runtime state, unavailable frames do not fall back
to unrelated template art. Layouts without such state retain their previous diagnostic fill renderer.

## Verification and exact test boundary

`gge_animated_verify`: **163 checks PASS** on both actual layouts, plus controlled variants:

- Actual Image44/resource key → unchanged shipped atlas PNG → existing provider's BGRA decode →
  separate window bindings → indices0..4 → atlas-composed quad.
- Constructor1, equal-index no-op, signed invalid indices/count, wrong regular duplicate/missing
  targets, unsupported Auto Play/obsolete width/zero grid, malformed types/duplicate properties.
- Window/sibling/template/fill-map isolation, independent frames with shared source image,
  late pixels, invalid replacement preserving old pixels, transactional quad failure and clear/reset.
- Existing DF9200 stage4→index3 plan consumed using an **explicit** object choice; controlled metadata
  is not claimed as an original save or a live GGE catalogue.
- Controlled **2×3 solid-colour grid**: real D3D9 readback asserts distinct red frame0/blue frame4
  and black pixels outside the scissor. This tests the host frame-quad/GPU path, **not original animation
  execution or a full mounted star card**. The shipped PNG is decoded/resolved but not that GPU fixture.

The large original `Spore_Graphics.package` exceeds the existing 256MiB explicit image-provider bound.
The test reads only its exact unique atlas record through the existing streaming `pkg::Package`,
places the **unchanged PNG bytes into a constructed one-record DBPF in memory**, and calls the bounded
provider. That carrier is explicitly a fixture, not a shipped package/save. The runtime provider's
size policy/priority/search scope is not widened; a production explicit streaming image adapter is
still needed to use that large package directly through `ImageProvider`.

```powershell
cmake --build build --config Debug --target OpenSpore gge_animated_verify
$capture = Join-Path 'C:\Users\DartPower\AppData\Local\Temp\opencode' ('GGE-frame-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\gge_animated_verify.exe <SPUI-fixtures> .\build\bin\Debug\Data\Spore_Graphics.package $capture
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --animated
python tools/gge_evidence.py --static --animated
```

Evidence: [facts + real property corpus](audit/gge-animated/facts.json), seven functions (C/instructions),
[QI/index extract](audit/gge-animated/qi-and-index-setter.asm.txt),
[frame quad](audit/gge-animated/frame-quad.asm.txt), [constructor](audit/gge-animated/constructor.asm.txt),
[seeds](audit/gge-animated-seeds.tsv), [shipped XML](scripts/xml/xml_033fe3fd.xml).
REA MCP was not connected in this session; an aligned registration was observed by `rea doctor`.
No configuration/engine changes were made. These are retained local Ghidra/objdump/asset results,
not REA Evidence IDs or native runtime observations.

Next: phase-action lifecycle and explicit package-backed pixel loading at runtime, then scene/camera
ownership and real Available catalogue input. Full playback can be added separately, not inferred
from this static stage selector. MoviePlayer/DBCheck/native mount readiness remain unchanged.

Final Debug x86 build warning-free. The 163 new checks and all prior image/phase1/instance/projection/
parser/catalogue/DBCheck/Intro/SPUI/package/scissor suites PASS. Six Python tests, 54 maintained
Markdown link checks and generated dashboard --check PASS. Launcher/Main → GGE/visible launcher →
CreatureEditor smoke runs exit0, aborts/asserts0; MoviePlayer observation remains Unknown. The new
launcher capture is RGB pixel-identical to `analysis/shots/spui_e3launch_uv.png` (diff bbox=None).
These smoke runs do not attach native/host star-card animation or establish a live galaxy scene.
