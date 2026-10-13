# `spui` is NOT a mystery: its property ids are the shipped designer schema

Session 13, and it is the biggest unblock since gmdl. It came from refusing to reverse the tokenizer.

## The move that did it

The method rule says reverse only as much as needed to advance, and here the needed amount turned out
to be **zero of the bit reader**. Instead: enumerate what data exists. A type histogram over all
150,945 resources (90 distinct types) showed a type nobody had looked at:

```
  0x02fac0b6 locale    2944
  0x250fe9a2 spui       748
  0x024a0e52 trigger    303
  0xcf6c21b8 xml         14      <<<
```

14 resources of type `xml`. Dumped, they are **the Spore UI Designer's own project files, shipped in
the game**: a 285-byte project manifest with `<Import file="..."/>`, a 2 KB resource-man config, a
583-byte font table, a **37,425-byte control/property class catalogue** and a **70,032-byte format
definition**, twice each (base game + EP1). The designer's schema for the very format we could not
read was sitting in `Spore_Graphics.package` the whole time.

## The result, measured

Every `<Class>` and `<Property>` in that schema carries a numeric `proxy` id. Harvesting them gives
**306 ids (66 Class + 240 Property)**. Scanning the 748 `spui` resources for those ids' 4-byte
little-endian forms:

* **748 / 748 layouts contain at least one schema id** (1,911,484 byte-granular hits; 469,497 of them
  on a 4-byte boundary, the rest off-boundary — the stream is not word-aligned, as a token stream
  predicts).
* **227 of the 306 schema ids appear** in the binaries.
* **Each id carries a STABLE byte width across the corpus**, which is what makes it a format and not
  noise:

| id | property (from the schema) | type | hits | pitch | dominance |
|---|---|---|---|---|---|
| `0x039a69e6` | Image Outline | Outline Format | 20,944 | **14 B** | **100%** |
| `0xeec1b004` | ShadeColor | color | 22,112 | **12 B** | **100%** |
| `0x00000007` | BorderColor | color | 54,804 | 6 B | 98% |
| `0xeec1b002` | CommandID | uint32 | 22,112 | 6 B | 95% |
| `0x00000005` | VerticalAlignmentBottom | int8 | 55,009 | 6 B | 90% |
| `0x00000006` | BorderFlags | flagset | 55,101 | 6 B | 89% |
| `0xeec1b005` | **Area** | **rectf** | 22,384 | 6 B | 92% |
| `0xeec1b00e` | TextFont | textstyle | 22,097 | 12 B | 80% |

A stable per-id pitch is exactly what a typed `(id, value)` record stream gives.

## The window record is now legible

The high block `0xeec1b0xx` is UTFWin's own window properties, and a minimal layout enumerates them in
order (`tools/spui_align.py`, smallest `spui` = 273 B, `Spore_Graphics.package` rec[4466]):

```
byte  47  WindowFlags   flagset     <- 0xeec1b000
byte  59  ControlID     uint32      <- 0xeec1b001
byte  71  CommandID     uint32      <- 0xeec1b002
byte  83  Area          rectf       <- 0xeec1b005   <<< THE GEOMETRY WE NEED
byte 107  BackgroundColor color     <- 0xeec1b006
byte 119  ShadeColor    color       <- 0xeec1b004
byte 131  TextFont      textstyle   <- 0xeec1b00e
byte 143  WinProcs      IWinProc[]  <- 0xeec1b00c
byte 159  WindowFlags ...           (the next window, same block)
byte 255  Children      IWindow[]   <- 0xeec1b00b
```

That is a window tree with **position (`Area`, a rectf), colours, font, procedures and children** —
i.e. everything a real menu screen needs, and it is named by shipped data rather than guessed.

Note also the id at byte 14 of that file: `0x2f418d72` = Class **`SimpleLayout`** (`base=LayoutStyle`),
and `0x4ec1b8d8` = Class **`Window`** (`base=WindowBase`). So the layouts record their **class** by the
schema's class id, and the schema gives the base-class chain (`Window` → `WindowBase` → …), which is
how field inheritance will resolve.

## The rule that generalizes: the id space is HIERARCHICAL, scoped per struct

The low ids looked like false friends and are not. 58 of 602 schema properties have a proxy id below
`0x100`, and those ids are **reused across structs**:

```
0x01 <- Size (Outline Format), TextStyle (elsewhere), Background Image (elsewhere)
0x02 <- Strength (Outline Format), Icon Image (elsewhere)
0x03 <- Quality, Wrapping, Background Color
0x08 <- Smoothness, Stroke Mode
0x0a <- Color, Stroke Shadow
```

So `0x00000001` inside an `Outline Format` value means `Size`, and inside a text control it means
`TextStyle`. **This is the same discovery as the `locale` tables' per-table key space**
(`analysis/text-locale.md`), arriving from the opposite direction: a low id is scoped by its
enclosing block, a high id is global. Any decoder that treats the low band as global will produce
confident garbage — which is precisely what my first word-aligned matcher did, reporting
`0x00000003` as "Wrapping" 746 times.

## What is still not known (recorded, not hidden)

* **The exact record framing.** Pitch is stable per id, but the value does not occupy the whole gap:
  `Image Outline` leaves 10 bytes after `id(4)` while its struct has 10 members totalling 40 bytes, so
  the stream stores **only the members that differ from their schema default** (the schema carries
  `default="0"`, `default="1"`, `default="false"`, …) plus presumably a presence mask. 10 members /
  10 bytes is suggestive of one byte per present member, but that is a hypothesis, not a measurement.
* **The bit packing.** Off-boundary id hits (1,441,987 of 1,911,484) prove fields are not
  byte-aligned in general, so a real reader must track a bit cursor. The reader is `0x9059c0`
  (dispatches on a mode at `[this+0x1e0]`), fed by `0x9015e0` (refills an 8192-byte buffer, sets an
  EOF flag at `+0x70`, error codes 1/2 at `+0x04`) — so the buffer straddling is real and a token can
  cross the 8 KB window.
* **Why `Area` (a rectf, 16 bytes) has pitch 6.** Either the four floats are quantized, or the value is
  an index into a shared table. Unresolved, and it is the single most important open question because
  `Area` is the geometry.

## The next step, and why it is now narrow

Do **not** try to read the bit reader cold. Instead:

1. Take one small layout, walk it with the schema as the dictionary, and for each id read the value
   width from the MEASURED pitch table (not from the type), emitting `(class, property, raw bytes)`.
2. Cross-check the result against the schema's defaults and base classes: any member whose raw bytes
   are absent must equal its `default`, and that equation is what reveals the presence mask.
3. Only then touch `0x9059c0`.

That is a data-driven derivation with a built-in correctness check, which is how gmdl and raster were
solved, and it is the first time we have a dictionary for this format.

## Files

* `tools/spui_schema.py` — harvest the ids from the XML, first correlation (748/748, 469,497 hits).
* `tools/spui_align.py` — byte-granular scan + the annotated smallest layout (the 12-byte pitch, the
  window field list, the rotation that made ids look like `0x09eec1b0`).
* `tools/spui_shape.py` — the per-id pitch table with dominance (the measurement that makes this a
  format rather than a coincidence).
* `analysis/scripts/xml/*.xml` — the 9 distinct shipped designer files, extracted verbatim (70 KB +
  37 KB + 2 KB + fonts + manifest).
