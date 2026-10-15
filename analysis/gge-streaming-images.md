# GGE images — direct bounded streaming provider

Reviewed **2026-10-15**. Host implementation against the existing DBPF/RefPack/PNG contracts;
no new original-runtime observation, resource-precedence recovery or native service execution.

## Outcome

`StreamingImageProvider` now implements the same `ImageProvider` port as the small-file
`ExplicitImageProvider`, but keeps **one explicitly selected file open** and reads only its bounded
directory and requested resource. The original `Spore_Graphics.package` (**942,881,325 bytes** in
this corpus) can supply the actual star atlas directly. No constructed DBPF carrier is required.

The atlas is the exact key `(0xB89D2CD2,0x2F7D0004,0x31A44893)`, record **15591**, decoded size
**256×512**. Original path/record provenance is attached to the immutable BGRA pixels. Both the
[Animated Icon selector](gge-animated-icon.md) and [host lifecycle](gge-star-lifecycle.md) verifiers
now use this direct file-backed path. A lifecycle integration case supplies the real provider itself,
not a fixture routing provider. Controlled metadata remains controlled; this is not a real save.

## Separate scope, unchanged old adapter

- `ExplicitImageProvider` / `ExplicitDbpfReader` retain their existing **256MiB memory-package**
  policy and in-memory fixture API. Their file-size/resource/type support is not silently expanded.
- The new provider accepts only **DBPF3.0/index3**, complete shared-mask directory parsing (`0..7`),
  and nonzero-instance PNG keys of type `0x2F7D0004`.
- It never searches directories, profiles, other packages or fallback keys; it is not an implementation
  of original package priority/cache behavior. Callers choose the one file explicitly.
- Lookup is exact `(instance,type,group)`. Duplicate records remain **Ambiguous**, including a duplicate
  whose unsupported high key word might otherwise tempt a first-match shortcut.

Both providers share the same bounded PNG signature/IHDR/GDI+ decode helper. Found requires matching
decoded dimensions and exact BGRA byte count, not an index hit or a synthetically blank texture.

## Bounds and file ownership

| Host policy | Limit |
|---|---:|
| Selected file | 4GiB inclusive |
| Directory records | 65,536 |
| Encoded directory | 4MiB |
| Packed resource / decoded PNG bytes | 64MiB each |
| PNG width / height | 1..4096 each |

`Package::open_bounded` checks file/count/index bounds on the **same FILE handle** used to parse/read.
It does not preflight one pathname and then reopen an unbounded parser on potentially different bytes.
The file-size check happens before directory work; count/index checks precede allocation of the index
or record vector. Version/complete-index checks must also pass before the provider becomes open.

The general Package file path now uses `_fseeki64` / `_ftelli64`, including resource seeks. Memory-
backed packages use subtraction-based bounds to avoid `offset+length` overflow. A valid header with
an unknown/mismatched index no longer reports `index_parsed=true`. Minor version is retained for the
new provider's strict version check. Existing unbounded `Package::open` callers keep their broad
format/allocation behavior; this change does not claim every package caller is now size-bounded.

Resource metadata is checked **before** packed/decoded allocation: nonzero sizes, supported flags1,
raw or RefPack compression, zero unsupported high key word, valid header-to-index span and raw-size
agreement. RefPack's declared size must match the bounded index memSize before its output allocation.
PNG IHDR bounds precede GDI+ decode. These guards are host policies, not newly recovered native limits.

Directory memory, packed bytes, RefPack output, BGRA and GDI+ working memory are separate allocations;
the table is **not a global process/GPU memory budget** or a hardened arbitrary-PNG sandbox. The
provider is synchronous/single-threaded and not a texture cache. The lifecycle's existing per-pass
memoization can share decoded pixels; the provider itself does not cache read results between calls.

`close` retires the file and logical directory state. Failed reopen also retires the previous source;
subsequent supported-key reads are Unknown, not a stale Found/Missing. Previously returned immutable
pixels remain valid independently of the provider. Empty/NUL-containing paths are rejected rather than
silently truncated. Paths otherwise follow the existing Package path/CRT encoding support.

The open directory is not a content-hashed immutable package snapshot or file watcher. Concurrent
in-place source changes are unsupported; provenance records the selected path/record, not a guarantee
against every mutable-file race. The Windows CRT read handle denied a controlled concurrent resize
with sharing-violation32; the test does not pretend truncation or a subsequent short read occurred.
No profile/save discovery or writes to original packages are introduced.

## Explicit outcomes

- **Unknown:** provider is closed or open failed (supported key).
- **Missing:** exact key absent in the selected parsed directory.
- **Ambiguous:** multiple exact directory matches; no first-record choice.
- **Unsupported:** zero/non-PNG key, unsupported record metadata/compression/high word, resource or
  PNG dimension bound. An unsupported container fails `open`, leaving the provider closed.
- **ReadFailure:** invalid resource size/span, file read/RefPack failure, corrupt/truncated PNG or
  pixel/dimension mismatch. No partial pixel binding is published.
- **Found:** actual BGRA decoded from the selected resource, with original file/record provenance.

Existing image/Animated Icon application rules still preserve old bindings on failure. A ready image
does not imply phase completion, mounted UI, listeners, catalogue availability or a live camera.

## Verification

`gge_streaming_image_verify`: **95 checks PASS**:

- Original large Graphics package → exact atlas → actual decoded pixels, compared byte-for-byte
  with an independent Package/resource + GDI+ decode. The small memory provider still rejects it.
- Controlled disk fixtures for all eight shared masks, raw and literal-only RefPack carrying the
  unchanged actual PNG. These containers are deliberately test fixtures, not shipped saves/packages.
- Duplicate/missing keys, unsupported versions/index metadata, corrupt/truncated directory/header,
  record/index allocation bounds, unsupported record fields, invalid spans/sizes and broken PNG/RefPack.
- **Sparse controlled file with both resource and index above 2GiB** decodes successfully through
  64-bit seeks. Other sparse fixtures exercise oversized packed-resource and >4GiB file rejection
  without writing/allocating gigabytes of payload.
- Reopen/close pixel lifetime and **544 successful-open/read/failed-reopen/close cycles**, beyond the
  default CRT512 descriptor limit. Temporary files are created exclusively inside a required **new**
  caller-selected test directory; cleanup removes only those created files and the empty directory.
- The Windows read-handle sharing policy prevents a concurrent truncation attempt on a controlled
  fixture; its unchanged bytes remain readable. Resource span/decode and truncated-file rejection
  are separate tests, not a claim that a live source was successfully mutated under the reader.

`gge_animated_verify`: **159 checks PASS** (previously163; four carrier-open assertions removed),
including the existing controlled multi-row D3D9 frame/scissor readback. Its actual atlas now comes
directly from the original package. `gge_star_lifecycle_verify`: **232 checks PASS** (previously228),
including direct provider → lifecycle → frame resolver/provenance checks on both actual layouts.
The controlled GPU fixture is still not a native star card or actual galaxy screenshot.

```powershell
cmake --build build --config Debug --target OpenSpore gge_streaming_image_verify gge_animated_verify gge_star_lifecycle_verify
$newDir = Join-Path 'C:\Users\DartPower\AppData\Local\Temp\opencode' ('GGE-stream-' + [guid]::NewGuid())
.\build\Debug\gge_streaming_image_verify.exe .\build\bin\Debug\Data\Spore_Graphics.package $newDir
```

Source: [provider](../source/engine/gge_image_provider.cpp), [bounded Package](../source/engine/pkg.cpp),
[new verifier](../tools/gge_streaming_image_verify.cpp). Reuses existing [image](gge-image-bindings.md)
and [Animated Icon](gge-animated-icon.md) evidence; these host bounds are not new native findings.

Next: scene/active-camera ownership, explicit catalogue input and actual per-star placement/selection
integration. The normal GGE screen, MoviePlayer Unknown gate and native mount completion are unchanged.
The [host scene collection](gge-star-scene.md) now consumes explicit catalogue/camera observations
and these original atlas pixels through owned lifecycles; native factories and a live producer remain absent.

Final Debug x86 warning-free. Streaming95/lifecycle232/AnimatedIcon159/image80 and all prior
phase1/instance/projection/parser/catalogue/DBCheck/Intro/SPUI/package suites PASS; controlled image/
frame/scissor GPU readbacks PASS. Six Python tests, 56 maintained Markdown link checks and dashboard
--check PASS. Launcher/Main → GGE/visible launcher → CreatureEditor smoke runs exit0, aborts/asserts0;
MoviePlayer remains explicitly Unknown. The launcher capture is RGB pixel-identical to
`analysis/shots/spui_e3launch_uv.png` (diff bbox=None). Those smoke runs do not invoke automatic star
cards or prove a native scene; direct streaming integration is exercised by the controlled verifiers.
