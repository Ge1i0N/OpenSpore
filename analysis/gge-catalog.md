# Play Spore catalogue — source tracing and typed entry projection

Reviewed **2026-10-15**. Static target: GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Retained Ghidra instructions/pseudocode (15 functions), focused objdump and
[field facts](audit/gge-catalog/facts.json) in `analysis/audit/gge-catalog/`.
No original runtime capture or original save file was used in this pass.

## Where the two collections actually come from

`0xB3D3A0` returns `DAT_0167EAE4`. `0xBA6450` is just `lea eax,[ecx+0xDC] / ret`.
The manager has two pointer vectors consumed by `0xDF95A0`:

| Collection | Begin/end | Consumer | Published placement |
|---|---|---|---|
| Existing/saved game stars | +0xF0/+0xF4 | `0xDF7F30` | first N GGE entries |
| New-game candidates | +0xDC/+0xE0 | `0xDF7CB0` | after those N entries |

Although `0xDF95A0` calls the new-game builder first, it supplies destination
`entry_begin + saved_count * 0xBC`. It then populates saved entries at the beginning.
Do not infer presentation order from function call order or sort the list by star ID.
Both builders use independent zero-based source indices, so `(saved, source_index)` distinguishes
two entries whose `source_index` is 0. A star ID may repeat; it is not a globally unique widget ID.

### Candidate population is a runtime operation

`0xBB61C0` removes/refills candidate slots using existing game count, quotas, available terrain
scripts and an underlying star grid. It checks candidates via `0xBB24D0`, may query a fallback
star selector `0xBB3E10`, assigns a selected terrain script (`0xBBADF0`), performs additional
planet-record work (`0xBA8980`) and appends references to +0xDC. These effects are **not ported**.
The fallback query's pseudocode includes unrecovered control/data flow; it is not safe to translate
it into guessed random star positions.

`0xBAC7B0` enumerates resource/property-list type `0xE38475D9` for candidate terrain scripts.
That enumeration is not a ready catalogue of game stars or proof that a package's instance IDs
are star positions.

### Persisted backing is not a simple on-disk 0xBC array

`0xBB4D40` writes through the game's serializer to wide **`stars.db.tmp`** (`0x146616C`) and
renames it to **`stars.db`** (`0x145FB30`). `0xBB8C90` initializes/loads that file via serializer
descriptors, handles a missing/unusable database path, loads planet record containers, then invokes
candidate refill. The loader has incomplete stack/calling-convention recovery in Ghidra; actual
instructions are retained. It does **not** establish a sufficiently known byte format to write
a safe stars.db parser yet. No save/database writes are added to OpenSpore.

A bounded filename search under `build/bin/Debug` found no stars-named files in this workspace.
This says nothing about other installations or the user's actual Spore profile; those paths were
not searched. We do not invent an empty-ready database from that negative result.

## Recovered GGE record fields

Constructor `0xB26CE0`, copy `0xB26F90`, builders `0xDF7CB0` and `0xDF7F30`:

| GGE 0xBC record field | Bounded behavior implemented |
|---|---|
| +0x30 resource triple | Saved placeholder `{0x06837044,PNG,0x02605BA3}`; new entry initially zero |
| +0x7C star ID | from source record +0x70 (`0xBA6380`), separate from per-list index |
| +0x80/+0x84/+0x88 position | raw source +0x3C vector (`0x5C66D0`), copied unchanged |
| +0x8C save status | probe true: header equals (0x25,3) -> 0, differs -> 1; probe false: any nonzero output header word -> 2, both zero -> 3; new-entry constructor default=4 |
| +0xA8 stage | new entry 5; saved stage from source +0x4C (`0xBB9C50`), except source flags +0x5C bit 0x80 forces 0 |
| +0xAC/+0xAD | saved/new flag and initial visible flag; builders set visible true |
| +0xB0 | per-source-list index, not star ID or script/AppState ID |
| +0xB8 | original per-entry runtime UI pointer; deliberately NOT represented as a host pointer |

If the terrain property-list query succeeds, resource +0x30 becomes `{property_or_zero,PNG,
0x02E9A2CA}`. Property `0x2BD4F5AC` must be type 10; a missing/wrong-type value yields instance 0.
If the property-list query fails, keep saved placeholder/new zero triple. This is different from
the **separate saved-game thumbnail** around +0x3C, whose hashing/metadata is not projected here.

**Correction found during instruction review:** `0xB27580` produces two header words; the references
at `0x0145F844/48` are static **.rdata constants (0x25,3)**, not a runtime current-save key. The
probe body reads through a serializer and its successful read path returns a predicate on the first
word (`>0x20`). Its calling convention is poorly recovered by Ghidra, so field names for exact
version/compatibility meaning remain open. The model uses `SaveHeaderWords` and neutral statuses,
not invented Current/Other identity labels. Both words are compared independently. Input is an
observed probe result/header pair; no filesystem I/O occurs in this helper.

The new builder also calculates `(position + 500) * 0.001` into scratch values. Those calculations
do **not** replace the raw position written at GGE +0x80. The host does not clamp positions or
use that scratch calculation as its galaxy camera transform.

## Implemented host subset

`engine/gge_catalog.{h,cpp}` builds owned `GameEntry` values from **typed producer metadata**:

- preserves saved-then-candidate ordering, both index spaces and duplicate IDs;
- projects positions, stage flags, image fallback distinctions and save status;
- retains UTF-16 text instead of narrowing it to ASCII;
- distinguishes Unknown source lists from explicitly observed empty lists;
- rejects non-finite coordinates / oversized text and atomically preserves output on rejection.

The host imposes **4096 total entries / 4096 UTF-16 code units per text** as safety limits. These
are not recovered StarManager quotas. The input promises all record fields are observed/resolved;
this helper does not discover scripts/stars, generate names, decode saves or execute manager calls.
It is not ABI-compatible with the pointer-bearing 0xBC original and must not be `memcpy`ed to/from
a stars.db file. No fake binary fixture is called original-runtime evidence.

**No live catalogue producer is attached yet.** The helper is compiled into OpenSpore for the next
integration step but only the verification target supplies controlled metadata today. GGE's existing
MoviePlayer/DBCheck wait is unchanged; no star UI, galaxy scene or game stage is made ready.

## Validation / continuation

`gge_catalog_verify` checks ordering, duplicate identities, both source indices, unchanged world
coordinates, new/saved defaults, flagged stage, all four save statuses plus NotSaved, both header words,
loaded/missing terrain image distinctions, Unknown/empty sources, NaN/infinity, text/count limits
and transactional rejection. Current suite: **19 grouped checks**, using controlled typed inputs.

```powershell
cmake --build build --config Debug --target OpenSpore gge_catalog_verify
.\build\Debug\gge_catalog_verify.exe
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --catalog
python tools/gge_evidence.py --static --catalog
```

Follow-up: [stars.db serializer prefix](starsdb.md) now recovers framing, the six metadata fields and
StarManager descriptor names. Its prefix inspector does not decode records/references or validate
a real save. Next concrete sources are the context/object protocol consumed by `0xBB8C90` and the
galaxy/star grid populated by its generation path. Separately, implement a real host MoviePlayer
and DBCheck adapter before driving the state-2 scene. Do not use the typed test entries as shipped
save data or mark a synthetic starfield as recovered gameplay.
