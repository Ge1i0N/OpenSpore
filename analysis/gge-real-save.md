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
caller-selected new snapshot:

```powershell
cmake --build build --config Debug --target starsdb_gge_capture
.\build\Debug\starsdb_gge_capture.exe `
  build\bin\Debug\UserData\Games\Game0\stars.db `
  build\bin\Debug\UserData\Games\Game0\planetRecords.pkp `
  build\bin\Debug\UserData\Games\Game0\Золяко.spo `
  C:\captures\game0-derived.txt
```

The tool reads the explicit DBPF/context/classes, resolves the single saved manager reference,
unwraps the nested planet DBPF resources, reads the explicit save header, and emits one saved row.
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
