# `spui` is NOT a mystery: its property ids are the shipped designer schema

> **2026-10-15: current authority is [spui-binary.md](spui-binary.md).** Historical experiments below
> are retained, but code 19 is count u16 object references, code 20 contains nested structs, and
> 0x5FF5 is a real object marker. Unknown ids do not block sequential parsing: 748/748 now reach EOF.
> The inline-Children and "vocabulary-only wall" explanations below were wrong premises.

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

## CORRECTION to this file: the bit-packing claim was wrong about the mechanism

Written earlier in this session and repeated in `MEMORY.md`/`RESUME.md`:

> "1,441,987 of 1,911,484 hits are off-boundary, so the stream is not word-aligned, exactly as a token
> stream predicts."

That inference was **wrong**, and the audit-of-my-own-work instinct is what caught it. The aggregate
mod-4 histogram was nearly uniform (24.56 / 24.73 / 25.58 / 25.13 %), which I read as bit packing. But
uniformity is also what **byte-packed records whose sizes are not multiples of 4** produce, and when
the test was redone per id, splitting ids by the number of zero bytes in their 4-byte pattern (padding
satisfies 3-and-4-zero-byte patterns trivially, so the aggregate was worthless), the result was:

```
  zc=0  ids seen 215/247  hits 494,552   mod4: 24.2% 22.7% 28.8% 24.4%
  zc=1  ids seen  18/38   hits  70,787   mod4: 25.5% 24.5% 25.6% 24.4%
```

Even the zero-free ids — which cannot arise from padding — sit uniformly across all four alignments.
Combined with the measured record sizes (`tools/spui_read.py`: code 9/11 → 12-byte records, code 20 →
**14**, code 18 → variable), the drift is fully explained: a 14-byte record moves the cursor off the
4-byte grid by 2, and the offsets accumulate. **The stream is byte-packed, not bit-packed.** No bit
cursor is needed to read the records; the reader needs the per-code sizes.

The lesson is the same one this session keeps teaching, one level up: a distribution that matches a
hypothesis is not evidence for it when other hypotheses predict the same distribution. The discriminating
test here was per-id, not aggregate — and it was cheap.

## The integration: a layout names its LOCALE TABLE, and the strings are the menu's own

`tools/spui_table_ref.py`. Code 18 (`string` / `string_resource`) is **overloaded** into two forms,
separated by a leading u16 of `0xFFFF`:

```
  'not inline' (leading u16 == 0xFFFF): 3,107 anchors, 36 distinct references
  inline UTF-16 (leading u16 = length): 4,207 anchors, 70 distinct lengths
     len=8 chars x1874 -> u32 0x006F0054 = UTF-16 "To"
     len=5 chars x241  -> u32 0x00300033 = UTF-16 "30"
```

Of those 36 references, **21 are real en-us `locale` TABLE instances** — against an accidental rate of
200/2^32 = 4.66e-08. And the tables they name are the game's UI text:

| ref | anchors | the table's own first strings |
|---|---|---|
| `0xC0152A6D` | 846 | **Spore Guide** |
| `0x2B7A0311` | 394 | **STATE = GalaxyGameEntry** → "Delete Game" |
| `0xAD7B2086` | 309 | **Audio Settings** |
| `0xF5E156F8` | 236 | Out of Range |
| `0x7C6511A1` | 140 | **Sort By:** |
| `0xC047E881` | 128 | Beat |
| `0x2C978DA4` | 104 | **Create a Creature** |
| `0xB7BCEF68` | 86 | Registration |
| `0x40D7E685` | 42 | **OK** |
| `0x8B96F855` | 41 | **Missions List** |
| `0xA1C68F50` | 25 | Ban Content |
| `0xDC351566` | 14 | City Legend |
| `0x9A5ACF10` | 10 | Carnivore |

So a `spui` layout does not carry a key into a string table — it carries **the identity of the table it
draws its text from**, and the per-string keys are resolved at runtime. That closes the loop between
three things this session reversed independently:

```
the STATE MACHINE            GalaxyGameEntry   -id 0x02ccda9c        (analysis/states.md)
      |  fnv1_lower(state name)
      v
the LOCALIZED TEXT           table instance 0x2B7A0311 -> "Delete Game", ...  (text-locale.md)
      ^
      |  code-18 "not inline" reference
the UI LAYOUT                748 spui, windows with Area rects + refs        (this file)
```

`0x2B7A0311` appearing in **both** directions is the confirmation: it is `fnv1_lower("GalaxyGameEntry")`
(the four-state cross-check from earlier) AND a code-18 reference inside real layouts. The state whose
name hashes to a table is the same state whose layouts reference that table.

**What this means for the menu path, concretely:** we can now compose a real screen without solving the
remaining format questions. From a layout we have window geometry (`Area`, 22,384 windows across 748
layouts, all of them) and the table its strings live in; from `source/engine/text.cpp` we already
resolve that table's keys to the game's own strings. The open items (the presence mask, the unknown
schema ids that make a sequential walk derail in 637/748 files, why `Area`'s payload is 16 bytes but
some records are 14) are no longer on the critical path for showing a menu — they are refinements.

## The widget HIERARCHY is in the data: `Children` = `0xEEC1B00B` (found 2026-10-15)

The engine has been GUESSING z-order — `spui::paint_order()` paints the largest Area first and
`hit_test()` selects the smallest containing Area. Those two rules agree with each other, which is what
makes clicks honest today, but neither reads the layout's real parent/child tree. The tree exists, and
it was already on disk twice over.

**1. The designer schema defines it.** Parsing the shipped `xml` resources by ATTRIBUTE (my first
attempt grepped only `proxy=`, which holds the hex id, and so returned ids with no names and looked
like a negative result - it was not; the names are in `name=`):

```
0xeec1b000  WindowFlags      0xeec1b005  Area            0xeec1b00a  Caption
0xeec1b001  ControlID        0xeec1b006  BackgroundColor 0xeec1b00b  Children
0xeec1b002  CommandID        0xeec1b007  Drawable        0xeec1b00c  WinProcs
0xeec1b004  ShadeColor       0xeec1b00e  TextFont
```

`Children` sits in the same group as `ControlID` and `CommandID`, which is exactly where a container's
child list belongs. (Same group also names the *other* structural candidates: `WinProcs`,
`ButtonGroupID` `0xeec1d005`, `ButtonStateFlags` `0xeec1d001`.)

**2. It is present in the layouts.** In `E3LaunchScreen` (`0xE932BD44`, the screen the state script
names) there are **13 `Children` records**, every one with `code=19` (the array code), `marker=0x0001`,
and a u16 count immediately after the header. Measured counts, in file order:

```
63, 69, 74, 79, 84, 89, ...
```

**3. What follows the count is a NESTED BLOCK of the child's own property records**, and the blocks are
byte-identical in shape across occurrences - each begins with the same sequence, e.g.

```
02 D0 C1 EE 09 00 01 00 00 00 00 00    ButtonFlags/Direction = 0
01 D0 C1 EE 09 00 01 00 00 00 00 00    ButtonStateFlags = 0
07 B0 C1 EE 13 00 01 00 <count>        Drawable, code 19, ITS OWN nested array
04 D0 C1 EE 09 00 01 00 01 00 00 00    Alignment = 1
06 D0 C1 EE 14 00 01 00 32 00 00 00    CaptionBorder, code 20 (6-byte payload)
```

so children are serialised INLINE, each as a full property set, and the array code 19 nests.

**WHAT IS NOT SETTLED, stated rather than glossed - and now with the ruled-out readings listed.** What
the u16 after a code-19 header counts is still open, and three readings are now MEASURED to be wrong:

| reading | test | result |
|---|---|---|
| RECORDS | walk that many records (`tools/spui_tree.py`) | derails after 10 -> **ruled out** |
| BYTE LENGTH of the nested block | next record at `off + 10 + N` (`tools/spui_code19.py`) | 1 of 14 land well-formed -> **ruled out** |
| ELEMENT COUNT of 4-byte elements | next record at `off + 10 + 4N` (same tool) | 0 of 14 -> **ruled out** |

The values rise steadily across the layout (41, 43, 44, 46, 47, 49, 50, 52, ... interleaved between
`FillDrawable` and an image record), which looks like a running index or a cumulative position rather
than a size - but that is a guess and is recorded as one. **Consequence: the explicit parent/child tree
is not readable from this field yet, so the engine keeps its geometry-based containment (paint largest
first, hit the smallest containing Area), which is honest and works.** Two heuristics that agree with
each other beat a tree read wrongly.

**Consequence for the engine:** the paint order and the hit order can both come from the tree the
designer actually built, instead of two heuristics that happen to agree. `spui::paint_order()` and
`spui::hit_test()` are the two functions to replace, and they were written to be replaced - the real
hierarchy is named in both as the single place to change.

## CORRECTION 2026-10-15: the third u16 is an ELEMENT COUNT, not a constant marker

The framing has been `id:u32 | code:u16 | marker:u16(==1) | payload[code]`, with "marker 0x0001 in
98.4% of 1,098,129 anchors". **The marker is not a marker.** It is an ELEMENT COUNT, and the 98.4% is
simply how many properties are scalar.

How it surfaced: a walk of the `Children` block (see the hierarchy section) consumed 10 records
coherently and then hit

```
0B D0 C1 EE | 09 00 | 08 00     id 0xEEC1D00B = CaptionColors, code 9, third u16 = 8
```

`CaptionColors` is a LIST of colours per the schema, so 8 is eight colours. Tested both readings
(`tools/spui_record_count.py`), over every record in the layout whose third u16 is neither 0 nor 1:

| reading | next record after the record of interest |
|---|---|
| third u16 is a constant marker (payload = element size) | **nothing** - misaligned |
| third u16 is a count (payload = count x element size) | `id=0xEEC1D003 code=9 third=1` - well formed |

**24 records in one layout where the count reading is coherent and the marker reading is not, 0 the
other way.** So the record is `id(4) | code(2) | count(2) | count * element_size`, and a record with 8
colours is 40 bytes rather than 12.

**Consequences, both measured:**

* **Coverage:** the old scan required `marker == 1` and therefore SKIPPED every multi-element record.
  With the count accepted - and the id's stable code still required, plus a code-plausibility guard
  (see below) - anchors read across the corpus go **961,360 -> 976,318**, of which **15,032** carry a
  count other than 1. So the correction buys **+14,958 anchors (+1.6%)**, not the +12.5% the 1,098,129
  total might suggest - the rest of that gap is the VOCABULARY (unknown ids), which is the wall it
  always was. (An earlier run of this same measurement reported 977,010 before the class-sentinel guard
  below; 976,318 is the honest figure and supersedes it.)
* **A FALSE ANCHOR the relaxation let in, and the guard that closes it.** Relaxing the third u16 from
  "must be 1" to "any sane count" admits an anchor whenever the schema holds a garbage code for an id.
  It did: `id=0x4EC1B8D8 "Window" code=24565 count=40`, which is not a record shape. The cause is in the
  GENERATOR: `tools/spui_typecode.py` reports **0x5FF5** for a `<Class>` id as its sentinel for "this is
  a class, not a property", and the schema emitted that as a code - for five ids (`Window`, `Button`,
  `Dialog`, `Outlined Text`, `Animated Icon`). Fixed at the source (classes now get 0xFFFF, which the
  reader skips) AND guarded in the reader (no code above 32 is a code; real codes are 2, 5, 7, 9, 11,
  16, 17, 18, 19, 20). `tools/schema_code_audit.py` now reports 0 implausible codes.
* **It does NOT fix the sequential walk.** Walking with count-aware sizing reaches 10 records where
  marker-sizing reaches 9 on E3LaunchScreen, and gets no further: `tools/spui_walk.py` reports
  12.7% vs 9.2% of the layout. So the "derails in 637 of 748" wall is NOT a mis-sizing artefact, which
  is worth knowing - the previous explanation was right and this correction is about coverage.
* **Overlap pruning had to change with it:** the old check advanced 4 bytes past an anchor, which would
  keep a false anchor from inside a 40-byte record's payload. It now advances by the record's true size
  (`record_size(code, count)`).

Nothing else moved: 22,384 windows, 18,232 usable Areas, 1,384 of 1,426 captions, 57/57 packages,
yardstick 10/10, 0 warnings.

## What is still not known (recorded, not hidden)

* **The exact record framing.** Pitch is stable per id, but the value does not occupy the whole gap:
  `Image Outline` leaves 10 bytes after `id(4)` while its struct has 10 members totalling 40 bytes, so
  the stream stores **only the members that differ from their schema default** (the schema carries
  `default="0"`, `default="1"`, `default="false"`, …) plus presumably a presence mask. 10 members /
  10 bytes is suggestive of one byte per present member, but that is a hypothesis, not a measurement.
* **The walk derails on unknown ids, not on bit packing.** A sequential walk reaches EOF cleanly in
  **0/748** layouts and derails in 637 with "id not in schema": some record ids in the binaries are
  absent from the shipped designer schema (only 227 of its 306 ids appear at all), so one gap spans two
  records. Extending the schema is the real remaining job. The reader is `0x9059c0` (dispatches on a
  mode at `[this+0x1e0]`), fed by `0x9015e0` (refills an 8192-byte buffer, sets an EOF flag at `+0x70`,
  error codes 1/2 at `+0x04`) — a buffered byte reader, consistent with byte-packed records rather than
  with a bit cursor.
* **`Area` is RESOLVED, and this file's earlier note about it was stale.** The first measurement said
  "pitch 6, unexplained"; the per-code measurement shows `Area` carries **code 16** and a **24-byte
  record = id(4) + code/marker(4) + payload(16)**, i.e. exactly a `rectf`, and the payload decodes to
  real UI coordinates — `(618, 183, 802, 209)`, `(49, 6, 161, 23)`, `(0, 0, 40, 26)` across 22,384
  windows. The "6" was an artifact of pooling low, block-scoped ids with high ones.

## The next step, and why it is now narrow

Do **not** try to read the buffered byte reader cold. Instead:

1. Take one small layout, walk it with the schema as the dictionary, and for each id read the value
   width from the MEASURED pitch table (not from the type), emitting `(class, property, raw bytes)`.
2. Cross-check the result against the schema's defaults and base classes: any member whose raw bytes
   are absent must equal its `default`, and that equation is what reveals the presence mask.
3. Extend the schema, because that is what actually blocks a clean walk: 637 of 748 layouts derail on
   an id the designer XML does not list, and only 227 of its 306 ids appear in the binaries. The
   derailing ids are the next vocabulary to recover (the `spui` bytes themselves name them, and the
   designer XML has 66 Class ids to go with 240 Property ids).
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
