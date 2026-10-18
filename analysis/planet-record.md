# PlanetRecord → saved-game name → typed GGE metadata

Reviewed **2026-10-17**. Static GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
GA instructions establish the structural record schema. Later explicitly authorized real saves
corroborate the bounded transport subset below (the `Game0`/`Game0.old` planetRecords.pkp corpus,
211 records inspected in full); **no original runtime replay**.

## What this closes — and what is still invisible

`engine/planet_record.*` now structurally reads PlanetRecord payloads, selects the first eligible
planet in original order, and obtains the name used by the saved GGE entry. `engine/gge_saved_adapter.*`
joins decoded StarRecord fields, planet observations, a decoded file-header probe and **explicit**
terrain-query observations into the existing typed saved metadata. Tests send that metadata into
the actual [catalogue projector](gge-catalog.md).

This is a producer **subset**, not runtime discovery or a complete catalogue. It does not publish
`CatalogAvailability`, generate candidates/names, query live services, create a camera, markers,
per-star UI or buttons. GGE on screen is still the diagnostic primary layout. No artwork-only
starfield or fake saved game is used to claim a working galaxy.

The next visible path is already traced: `0xDF9200` creates per-entry `GGEGameStarUIv4`, while
`0xDF6010 → 0xDE3C20` projects world positions; `0xDF8300` creates marker effects. These require
scene/camera/effect services and a genuinely available catalogue. Do not force serialized hidden
panels visible as a substitute. [State-2 dependencies](gge-dbcheck-play.md) remain authoritative.

## Prefix before the PlanetRecord field frame

[`0xB8EBF0`](audit/starsdb/planet/00b8ebf0.asm.txt), its writer
[`0xB8DCC0`](audit/starsdb/planet/00b8dcc0.asm.txt), and nested readers establish this exact order:

```text
BE civilization_count
  each civilization:
    BE city_count
      each city: 14 BE u16 words (28 bytes), then a field frame
    BE vehicle_count
      each vehicle: field frame
    civilization field frame
BE tribe_count; tribe_count field frames
BE event_count; event_count field frames
BE commodity_node_count; commodity_node_count field frames
PlanetRecord field frame
```

The city prefix comes from `0xFF0380`; `0xFF2D30` supplies the two civilization collections.
`0xFF0440`, `0xBC33A0`, `0xBFE410` dispatch the remaining standalone frames. The host walks these
payloads **sequentially**, bounds each frame and leaves its field contents opaque. It does not
validate a civilization/city simulation or reconstruct native pointers. Magic bytes in the city
prefix or an unknown field cannot redirect the reader.

Host limits are 4096 aggregate collection nodes (including cities/vehicles), 128 fields per frame
from the shared bounded parser, 4096 UTF-16 units per planet name, and 64 MiB decoded bytes per
selected planet batch. These are host allocation policies, not GA world quotas. Direct `read_record`
reports an unvalidated tail; the DBPF batch/selection boundary rejects such tails. Failure keeps
the caller's output unchanged even if earlier records were valid.

## Decoded PlanetRecord field subset

Initializer [`0x1322A30`](audit/starsdb/planet/01322a30.asm.txt) constructs descriptor `0x156B758`.
Six fields needed here are decoded; other properties remain bounded opaque spans:

| Field | ID | Runtime offset | Serialized payload |
|---|---|---|---|
| `mKey` | `0x04164F8E` | `+0x184` | BE u32 |
| `name` | `0x03D58B9A` | `+0x18` | BE count + native UTF-16LE units |
| `type` | `0x03D58B9E` | `+0x28` | BE u32 |
| `techLevel` | `0x03D58B9F` | `+0x194` | BE u32 |
| `flags` | `0x03D58B99` | `+0x2C` | BE u32 |
| `mGeneratedTerrainKey` | `0x03D58B9B` | `+0x1A4` | three BE words into memory +8,+4,+0 |

Fields remain optional until observed; zero-size fields do not establish constructor defaults.
Last nonzero duplicate wins. Names preserve all UTF-16 units, including embedded zeroes; this is
not path validation or permission to use that raw text as a filesystem path.

## Counted resource identity and planet selection

`0xBBA7B0` lazily materializes planet records using
`{instance=star.mKey + i*0x01000000 (uint32 wrap), type=0x05220CB8, group=1}` for `i < planetCount`.
The factory [`0xBA6300`](audit/starsdb/planet/00ba6300.asm.txt) assigns this resource identity separately
from the serialized planet `mKey`. Neither is the context's pointer-shaped object token.

`read_selected_records` reads **one explicitly selected DBPF** via exact keys; no fallback/merge,
resource-manager execution or profile scan. The original direct counted-payload path remains for
explicit decoded inputs/controlled fixtures. A separate observed nested transport is now strictly
validated rather than taking the only inner directory entry regardless of its identity.

### Observed nested transport subset

For 50 selected resources in six real `planetRecords.pkp` files, decoded outer bytes are:

```text
00 25 00 03                 # observed prefix, two BE halves (37,3)
DBPF3.0 / index version3    # begins at offset4 of outer decoded resource
  exactly one record:
    key(instance,type,group) = (1,1,1)
    decoded payload = counted PlanetRecord collections + field frame
```

The outer counted key remains `(starKey+i*01000000,05220CB8,1)`; the inner key `(1,1,1)` is neither
that outer identity nor the optional serialized planet `mKey`. The four-byte prefix matches save
version numbers visually, but its native interpretation/dispatch is **not recovered**. The reader
accepts only the observed prefix tuple as an explicitly bounded host subset, not all compatible
save versions.

Nested DBPF now uses the same strict `ExplicitDbpfReader` preflight as the outer context path:
version/index layout, count/stride/bounds, unique exact key, high-key word, flags, compression and
decoded size are checked. A sole wrong-type/group/instance record is rejected, not substituted;
multiple records (even with one matching key) are outside this observed subset. Recognized prefix
with bad magic, or DBPF-at-offset4 with a different prefix, fails closed. No magic scanning occurs.

**Index end is not EOF in 24/50 selected containers.** All referenced payloads/index spans remain
bounded, but 1–5 bytes can remain after the inner index. Those bytes are reported as
`container_tail_bytes`, not treated as decoded PlanetRecord data or assumed padding/garbage.
The inner record itself must be consumed exactly; its field-frame tail remains a hard failure.
Do not add an index-at-EOF rule based solely on shipped main packages.

`ResourceRecord::envelope` retains prefix halves, exact inner key, container size, index end and
unvalidated container-tail size. Direct payloads have no envelope observation. Both encoded outer
transport and decoded inner payload batch sizes are separately capped at 64 MiB. The JSON diagnostic
capture and normal planet inspector expose these distinctions; raw `--resource-counts` previews
intentionally do not imply nested identity validation.

[Corpus summary](audit/starsdb/planet/wrapper-corpus.json) records six package SHA-256 values,
explicit key selections, counts, tail bounds and reproduction. The selected 50 comprise five
planets per explicit star key in `Game0` and `.old`, not an inspection of every planet in Ronikari's
large package. Inputs' SHA-256 values matched before/after. ModAPI's serializer interfaces corroborate
the distinction between serializer stream/record and ResourceKey identity but do not establish this
physical envelope; the targeted SporeModder-FX source search supplied no matching planet wrapper.

A full-corpus pass over all 211 records in `Game0` (114) and `Game0.old` (97) `planetRecords.pkp`
also unwraps and parses cleanly with zero tails and every record named. Observed values: `Game0`
holds 2 civilizations, 22 tribes, 0 events and 61 commodity nodes, with planet types 2 (63) / 1 (21) /
3 (16) / 5 (12) and terrain keys on all 114; `Game0.old` holds 1 civilization, 22 tribes and 46 nodes.
The tribe count stays at 22 while civilizations/nodes grow with play. Reports:
`tools/planet_real_decode.py` and the audit JSONs beside the stars ones.

`0xBBA900` uses the unfiltered list unless star flags include `0x40`. With that flag it excludes
planets whose `flags +0x2C` include `0x100` (`0xB8DAC0`). Order is retained; GGE `0xDF7F30` takes the
**first selected planet name +0x18** as the display/save-stem string. This is not the StarRecord name
and planet tech level does not replace StarRecord stage.

The host requires a complete counted vector with matching resource identities. Unknown later
filter flags cannot be ignored just because an earlier selected name exists. Explicit zero/all-filtered
selection is represented separately from Unknown. The saved metadata adapter blocks no-selection
cases: original shared-string carry behavior in GGE's reused scratch record is not reproduced as
an invented empty save name.

## Planet BAD_DATA rule differs from StarRecord

After field loading, [`0xB8EF9E..D4`](audit/starsdb/planet/planet-name-assignment.asm.txt) calls name source
`0x07E1310A` only when the zero-terminated name equals `BAD_DATA`. It assigns the generated result
only if that result is nonempty **and flags bit0 is clear**. A serialized empty planet name does not
by itself enter this branch; do not apply [the StarRecord empty-name rule](starsdb-postload.md) here.

The host never calls the generator. For an assignable selected BAD_DATA name, a supplied generation
observation is required; Unknown blocks selection, observed empty keeps BAD_DATA. If bit0 prevents
assignment, BAD_DATA remains the selected value without inventing a generator output. No original
record is mutated and no setter/notification runs.

## Typed saved adapter

`adapt_saved_metadata` requires observed StarRecord key, finite raw position, flags, stage (unless
`0x80` overrides it), selected planet name, [file-header probe](save-header.md), and terrain status:

- **Unknown** (including invalid enum) blocks the adapter.
- **QueryFailed** means an actual observed property-list failure; the GGE saved placeholder remains.
- **Loaded** requires an explicit image-property result, including zero for observed missing/wrong type.

The adapter sets both saved display name and save-stem from the selected planet name. It does not
construct `.spo` paths, open derived filenames, equate header versions with identity or attach a live
producer. The test's explicit catalogue Available observations belong only to controlled verification.

## Checks / reproduction

`planet_record_verify`: **547 constructed checks PASS**. Literal independent name fixture, all
prefix/field truncations, every collection route, city prefix and exact node cap, field/text bounds,
duplicate fields, no magic scanning, counted resource identity/wrap, missing/duplicate DBPF entries,
filter/order/Unknown distinctions, BAD_DATA rules, atomic rejection and decoded fields → typed
saved adapter → real GGE projector. Optional new path writes only a labelled synthetic DBPF and
refuses an existing destination. CLI smoke selects index 1 / `Planet Save` after filtering index 0.
Additional nested tests cover every wrapper truncation, each wrong inner-key word, absent/duplicate/
extra inner records, wrong prefix/magic/version/index/flags/compression/size, nested RefPack decoding,
oversized decompression declarations and atomic failure of a later planet. An opaque container tail
passes with its size recorded, whereas a decoded PlanetRecord tail is rejected. Capture integration
tests verify JSON envelope provenance and failure for a sole wrong inner key.

```powershell
cmake --build build --config Debug --target OpenSpore planet_record_verify planet_record_inspect
.\build\bin\Debug\planet_record_verify.exe
# Caller supplies an explicit planet-record DBPF and observed star key/count/flags, not a profile.
.\build\bin\Debug\planet_record_inspect.exe <explicit-planet-DBPF> <star-key-hex> <count-decimal> <flags-hex>
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py --planet
python tools/starsdb_evidence.py --static --planet
```

Evidence: 15 focused functions (C + instructions), three extracts and [schema notes](audit/starsdb/planet/schema.json).
Next: join saved-list context references with selected planet resources and explicit observations,
then focus on state-2 camera/per-entry UI boundaries. Real sources, candidate generation and live
services must remain explicit prerequisites rather than synthetic data or forced completion.

Final Debug x86 build warning-free; all parser/catalogue/DBCheck/Intro/binding/SPUI/package/scissor
regressions PASS, six Python tests and 49 maintained Markdown link checks PASS. Main → GGE and
launcher → CreatureEditor with `-w -devDirs` both exit 0, aborts/asserts 0; the MoviePlayer-Unknown
gate and diagnostic GGE appearance are unchanged. These checks do not demonstrate a rendered galaxy.
