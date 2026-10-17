# Explicit Game0 save capture — first real catalogue input

Reviewed **2026-10-17** from the explicitly selected local files under
`build/bin/Debug/UserData/Games/Game0/` and `Game0.old/`. No profile search was performed, and the
save binaries are not committed to Git.

## Observed files

`Game0` contains:

```text
stars.db              2,565,517 bytes
planetRecords.pkp        33,771 bytes
planetRecords.pkt            96 bytes
PlanetScripts.pld       212,607 bytes
GGEUserData.dat             387 bytes
lastSave.pld             189,486 bytes
*.spo                    17,754 bytes
```

`Game0.old` contains a second `stars.db`, `planetRecords.pkp`, `GGEUserData.dat` and `.spo` set.
Both selected `stars.db` files have DBPF magic/version `DBPF/3`, and the read-only physical context
inspector accepts root `0x01897C18` with exact index/class resources.

Additional bounded DBPF inventory for the explicitly selected `Game0` files:

```text
GGEUserData.dat    1 record, type/group 0x01897C18/0x01897C18
lastSave.pld       325 records across 5 type/group pairs
PlanetScripts.pld  328 records across 5 type/group pairs
planetRecords.pkt  0 indexed records
```

These are container observations only. Their payloads are not promoted to catalogue, candidate or
refill readiness without a format-specific decoder and producer provenance.

`GGEUserData.dat` is a complete decoded serialized frame when its only DBPF resource is selected:
tag `0x01A80D26`, six fields, zero trailing bytes. Observed field IDs/sizes are:

```text
0x066B980A  16 bytes
0x066B980B   4 bytes
0x07CA2686   4 bytes
0x07CA2687   4 bytes
0x07CA2688   4 bytes
0x07CA2689 564 bytes (nested/opaque payload)
```

The host now has a bounded explicit `dbpf_serialized_frame_inspect` tool for this observation. Field
semantics and nested payload meaning remain Unknown; no GGE catalogue or readiness status is inferred
from these IDs.

Comparing the same explicit resource in `Game0.old` gives the same six outer field IDs and the same
five small-field sizes/previews, but the nested `0x07CA2689` field changes from **60 bytes** in the
older save to **564 bytes** in `Game0` after the space-stage play/save. The decoded outer frame sizes
are 152 and 656 bytes respectively. This is a real observed save delta, not a semantic interpretation:
the nested payload remains opaque until its schema is independently recovered.

## Real StarManager observations

**Candidate-source clarification:** the persisted `starters` list is the descriptor-backed
`mStarterStarRecords` at manager `+0xDC`, the vector exposed to GGE by `BA6450`. Its saved references
are already decoded/resolvable, not absent. What remains unobserved is the post-load/post-`BB61C0`
runtime vector, terrain property results and refill completion. See
[community-reference cross-check](community-references.md). This does not alter the admission gate.

### Starter-reference comparison prompted by the SDK cross-check

Read-only reinspection of the three explicitly authorized current `Game0/stars.db` files resolves
**all six starter references in each corpus** to decoded StarRecords. The existing 16-reference
inspector preview therefore covers each complete starter list, not just a sample. In serialized order:

| Corpus | Starter StarRecord keys |
|---|---|
| `UserData1` (before) | `00B2F02E`, `005AF046`, `0039A02F`, `00599019`, `00B70018`, `0078E041` |
| `UserData` (after) | `00B2F02E`, `005AF046`, `00599019`, `00B70018`, `0078E041`, `00BD9024` |
| `UserDataRonikari` | `00A9702A`, `00B2F02E`, `0069102B`, `0041F020`, `0088D02F`, `00AE3044` |

Before-to-after, `0039A02F` is removed from starters and appears in the saved list, while
`00BD9024` is appended to starters; the five retained starter keys keep their relative order.
Object-pointer tokens and decoded object indices change and are **not** cross-save identities.
All 18 resolved starter rows have observed tech level 0, type 4 and planet count 5. These source
values must not be confused with GGE presentation stage 5 written by the candidate builder.
Ronikari is a separate corpus, not an additional before/after observation of the user's play session.

This establishes a persisted-list transition consistent with the candidate-source interpretation,
not a runtime trace of `BB61C0`, its selection algorithm, or proof of post-load catalogue readiness.
The current check covers `Game0`, not an additional comparison of the `.old` copies.
Reproduce separately for each explicit corpus (substitute only its authorized directory):

```powershell
build/bin/Debug/starsdb_dbpf_inspect.exe build/bin/Debug/UserData1/Games/Game0/stars.db 0x01897c18 0x01897c18 0x01897c18
build/bin/Debug/starsdb_dbpf_inspect.exe build/bin/Debug/UserData/Games/Game0/stars.db 0x01897c18 0x01897c18 0x01897c18
build/bin/Debug/starsdb_dbpf_inspect.exe build/bin/Debug/UserDataRonikari/Games/Game0/stars.db 0x01897c18 0x01897c18 0x01897c18
```

For `Game0\stars.db` the explicit DBPF/context reader observed:

```text
classes=5
StarRecord class 0x03E4353C objects=42010
manager starters=6
manager saved=1
manager black_holes=336
manager possible_starts=44
manager grid=4096 serialized cells
```

The saved manager reference resolves to a real StarRecord:

```text
star_id/key       0x0041C023
position          (-79.1504, -347.477, -0.55087)
flags             0x37
tech_level        5
planet_count      5
name              present on StarRecord
```

The explicitly selected `planetRecords.pkp` contains five wrapped nested DBPF planet resources.
After read-only nested-DBPF unwrapping, all five structural PlanetRecord observations parse and the
selection adapter returns save stem `Золяко`. The `.spo` first-header reader observes
`mFileVersionMajor=37`, `mFileVersionMinor=3`, and the bounded predicate `major >= 33` is true.

## Corpus comparison

The full bounded inspection of the three explicitly selected corpora found:

```text
                         UserData1     UserData       Ronikari
stars.db root bytes       408,914       410,680       901,952
StarRecord objects          42,010        42,010        42,010
saved references                  1             2             2
starter references                6             6             6
black holes                     336           336           336
possible starts                  44            44            44
grid cells                    4,096         4,096         4,096
```

`UserData` contains one additional saved StarRecord after the space-stage experiment: key
`0x0039A02F`, flags `0x1007`, tech level 5, position `(-124.487,-389.697,-0.577652)`, save stem
`Рамант` (the StarRecord name is `Реберилиа`; the former wording confused the two). The original
`0x0041C023` row remains. Ronikari contains two saved rows (`0x009D4020`
and `0x00524036`) and is marked modded/non-vanilla; its structural format is still useful evidence,
but its package-edited semantics are not treated as vanilla behavior.
The post-flight class-count delta is also bounded and interesting: `UserData1` has class
`0x018EB9D2=2121` and `0x042232A1=1`, while `UserData` has `2122` and `2`; StarRecord class
`0x03E4353C` remains `42010`. This indicates new serialized state in other class streams, not merely
an additional StarRecord. The class meanings are not inferred yet.

`GGEUserData.dat` outer frames also differ: `UserData1` decoded frame 656 bytes with nested field
`0x07CA2689` size 564; `UserData` 684/592; Ronikari 4340/4240. Ronikari's small fields also differ
(`0x066B980B` is 12 bytes rather than 4, and fields `0x07CA2687/8` are nonzero). These are observed
save deltas with unknown semantics, not readiness signals.

The nested `0x07CA2689` frame has four fields in all three corpora:

```text
field             UserData1   UserData   Ronikari
0x07DB67DB            172        172       1324
0x07DB67DC            172        172       1324
0x07DB67DD            172        172       1324
0x07DB67DE              4         32        224
```

The post-flight growth of `0x07DB67DE` from 4 to 32 bytes is a real GGEUserData delta; Ronikari's
larger 224-byte variant is modded/non-vanilla comparison evidence. Field semantics remain Unknown.

### Exact counted-table comparison

`tools/gge_userdata_compare.py` now validates the complete decoded nested bytes, not just their
previews. It invokes the explicit C++ inspector with a 30-second subprocess timeout; no Ghidra,
profile search, save writes or runtime code execution is involved. It enforces exact frame/field
lengths, unique expected field IDs and a host cap of 4096 rows per field.

The observed wire shape is three counted six-word tables (`DB/DC/DD`) and a counted word list
(`DE`), all big-endian. Reading each six-word row as two ResourceKey-sized triples is a structural
hypothesis, not a recovered native member/type declaration. Observed counts:

| Explicit save | DB | DC | DD | DE |
|---|---:|---:|---:|---:|
| UserData1/Game0.old | 0 | 0 | 0 | 0 |
| UserData1/Game0 | 7 | 7 | 7 | 0 |
| UserData/Game0.old | 7 | 7 | 7 | 7 |
| UserData/Game0 | 7 | 7 | 7 | 7 |
| UserDataRonikari/Game0.old | 53 | 53 | 53 | 53 |
| UserDataRonikari/Game0 | 55 | 55 | 55 | 55 |

For every inspected file, swapping the two triples in each DB row gives exactly the DD rows as a
multiset, preserving duplicates. DB and DC have equal left-hand triples in stream order. In the
post-flight UserData and both Ronikari variants, DE equals the multiset of third words in the
left-hand triples of DD. This last equality is **false** in UserData1/Game0, where DE is explicitly
empty; that discrepancy is retained, never repaired or replaced by the DD entries.

UserData1/Game0 versus UserData/Game0 has identical DB/DC/DD rows in exact order. Only DE changes,
adding seven words and removing none. Thus the tables themselves did not gain seven new mappings
during this experiment. A native meaning such as candidate stars, visited systems, or refill
completion is not established by these relationships. Ronikari provides a larger valid structural
sample; the data alone does not attribute its larger counts to mods rather than normal play.
Ronikari/Game0.old versus Ronikari/Game0 adds exactly two rows in each of DB/DC/DD and two words
in DE, with zero removals under duplicate-preserving multiset comparison.

Reproduce the before/after comparison (utilities reside in the canonical build output directory):

```powershell
python tools/gge_userdata_compare.py --inspector build/bin/Debug/dbpf_serialized_frame_inspect.exe `
  build/bin/Debug/UserData1/Games/Game0/GGEUserData.dat `
  build/bin/Debug/UserData/Games/Game0/GGEUserData.dat
```

Tests cover all nested-payload truncations, extra tails, invalid counts/IDs, duplicate-aware delta
comparison, relation mismatches, and a child frame attempting to consume its sibling's bytes. The
inspector rejects ambiguous nested selectors and caps the explicit hex dump at 64 KiB. None of these
structural observations changes the catalogue admission gate.

### Correlation with selected package directories

`tools/gge_userdata_correlate.py` compares **every** row against every entry of each explicitly
selected DBPF directory. The triple-order hypothesis is `(group,type,instance)`. Exact matches and
same-group/instance entries with a different type are reported separately. Duplicate keys remain
ambiguous, and zero-size/unreadable-marked entries are never reported as usable payloads. Packages
are inspected independently; no priority merging or automatic folder search is performed.

| Corpus | Selected package | Directory entries | Exact triple matches | DB-right / DD-left same group+instance, different type |
|---|---|---:|---:|---:|
| UserData1 | PlanetScripts.pld | 328 | 0 | 0 of 7 |
| UserData1 | lastSave.pld | 325 | 0 | 0 of 7 |
| UserData1 | Planets.package | 110 | 0 | 0 of 7 |
| UserData | PlanetScripts.pld | 575 | 0 | 7 of 7 |
| UserData | lastSave.pld | 575 | 0 | 7 of 7 |
| UserData | Planets.package | 7 | 0 | 0 of 7 |
| Ronikari | PlanetScripts.pld | 116639 | 0 | 5 of 55 |
| Ronikari | lastSave.pld | 116512 | 0 | 5 of 55 |
| Ronikari | Planets.package | 1638 | 0 | 0 of 55 |

The different type is `0x00B1B104` in the package entries versus `0x011989B7` in the table's right
triples (DB) / left triples (DD). DB-left and both sides of DC had no exact or same-group/instance
matches in these selected directories. The report does **not** substitute one resource type for
another. These are cross-directory correlations, not an established conversion/lookup rule.

Thus the before/after result is stronger than a file-size delta: the seven unchanged GGE mappings
become correlated with seven script-directory entries and the DE word list becomes populated.
Neither causality nor a native meaning such as candidate refill is proven. Missing exact keys are
absent only from the selected directories, not necessarily from other caches or shipped packages.

Ronikari required more than the initial diagnostic inventory cap of 65536 entries: the full
PlanetScripts directory contains 116639 records. The inventory tool now has explicit host limits
of 512 MiB per file, 262144 directory entries and 16 MiB directory bytes, and rejects unparsed
directories rather than reporting an empty successful inventory. Payloads are not decoded by this
comparison; 30-second per-inspector subprocess timeouts bound each probe.

```powershell
python tools/gge_userdata_correlate.py `
  --frame-inspector build/bin/Debug/dbpf_serialized_frame_inspect.exe `
  --inventory-inspector build/bin/Debug/dbpf_inventory_inspect.exe `
  --package build/bin/Debug/UserData/Games/Game0/PlanetScripts.pld `
  --package build/bin/Debug/UserData/Games/Game0/lastSave.pld `
  --package build/bin/Debug/UserData/Planets.package `
  build/bin/Debug/UserData/Games/Game0/GGEUserData.dat
```

Tests preserve exact type/key matching, distinguish related types from matches, retain duplicate
rows/ambiguous directory entries, and reject incomplete inventory output. Raw reports remain local
and save binaries are not committed. Ghidra is not required for these byte/directory observations.

### Related script payloads: direct-key hypothesis checked

`tools/gge_userdata_payload_probe.py` follows only an explicitly selected payload type, not an
automatic type fallback: for each DB right-hand triple it probes the same group/instance with
caller-supplied type `0x00B1B104`. Duplicate directory matches remain ambiguous; no first-match
selection is used. The tool decodes only unique readable resources, hashes the exact decoded bytes,
and tests four literal source-key sequences (BE/LE group-type-instance and BE/LE instance-type-group).
Matches would be byte occurrences, **not** typed references or evidence of native execution.

For the explicitly selected `Game0/PlanetScripts.pld` files:

| Corpus | Table rows | Related payloads decoded | Absent in selected directory | Full source-key sequence hits |
|---|---:|---:|---:|---:|
| UserData1 | 7 | 0 | 7 | 0 |
| UserData | 7 | 7 | 0 | 0 |
| UserDataRonikari | 55 | 5 | 50 | 0 |

The seven UserData payloads are 34751, 38915, 32451, 34747, 32451, 30651 and 32835 decoded bytes.
The five Ronikari payloads are 25562, 13946, 36743, 37472 and 30262 decoded bytes. All selected
payloads decoded successfully; the negative result covers their **complete** bytes, not merely the
256-byte console preview. No literal source key was found in the four tested layouts. This rules out
that simple embedded-key hypothesis within this exact scope, but does not disprove a relationship
through property fields, names, remapping tables, or other byte layouts/resources. In particular,
the related payload type is still not substituted for the GGE table type.

The payload inspector now uses explicit file/directory/decoded-size bounds and strict decimal record
index parsing. Raw dump mode is limited to 1 MiB per resource. The probe caps table size at 64 rows,
reports all missing/ambiguous/blocked entries separately and applies a 30-second subprocess timeout.
No Ghidra restart or new native-analysis claim is involved.

### Property-table framing corroborated on selected script payloads

The original `tools/prop.py` uniform-entry walker rejects the selected UserData payload at its first
field, so it cannot establish property semantics here. A new bounded `tools/prop_table.py` subset uses
the historical layout in `upstream/SporeMaster3/docs/PROP_FORMAT.md` as a hypothesis: BE property
ID/type/flags, scalar or counted array according to flags, LE composite values, and a 16-byte slot
for a scalar ResourceKey (12 key bytes plus four retained tail bytes). The tail was missing from the
initial local scalar-prefix experiment; including it correctly aligns the next header `0x02A907B5`
(type `0x20`, array flags `0x9C`) in all selected payloads. No resynchronization or magic search is used.

All seven UserData payloads parse to exactly **35 fields**, consuming their complete bytes with no
tail. The five available Ronikari payloads parse to **56, 55, 35, 55, 56 fields**, respectively, also
with complete byte consumption. Unsupported types, flags, array strides, duplicate IDs and invalid
counts fail closed. Raw padded bytes remain visible; the script does not replace them with zeros.
This corroborates the selected wire layout, not every `.prop` variant or native method behavior.

Property ID `0x00B2CCCA` contains these observed UTF-16 strings:

- UserData: `GameplayTheme17`, `GameplayTheme04`, `GameplayTheme02`, `GameplayTheme`,
  `GameplayTheme07`, `GameplayTheme19`, `GameplayTheme18`.
- Ronikari: `EP1 Shaman Theme`, `EP1 Bard Theme`, `GameplayTheme11`, `EP1 Warrior Theme`,
  `EP1 Bard Theme`.

The local historical registry labels `0x00B2CCCA` as `description`, `0x00B2CCCB` as `parent`, and
`0x02A907B5` as `modelEffect`; those labels are not recovered native symbols. Scalar keys in the
second field are now extracted with their exact LE memory words and checked independently against
the selected directory (no automatic priority merge or parent traversal). None of the 12 complete
property tables contains field ID `0x2BD4F5AC`. That absence covers only these selected resources;
it does not prove that an inherited or elsewhere-resolved terrain property is absent. No terrain
query was executed and this does not turn Unknown into QueryFailed or Loaded.

The payload probe includes the exact-subset property report alongside byte-pattern results. Tests
cover scalar-key padding, array versus scalar widths, every fixture truncation, unsupported strides,
duplicates, invalid counts and UTF-16 supplementary/invalid sequences. Ghidra was not started.

```powershell
python tools/gge_userdata_payload_probe.py `
  --frame-inspector build/bin/Debug/dbpf_serialized_frame_inspect.exe `
  --inventory-inspector build/bin/Debug/dbpf_inventory_inspect.exe `
  --resource-inspector build/bin/Debug/dbpf_resource_inspect.exe `
  --package build/bin/Debug/UserData/Games/Game0/PlanetScripts.pld `
  --payload-type 0x00b1b104 `
  build/bin/Debug/UserData/Games/Game0/GGEUserData.dat
```

## Explicit UserData package inventory

Package files can provide planet/editor/resource correlation, but they are not substitutes for
StarManager vectors. Bounded DBPF inventory of the explicitly selected corpus found:

```text
                         records / groups
UserData1 Planets.package       110 / 5
UserData  Planets.package         7 / 3
Ronikari Planets.package       1638 / 8
Ronikari Planets backup        1081 / 6
Ronikari EditorSaves.package    178 / 37
Ronikari EditorSaves backup     103 / 34
Ronikari RigblockInfo.package    54 / 1
```

`GraphicsCache.package` and `Pollination.package` did not pass the current DBPF magic validator;
they remain separate cache/format inputs, not silently treated as DBPF. Ronikari's package-edited
resources are useful for format/resource comparison, but are explicitly non-vanilla evidence. No
package inventory result changes the candidate/refill producer gate.

The vanilla `Planets.package` delta is notable: `UserData1` has 105 generic `0x00B1B104` records
plus five `0x2F7D0004` planet-package records, while post-flight `UserData` has seven records, all
`0x2F7D0004`, with several records carrying `packed=0`/nonzero memory sizes. This is an observed
package/cache-state change, not yet a decoded candidate vector or terrain success. Ronikari's
`Planets.package` has 1,638 records, mostly `0x2F7D0004`, consistent with its much larger edited
corpus but not proof of vanilla producer semantics.

## Derived snapshot boundary

A historical temporary derived `GGE_CATALOG_V1` row was inspected, not committed. **It is not a
trusted capture:** its terrain flag encoded a failed query that had not actually occurred, and the
old utility could reuse one header across multiple saved references. The superseding diagnostic
format below removes both problems. Historical output (not a current readiness claim):

```text
saved_status=Available
candidate_status=Unknown
manager_status=Available
starsdb_status=Available
planet_status=Available
refill_status=Unknown
resource_root/index/context/class=Available
refill_append=Unknown
saved_count=1
candidate_count=0
```

Its producer gate is correctly **BLOCKED**. This is because the real save proves one saved entry and
the physical root/index/class/planet observations, but it does not prove the candidate vector after
`BB61C0`, candidate refill completion, terrain property success, or a complete caller catalogue.
No empty candidate list is substituted for the absent/unknown candidate producer.

### Hardened structural capture: `GGE_SAVE_OBSERVATIONS_V1`

`starsdb_gge_capture` no longer writes `GGE_CATALOG_V1`. That production format has no per-row
Unknown terrain representation; putting `terrain=0` there means observed query failure, not an
unexecuted query. The diagnostic JSON is intentionally rejected by the production snapshot loader.
There is no migration that silently upgrades historical derived rows into trusted metadata.

The replacement preserves **every** persisted saved/starter reference in serialized order, including
null, missing-class, missing-object, unsupported-class and duplicate-reference outcomes. Each
resolved row retains token, class, object index, optional StarRecord fields and source index. Empty
observed lists remain different from absent/zero-size fields. Starter tech level is serialized source
data, not GGE presentation stage 5. No starter name post-load service is implied.

Saved rows independently record complete counted planet selection or a blocked/empty-filtered result.
Each optional `--save-header` assigns **one explicit file to one saved index**, never to every row:

```powershell
cmake --build build --config Debug --target starsdb_gge_capture
.\build\bin\Debug\starsdb_gge_capture.exe `
  build\bin\Debug\UserData\Games\Game0\stars.db `
  build\bin\Debug\UserData\Games\Game0\planetRecords.pkp `
  C:\captures\game0-observations.json `
  --save-header 0 build\bin\Debug\UserData\Games\Game0\Золяко.spo `
  --save-header 1 build\bin\Debug\UserData\Games\Game0\Рамант.spo
```

The parent directory must already exist and the output must be NEW. The tool first constructs the
whole JSON, writes an exclusively created sibling, flushes/closes it, then publishes without replacing
any existing destination. Racing writers cannot overwrite each other. Ordinary failure removes only
the sibling this invocation created; interruption can leave a `.partial-*` sibling, not partial final
JSON. This is a publication policy, not a power-loss durability guarantee.

Unicode argv/UTF-8 filesystem paths and strict surrogate-pair conversion preserve Cyrillic and
supplementary characters. Invalid UTF-16/nonfinite coordinates block publication; float output uses
classic locale and `max_digits10`, retaining binary32 round trips (including signed zero). Explicit
header bindings reject duplicate/out-of-range indices, reuse of one file through canonical paths or
filesystem aliases, filename/save-stem mismatches and undecodable headers. Filename agreement and a
valid version header still **do not prove file ownership**; JSON labels the assignment as caller-made
and `ownership_verified=false`. Unassigned rows keep header Unknown. No directory/save discovery runs.

Read-only validation on the three current `Game0` corpora produced:

| Corpus | Saved rows | Persisted starters | Individually assigned headers |
|---|---:|---:|---|
| UserData1 | 1 | 6 | index0 `Золяко.spo`: `(37,3)` |
| UserData | 2 | 6 | index0 `Золяко.spo`, index1 `Рамант.spo`: each `(37,3)` |
| Ronikari | 2 | 6 | index0 `Джос.spo`, index1 `Энтира.spo`: each `(37,3)` |

All starter refs resolve to StarRecords, confirming the earlier order/key comparison. Terrain query,
runtime refill and post-load fixups remain **Unknown**; `production_catalogue=false`. SHA-256 of each
explicitly selected stars/planet/header input matched before and after the run. Reports are local
temporary artifacts, not committed save data. Neither `snapshot.spo` nor unrelated Ronikari files
were selected by this validation. No REA/Ghidra process was launched for this host-tool correction.

Controlled integration tests cover distinct headers with `(37,3)` and `(32,99)`, Unicode paths/names,
float precision, all reference outcomes, Unknown/absent/empty distinctions, invalid bindings,
malformed Unicode/nonfinite rejection, blocked planets, concurrent publication and output protection.

## Next required observation

Use the same explicit save corpus to add a read-only capture adapter that preserves:

- saved row/name/stage/flags/header/planet observations;
- root/index/context/class resource outcomes;
- persisted candidate-source rows separately from the unobserved post-load/refill runtime vector;
- terrain property state as Unknown or Failed unless a real property lookup is captured.

Only after an authorized observation proves the candidate/refill side may a caller snapshot become
scene-admissible. This document is evidence of a real save corpus and structural parsing, not a claim
of native runtime replay or playable GGE readiness.
