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

The current diagnostic renderer draws geometry and captions. It intentionally does not pretend that
`ImageDrawable` or `ButtonDrawableStandard` are already rendered. That is the next visual milestone.
