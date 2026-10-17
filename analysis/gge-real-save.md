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
`Реберилиа`. The original `0x0041C023` row remains. Ronikari contains two saved rows (`0x009D4020`
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

A temporary derived `GGE_CATALOG_V1` row was inspected, not committed:

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

The reproducible bridge is now `starsdb_gge_capture`; it accepts only explicit paths and writes a
caller-selected new snapshot. It preserves **all** resolved saved manager references rather than
choosing the first one; this matters because the post-flight `UserData` corpus has two saved rows:

```powershell
cmake --build build --config Debug --target starsdb_gge_capture
.\build\Debug\starsdb_gge_capture.exe `
  build\bin\Debug\UserData\Games\Game0\stars.db `
  build\bin\Debug\UserData\Games\Game0\planetRecords.pkp `
  build\bin\Debug\UserData\Games\Game0\Золяко.spo `
  C:\captures\game0-derived.txt
```

The tool reads the explicit DBPF/context/classes, resolves every saved manager reference, unwraps the
nested planet DBPF resources, reads the explicit save header, and emits one row per saved reference.
It deliberately emits candidate/refill Unknown and terrain QueryFailed until those observations are
provided by a real producer/property capture. It never scans or modifies UserData.

## Next required observation

Use the same explicit save corpus to add a read-only capture adapter that preserves:

- saved row/name/stage/flags/header/planet observations;
- root/index/context/class resource outcomes;
- candidate vector/refill state as Unknown until directly observed;
- terrain property state as Unknown or Failed unless a real property lookup is captured.

Only after an authorized observation proves the candidate/refill side may a caller snapshot become
scene-admissible. This document is evidence of a real save corpus and structural parsing, not a claim
of native runtime replay or playable GGE readiness.
