# SPUI drawable/object graph — current evidence

The sequential reader now exposes the object graph, so the next UI pass can follow references instead
of guessing from window colors. `tools/spui_drawables.py` reports the E3LaunchScreen graph by class.

## E3LaunchScreen

| item | measured |
|---|---:|
| objects | 227 |
| Window/Button objects | 136 |
| `DialogDrawable` references | present on window/button objects |
| drawable classes | `ImageDrawable`, `ButtonDrawableStandard`, `ButtonDrawableRadio` |
| WinProc classes | `Fade`, `Glide`, `Inflate` |
| Children links | 67 |

The class mapping is from the shipped designer schema/object class table, not from filenames. For
example:

```
Window  -> DialogDrawable -> ImageDrawable
Button  -> DialogDrawable -> ButtonDrawableStandard
Window  -> WinProcs -> Fade / Glide / Inflate
```

`FillDrawable` is an object-index reference (`code 19`), so the next implementation boundary is
well-defined: resolve the target drawable object's class and property payload, then locate the
resource references used by that drawable. It is not yet honest to treat the `raw` property words as
PNG/raster instance ids; the object class and its class-specific property schema must be read first.

## Image reference resolution — measured

`ImageDrawable`/`ButtonDrawableStandard` image properties resolve to an intermediary object class
`0x01BE6B15`, whose `0x01BE0001` property contains an external-table index. E3LaunchScreen has 40
external entries. Every used image reference resolves as `{instance, type=0x2F7D0004,
 group=0x31A44893}` and all 16 used instances resolve exactly once in `Data/Spore_Graphics.package` as
 PNG resources. Across the full 748-resource corpus, the C++ parser validates **15,489 ImageDrawable
 external references** against their owning external tables. The live renderer now decodes the first
 PNGs (`512x512`, `1024x256`) through this path; four entries still miss in the C++ package lookup and
 are logged for the next cache/type/group-precedence fix. This closes `ImageDrawable -> external index
 -> PNG DBPF resource`; alignment, tiling, scaling and Button state-image selection remain.

The current diagnostic renderer draws geometry and captions. It intentionally does not pretend that
`ImageDrawable` or `ButtonDrawableStandard` are already rendered. That is the next visual milestone.
