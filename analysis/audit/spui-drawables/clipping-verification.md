# Host ancestor Clip / D3D9 scissor verification

Reviewed 2026-10-15. **Runtime evidence on OpenSpore only**; no original game process was executed.
The contract seed is the shipped WindowBase `Clip` mask `0x400` (clip child windows), not a claim
that GA's complete render traversal or fractional-pixel rounding has been reversed.

## Geometry

`tools/spui_verify.cpp` constructs a synthetic root/child/grandchild graph and checks:

- clipping ancestors intersect the viewport;
- a window's own Clip flag applies to its children, not itself;
- non-Clip ancestors do not restrict the child;
- disjoint intersections suppress the descendant;
- picking respects half-open boundaries and agrees with the shared rectangle;
- invalid indices and cyclic synthetic paths terminate with an empty rectangle.

Result: **PASS**, alongside 748 corpus matches / 486 atomically rejected malformed inputs and
the drawable atlas/state/scale/alignment tests.

## Pixel readback

Build target `overlay_clip_verify` uses the real `render::Device` and `render::Overlay`, a hidden
test window and a 64x64 D3D9 backbuffer. A caller-selected **new** BMP path prevents overwriting
an unrelated capture. Test sequence and RGB samples:

| Step / pixel | Expected | Observed assertion |
|---|---|---|
| Text at (2,2), queued before first clip | visible ink in top-left 16x16 | PASS |
| Red full-screen flat quad clipped to [16,32)², sample (17,17) | FF0000 | PASS |
| Blue full-screen image quad clipped to [20,28)², sample (23,23) | 0000FF | PASS |
| Samples outside the clip: (10,30), (33,23) | black | PASS |
| Text queued outside active scissor, sample (45,5) | black | PASS |
| Reset, then green square; sample (52,52) | 00FF00 | PASS |
| Empty or NaN rectangle | false; caller skips drawing | PASS |

This checks flush-before-state-change and scissor reset as well as geometry. Driver/runtime failure
returns nonzero instead of reporting a successful visual test. A real D3D9 device/display environment
is required; this target is not a headless-only CI test.

```powershell
cmake --build build --config Debug --target overlay_clip_verify
$path = Join-Path $env:TEMP ('OpenSpore-scissor-' + [guid]::NewGuid() + '.bmp')
.\build\Debug\overlay_clip_verify.exe $path
```

Live launcher regression: `-w -devDirs -state:DevLauncher -quitAfter 30`, boot 10/10,
exit 0, no scissor failures, aborts 0, asserts 0. No original runtime parity is inferred.
