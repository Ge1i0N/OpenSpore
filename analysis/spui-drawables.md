# SPUI drawables — GA evidence and implemented subset

## Identity and corrections

Address authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No beta addresses are used. Retained instructions, property/state tables and persistent-Ghidra
pseudocode: `analysis/audit/spui-drawables/`.

The earlier `Window -> DialogDrawable -> ImageDrawable` summary confused a **property alias** with
an object class. The shipped XML overrides `0xEEC1B007` under different window subclasses; the old
name dictionary kept the last alias. Actual E3 sample chains are:

```
Window 42 -> FillDrawable object 41 (ImageDrawable, 0xCF3C4EAE)
         -> Image property 0x4F3C4A26 -> object 40 (Image, 0x01BE6B15)
         -> 0x01BE0001 -> external[0] -> PNG
Button 45 -> drawable 44 (ButtonDrawableStandard, 0xAF3A9E26)
          -> Image property 0x0F3AC75E -> Image object 43 -> external[1] -> PNG
```

`0x01BE6B15` is **Image**, not ImageDrawable. The prior recursive "first image reachable" rendering
was diagnostic, not recovered wrapper semantics. It has been replaced by explicit class/property
resolution. `tools/spui_drawable_props.py` also had a typo (`0x4F3C4EAE`); corrected to `0xCF3C4EAE`.

## Image object: atlas UV and logical size

GA `0x13004A0` registers `0x01BE0002` (code 16) through getters/setters `0x957710/0x957750`.
The setter stores left/top and right-left/bottom-top; the getter reconstructs the corner rectangle.
It also registers `0x01BE0003` (code 15), whose setter `0x9576F0` writes logical width/height at
`Image+0x1C/+0x20`. These dimensions are not necessarily the PNG dimensions.

Example: object 43 has logical **688x173**, texture **1024x256**, atlas UV
`(-0.0224610008, 0, 0.6494140029, 0.6757810116)`. Do not clamp/normalize the serialized rectangle
to `[0,1]` in the parser: slightly negative or >1 values occur in shipped images.

External triples remain `{instance,type,group}`. E3 has 40 entries and 16 unique PNG instances,
all found in `Spore_Graphics.package` (`type=0x2F7D0004`, group `0x31A44893`). Across 748 layouts,
15,489 Image external references are structurally validated. The live PNG index covers 57 packages,
150,945 records, 14,549 PNG entries and 14,450 unique keys. Duplicate resolution currently retains
the first discovered key; this is a presentation policy, not a fully reversed resource-priority rule.

## ImageDrawable: implemented None-tiling branch

Property table at `0x0154F690`, constructor `0x97D840`, render function `0x97D1E0`:

| property | object field | default / meaning |
|---|---:|---|
| `0x4F3C4A26` Image | +0x24 | reference to Image |
| `0x4F3C4A27` flags | +0x14 | 0; bit 0 FixedWidth, bit 1 FixedHeight |
| `0x4F3C4A29` horizontal alignment | +0x1C | 3; 1 left, 2 right, 3 center |
| `0x4F3C4A28` vertical alignment | +0x20 | 3; 1 top, 2 bottom, 3 middle |
| `0x4F3C4A2A` tiling | +0x18 | 0 None, 1 Standard, 2 Edge |
| `0x4F3C4A2B` scale | +0x10 | 1.0 |

Instructions `0x97D24B..0x97D2E8` apply `logical dimension * scale` only to fixed axes; other axes
fill the window bounds independently (not aspect-preserving letterbox). Fixed axes are aligned
within those bounds. The render function also has outline/shadow and tiling branches; these are
not implemented. Decompiler stack recovery is imperfect; its assembly remains primary evidence.

## Standard / radio button state atlas

`0x966A40` returns normalized horizontal quarter rectangles. State low three bits:

| drawable state | standard frame |
|---|---:|
| 0 Normal | 1 (or 3 when selected bit 0x8 is set) |
| 1 Disabled | 0 |
| 2 Highlighted | 2 |
| 3 Active | 3 |

`0x966AD0` returns sixths for radio: states 0/1 -> frame 0, 2 -> 1, 3 -> 2; selected bit adds 3.
The corresponding natural-size routines `0x964690/0x9646D0` divide logical width by 4/6, preserving
height. `0x9655F0..0x9656CB` obtains the state rectangle and submits it to the Image renderer; a
nonzero Tileable byte selects a separate edge-render path. Current implementation supports only
non-tileable images and composes the state rectangle with the Image atlas rectangle.

Host pointer/enabled -> drawable state is our adapter, not the recovered general Button event
machine. Radio checked-state mutation, mouse capture/release rules and command dispatch remain open.

## Implementation and verification

`source/engine/spui_drawable.cpp` produces a pure `ImageQuad` from explicit classes/properties;
unsupported classes/tiling modes and invalid geometry return false without changing output.
`Overlay::image_uv` submits the chosen UV rectangle. Diagnostic window outlines/text remain enabled.

`spui_verify` checks atlas UV, standard four frames, selected/unselected radio six frames,
fixed-size scaling, all three alignments, unsupported tiling and invalid/missing drawable handling,
alongside the 748-layout / 486 malformed-input suite. Capture: `analysis/shots/spui_e3launch_uv.png`.
This is a coherent real-art launcher, **not** a full original SPUI implementation: dynamic sibling
Z-order, general scissor/Clip application, outline/shadow, Standard/Edge tiling and Dialog pieces
still need work. No gameplay completion percentage is inferred from the visual improvement.

### Previously missing PNGs: resource lifetime defect, not absent assets

Temporary `pkg::Package` objects had no destructor and leaked their `FILE*`. Repeated per-image
scans exhausted the default CRT stream limit (512), after which even existing files failed to open.
The index reduced repeated scans but did not fix ownership. `Package` now closes on destruction and
is non-copyable. `pkg_lifetime_verify` runs 544 scoped open/read/destruct cycles with the limit
unchanged at 512. Earlier explanations attributing misses to type/group or run length were unproven
and are superseded by this lifecycle diagnosis.
