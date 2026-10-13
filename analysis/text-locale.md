# The localized text tables (`locale`) — first real game TEXT on our screen

Authority: the shipped data under `build/bin/Debug/Data/Locale/<lang>/*.package`, read by
`source/engine/text.cpp`. Type id `0x02FAC0B6` = `locale` (from `reg_type.txt` via `typeids.inc`;
type ids are ASSIGNED by the game, not FNV of the name — see MEMORY.md).

## The format, measured

| fact | measurement |
|---|---|
| encoding | UTF-8 **with BOM** (`ef bb bf`), verified on the raw first bytes of a record |
| record separator | CRLF (`0d 0a`) |
| record grammar | `0x<8 hex>` then an optional `~markup~` token then the text |
| locale resources game-wide | 2,944 (15 languages) |
| en-us | 200 resources → **197 parse**, 3 carry no keys at all |
| entries / keys | **18,441 entries, 10,246 distinct keys** |
| comments | 5,434 lines start with `#` |
| continuation lines | 17 of 31,209 (a record's text may wrap) |
| unterminated `~markup~` | **0** in 18,439 key lines |
| non-ASCII texts | 53 |
| ambiguous keys | **684** |

One record (instance `0x04DC2F94`, 1,644 keys) opens with the game's own warning in prose:

```
# WARNING: This is an assembled content string table. It is auto-generated.
# Any change you would do in this file will be overwritten by the assembled content pipeline.
```

## The finding that decides the API: the key space is PER TABLE

| key band | keys | ambiguous |
|---|---|---|
| `0x00000000..0x00000FFF` | 1,752 | **36.2%** |
| `0x00001000..0x0000FFFF` | 105 | 1.9% |
| `0x00010000..0x000FFFFF` | 180 | 0.0% |
| `0x00100000..0x00FFFFFF` | 21 | 0.0% |
| `0x01000000..0x0FFFFFFF` | 2,957 | 0.7% |
| `0x10000000..0xFFFFFFFF` | 5,230 | 0.5% |

Two independent confirmations:

* **Within** any single locale resource a key appears **at most once** — 0 of 200 en-us tables
  contain a repeated key.
* **Across** tables the low band collides: `0x00000011` is `Build Mode` in table `0xE41C1333` and
  something unrelated in table `0x613373C1`. High keys (`0x05B08767`-style, which look like FNV
  outputs) are effectively global.

So `Tables::get(key)` **refuses** an ambiguous key instead of returning whichever table was parsed
first. `txt 0x00000011` prints the collision set and asks for a table instance. A wrong string in a
menu is worse than no string, and this is the same discipline as `naming`: never guess.

The practical consequence for the menu path: UI labels live in the high band (unique, so
`get(key)` answers them), while tutorial/guide text lives in the low per-table band. That matches
how a `spui` layout would reference them — a widget points at a key, and the widget's own table is
implied by which layout file it is in.

## Evidence: contiguous key runs are per-widget-group

`0x045BEDD1 Save`, `0x045BEDD4 Cancel`, `0x045BEDD8 Exit`, `0x045BEDD9 Quit` — four button labels
whose keys differ only in the low nibble. That is an allocated RUN, not a hash distribution, and it
is a hint for reading `spui` later: the layouts likely store a base key plus a small index.

## Reverse lookup, because a menu knows the LABEL not the key

`txt -label "New Game"` → `0x05B08767 table 0x2B7A0311 "New Game"`. The Title screen resolves its
five rows this way and prints label + key together so the provenance is visible on screen
(`analysis/shots/title_state.png`):

```
> New Game   0x05B08767
  Load       0x0045B9EB
  Options    0x00010009
  Credits    0x0473EF30
  Exit       0x045BEDD8
```

`Load Game` does not exist as a string in the tables (the game says `Load`), so the screen falls
back through candidates rather than inventing a label — the visible text is the game's.

## DISASM-CORRECTED: the locale search path was wrong

`fs.cpp` used to add `exe_dir/Locale/<lang>`, which **matches nothing on disk**; the localized-text
reader was therefore silently starved. The shipped layout is `Data/Locale/<lang>/…`, and the
original agrees: at `0x7e863c..0x7e8661` it loads the wide key `'Locale/'` (`0x01413cc4`), skips to
its end (`0x7e8641..0x7e864e` walk the UTF-16 to a NUL), and **appends** it to a base-path object
(`call 0x429760`), then appends the language — so `Locale/` hangs off a DATA ROOT, not off the
executable directory. The same cluster uses `'BaseData/'` `0x01413cb0` and `'UserData/'`
`0x01413c8c` the same way, which is the tie back to VERIFY row 2.

`fs::init` now attaches `Locale/<lang>` to every data root it knows (`Data`, `BaseData`), matched
case-insensitively (the tree ships lowercase `en-us`, `-locale` may be spelled any way). Verified:
`locale path: …\build\bin\Debug\Data\Locale\en-us`, then 197/200 tables parsed.

## Display shim, not transliteration

Our overlay font is a GDI raster atlas of ASCII 32..126 (`render/overlay.h`), so it cannot draw the
accented Latin of de/fr or the Cyrillic of ru. `to_display_ascii()` folds UTF-8 code points to a
near-ASCII rendering (Latin-1 supplement + Latin Extended-A + general punctuation) and emits one
`?` per unknown code point — never a per-byte burst, so a Cyrillic sentence degrades to a
readable-length placeholder. Proper diacritics need a real font atlas; that is backlog, and it is
recorded as such rather than hidden behind a silently-truncated string.

## What this does NOT give us

Which key belongs to which widget. That mapping is inside the `spui` layout, which is a bit-packed
serializer (see `analysis/ui.md`) and still needs its reader. So today we have the game's STRINGS
and not yet its LAYOUT — the next step on the Main→playable path remains the `spui` deserializer,
reachable through the window-class registrar cluster `0x8009f0..0x800b60`.
