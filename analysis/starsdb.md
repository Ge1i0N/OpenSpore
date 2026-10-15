# stars.db — recovered serializer prefix, not a galaxy reader

Reviewed **2026-10-15**. Authority: GA unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
All original-code findings below are **static**; no original save has been replayed or validated.

## Result and boundary

`engine/serialized_frame.*` implements a bounded, transactional reader for the field-directory
frame used by GA's serializer. `engine/starsdb.*` inspects two consecutive frames: the save/generation
metadata, then opaque StarManager fields. That default prefix mode decodes six metadata fields.
Additional explicit modes decode wire lists/grid and standalone StarRecord fields, **not** object
resolution, a complete save or the catalogue producer.

`starsdb_inspect` is a read-only tool for an **explicitly selected serializer-stream file**. There is
no profile scan, no automatic save loading and no save/database writing. The runtime still has no
catalogue producer: neither a valid prefix nor an observed empty directory makes the galaxy ready.
Outer transport modes must be resolved before claiming that every physical `stars.db` starts with
this stream. Bytes after the second frame remain unvalidated and are reported, not ignored as EOF.

## Recovered path

| Point | Evidence | Observation |
|---|---|---|
| Loader `0xBB8C90` | [retained function](audit/gge-catalog/00bb8c90.c.txt), [header call instructions](audit/starsdb/header-load-call.asm.txt), [manager call instructions](audit/starsdb/manager-load-call.asm.txt) | configures metadata serializer with descriptor `0x1689C50` and tag `0x01A80D26`; later configures StarManager with `0x156C668` and the same tag |
| Writer `0xBB4D40` | [retained function](audit/gge-catalog/00bb4d40.c.txt) | writes the metadata/manager serialization path to `stars.db.tmp`, then replaces the named database; **not ported** |
| Descriptor registration `0x692840` | [function](audit/starsdb/00692840.c.txt) | descriptor-driven field callbacks, rather than raw struct dumps |
| Frame writer `0x6928C0` | [instructions](audit/starsdb/006928c0.asm.txt), [length patch](audit/starsdb/frame-length-patch.asm.txt) | reserves directory, emits payloads, then patches frame length/count and each field's serialized size |
| Frame reader `0x693DE0` | [instructions](audit/starsdb/00693de0.asm.txt) | reads directory, dispatches known nonzero fields and seeks over unknown payloads |
| Scalar read/write `0x93A6F0/0x93A9E0` | [read](audit/starsdb/0093a6f0.asm.txt), [write](audit/starsdb/0093a9e0.asm.txt) | framing words use byte swapping, i.e. big-endian in the stream |

The loader's virtual calls and stream wrapper are not all resolved. `0x693CA0 → 0x6938D0` creates
the serialization context through manager ports; [context instructions](audit/starsdb/006938d0.asm.txt)
and [object creation](audit/starsdb/0069e3a0.asm.txt) are retained, not treated as proof of raw-file
transport. Pseudocode has incomplete stack/calling-convention recovery; actual instructions govern.

## Frame layout

At an explicit stream offset `O`:

| Offset | Encoded value |
|---|---|
| `O + 0` | BE `u32` tag; this call site expects `0x01A80D26` |
| `O + 4` | BE `u32 L`, bytes **after the first eight bytes**, not total frame size |
| `O + 8` | BE `u32 N`, number of directory entries |
| `O + 12` | `N` pairs of BE `(field_id, payload_byte_size)` |
| `O + 12 + 8*N` | consecutive payloads, in directory order |
| `O + 8 + L` | next frame / end of this frame |

The original descriptor is a runtime structure, not a file record. Its field offsets and callback
pointers never become a disk stride; neither does GGE's `0xBC` presentation record.

The host parser verifies every bound before indexing and requires directory sizes to consume the
frame exactly. It preserves unknown IDs, duplicates and zero-size entries as spans. It limits the
table to **128** entries, motivated by the original registry capacity/fixed reader stack table;
rejecting larger input is a **host safety policy**, not an observed graceful GA rejection path.
Failure leaves the caller's output unchanged. This is a structural parser, not an implementation
of the original allocation/context/reference protocol.

## Six metadata fields

The [initializer `0x1325DB0`](audit/starsdb/01325db0.asm.txt) constructs descriptors in BSS.
[Machine-readable notes](audit/starsdb/schema.json) are transcribed from that initializer, not
obtained by reading the destination addresses as executable file bytes.

| Field | ID | In-memory offset | Serialized payload |
|---|---|---|---|
| `savedGameVersionOnSave` | `0x03F140B6` | `+0x04` | BE `u32` |
| `savedGameVersionOnGenerate` | `0x03F140B7` | `+0x00` | BE `u32` |
| `buildVersionOnSave` | `0x03F140B8` | `+0x18` | BE unit count, then native x86 UTF-16LE units |
| `buildVersionOnGenerate` | `0x03F140B9` | `+0x08` | BE unit count, then native x86 UTF-16LE units |
| `timestampOnSave` | `0x03F140BA` | `+0x4C` | nine BE `u32` words |
| `timestampOnGenerate` | `0x03F140BB` | `+0x28` | nine BE `u32` words |

The version callbacks are `0x572810/0x572840`. Wide-string callbacks `0xAC8700/0xAC8730` reach
`0x693860/0x692E10`: the count uses the swapped word routine, but units use
`0x93A670(..., raw=1)`, bypassing its swap loop ([instructions](audit/starsdb/wide-native-units.asm.txt)).
Thus **not everything is big-endian**. The reader preserves raw UTF-16 units, including embedded
zeroes; it does not interpret a zero unit as end-of-field. A host limit of 1,048,576 units protects
metadata inspection. The CLI escapes non-ASCII/control units rather than lossy console conversion.

Timestamp callbacks `0xB26950/0xB26970` use `0xB318C0/0xB31A10`, nine scalar operations. The inspector
retains all nine words without imposing calendar validity or timezone semantics. Missing/zero-size
known fields stay optional; the last nonzero duplicate wins. These choices model the inspected
field-dispatch subset, not original default construction or schema migration.

## StarManager fields and wire references

The [initializer `0x1326120`](audit/starsdb/01326120.asm.txt) provides 19 descriptor names. Important
catalogue dependencies are now identified exactly:

| Field | ID | In-memory offset | Read/write callbacks |
|---|---|---|---|
| `mStarRecordGrid` | `0x03FABAD0` | `+0xC8` | `0xBB6950 / 0xBA7500` |
| `mStarterStarRecords` | `0x03EA8676` | `+0xDC` | `0xBAE0B0 / 0xACA670` |
| `mSavedGameStarRecords` | `0x03EA8679` | `+0xF0` | `0xBAE0B0 / 0xACA670` |
| `mPossibleStartLocations` | `0x0563C450` | `+0x118` | `0xBAE0B0 / 0xACA670` |

The list reader [0xBAE0B0](audit/starsdb/links/00bae0b0.c.txt) now establishes the wire shape:
`BE count`, then for every entry a `BE token`; if token is nonzero, a `BE class_id` follows. A null
reference therefore occupies four bytes, while a non-null reference occupies eight. Starter/Saved
lists request interface `0x03E4353B`; the StarRecord class ID is `0x03E4353C`. These are wire object
identities, **not** `mKey`, star IDs or file offsets. The writer actually emits its original object
pointer word as the token; on reload it is an opaque class-scoped map key, never an address to
dereference in this process. It is not a stable cross-save star identifier.

The context read/write paths [0x6A06A0 / 0x6A05F0](audit/starsdb/links/006a06a0.c.txt) show that GA
uses a context object to resolve or create referenced objects. The host implementation deliberately
does not run that protocol. `starsdb_links.*` validates lists and grid cells as unresolved wire
references only. Empty, absent and unknown fields remain distinct; no reference is dereferenced and
no catalogue entry is published.

Host limits: at most 1,048,576 references per decoded list/grid, 65,536 grid cells, and aggregate
references are counted across all cells of one grid. These are allocation safety policies, not GA
galaxy sizes. Unknown class IDs and duplicate tokens remain inspectable; parsing does not establish
that they support the requested StarRecord interface. Nonfinite position words remain raw data;
the separate catalogue builder rejects nonfinite coordinates when projecting typed inputs.

The grid reader [0xBB6950](audit/starsdb/00bb6950.c.txt) constructs cells and calls a nested record
reader. The list writer [0xACA670](audit/starsdb/00aca670.c.txt) writes a BE count and context
references; the values are not raw star coordinates.

### StarRecord field subset

The descriptor initializer [0x13277D0](audit/starsdb/links/013277d0.c.txt) provides 14 fields:
`mKey`, `mPosition`, `mType`, `mTechLevel`, `name`, `mFlags`, `citizenSpeciesKey`, `mStarterWorldID`,
`empireID`, `lastObservedTime`, `savedGameTimeStamp`, `savedGameVersionMajor`,
`savedGameVersionMinor` and `planetCount`. The host inspector decodes these fields at an explicit
frame offset. Position preserves three BE float bit patterns; `citizenSpeciesKey` preserves GA's
memory-word order (+8,+4,+0). The StarRecord read function [0xBBA6E0](audit/starsdb/links/00bba6e0.c.txt)
also performs resource/property and side-effect fixups; those are not ported. A shared frame tag does
not prove that any arbitrary frame is a StarRecord: `starsdb_inspect --star-record` is a caller-selected
structural decode, not class verification.

[Context index/object envelopes](starsdb-context.md) now have an explicit decoded-stream reader,
CRC checks and structural `(class_id, token)` lookup. This does not load a physical DBPF save or run
StarRecord post-load fixups. Next: attach the actual resource adapter and complete the grid/record
graph sufficiently to feed [the typed catalogue builder](gge-catalog.md). Also recover the generated-new-galaxy
path: absence of a saved database does not mean no stars.
MoviePlayer/DBCheck adapters and state-2 scene/UI/effects remain separate runtime dependencies.

## Evidence helper correction

`tools/pe.py` previously mapped virtual zero-fill tails past a section's `SizeOfRawData` into unrelated
following file bytes. `file_off` now rejects those addresses; `read` rejects spans crossing the raw
boundary as well. This prevents false descriptor bytes at `0x1689C50`. Two constructed Python tests
cover backed edges, BSS and crossing/negative reads; no original binary is needed for those tests.
The evidence exporter also asserts that this descriptor address has no raw-file mapping.

## Reproduction and checks

```powershell
cmake --build build --config Debug --target OpenSpore starsdb_verify starsdb_inspect starsdb_links_verify
.\build\Debug\starsdb_verify.exe
.\build\Debug\starsdb_links_verify.exe
# Read-only, optional: pass an explicit serializer-stream file; no profile discovery.
.\build\Debug\starsdb_inspect.exe <explicit-local-file>
.\build\Debug\starsdb_inspect.exe <explicit-local-file> --links
.\build\Debug\starsdb_inspect.exe <explicit-StarRecord-frame> --star-record
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py
python tools/starsdb_evidence.py --static
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py --links
python tools/starsdb_evidence.py --static --links
python -m unittest discover -s tools/tests -p 'test_*.py'
```

`starsdb_verify`: **240 constructed checks PASS**. Includes an independently literal BE fixture,
six metadata fields/mixed string endianness, unknown/zero/duplicate IDs, explicit embedded offsets,
the 128-entry boundary, every truncation before the second frame ends, malformed sizes/tags/counts,
atomic rejection and reporting an opaque trailing span. This is **not real-save corpus validation**.
An optional new destination argument writes only a labelled synthetic fixture for testing the CLI;
it refuses an existing destination. The engine is not wired to this inspector or those fixtures.

Final regression: Debug x86 build warning-free; catalogue/DBCheck/Intro/binding/SPUI/package/scissor
suites PASS, six Python tests PASS, 45 maintained Markdown link checks PASS. Live `Main → GGE` and
`DevLauncher → CreatureEditor` with `-w -devDirs` both exit 0, aborts/asserts 0. The original
MoviePlayer-Unknown gate remains in place. These runs test host regressions, not database loading.

`starsdb_links_verify`: **362 constructed checks PASS**. Covers variable-width null/non-null references,
list/grid bounds and atomic failure, manager dispatch, all 14 StarRecord fields, UTF-16 units, float
bit patterns, resource-key order, duplicates and explicit offsets. These are instruction-derived
fixtures, not real saves. The exporter retains 28 focused link/StarRecord functions under
[audit/starsdb/links](audit/starsdb/links), file-backed vtables, schema notes and the [new Ghidra
seeds](audit/starsdb-links-seeds.tsv). No original target execution, save discovery, database mutation
or game-completion claim is involved.
