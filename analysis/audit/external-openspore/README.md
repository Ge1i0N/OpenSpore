# External OpenSpore cross-check — selected GA save and Cell lead

Reviewed **2026-10-18**. This audit records what was actually reproduced from the pinned
`Ge1i0N/OpenSpore` reference clone. The clone is ignored under `refs/thirdparty/` and is not part of
the OpenSpore build or Git history.

## Identity and method

```text
repository: https://github.com/Ge1i0N/OpenSpore.git
revision:   51ba127f9da0f57fdb01a9b71b8bd557fcd8f965
clone:      refs/thirdparty/Ge1i0N-OpenSpore
save:       build/bin/Debug/UserData/Games/Game0/stars.db
planet:     build/bin/Debug/UserData/Games/Game0/planetRecords.pkp
```

The save and package files are local, explicitly selected and uncommitted. No profile discovery,
save write or gameplay service was run.

This is a **reproducibility/corroboration pass**, not a new native authority. The fork contains
code with overlapping ancestry to this project; in particular, normalized comparison found our
`source/engine/starsdb_context.cpp` and the reference copy byte-for-byte identical. The reference
Python decoder is a separate execution path, but its comments intentionally mirror the same
structural contracts. Therefore this audit can confirm that the reported bytes pass the bounded
readers; it cannot promote native producer, refill, camera or gameplay readiness.

## Real `stars.db` result

Reference command:

```powershell
python refs/thirdparty/Ge1i0N-OpenSpore/tools/real_save_probe.py `
  build/bin/Debug/UserData/Games/Game0/stars.db --list-keys 12

python refs/thirdparty/Ge1i0N-OpenSpore/tools/starsdb_real_decode.py `
  build/bin/Debug/UserData/Games/Game0/stars.db --star-values
```

Observed output:

```text
DBPF 3.0 / minor 0 / index3
records=7, kind=4, prefix=8, stride=28
context index: version=1, classes=5, total=46255
0x03E4353C StarRecord envelopes=42010, crc_fail=0
all StarRecord values: ok=42010, fail=0
mKeys_unique=42010
nonfinite_positions=0
names=42010 present + 0 empty
unknown_ids=[]
```

The seven records are:

```text
root      (0x01897C18, 0x01897C18, 0x01897C18)
index     (0x0179D304, 0x0179D310, 0)
classes   0x0424EA07, 0x018EB9D2, 0x03A3AC9F, 0x03E4353C, 0x042232A1
```

The local C++ reader was then run against the same explicit file:

```powershell
build/bin/Debug/starsdb_dbpf_inspect.exe `
  build/bin/Debug/UserData/Games/Game0/stars.db `
  0x01897c18 0x01897c18 0x01897c18
```

It independently reached the already-supported structural boundary:

```text
classes=5
0x018eb9d2 objects=2122
0x03a3ac9f objects=1
0x03e4353c objects=42010
0x042232a1 objects=2
0x0424ea07 objects=2120
manager_links starters=6 saved=2 black_holes=336 possible_starts=44 grid=4096
```

The two saved references resolve to the same typed rows already used by the host preview:

```text
0x0041C023  position=(-79.1504,-347.477,-0.55087) flags=0x37 tech=5 planets=5
0x0039A02F  position=(-124.487,-389.697,-0.577652) flags=0x1007 tech=5 planets=5
```

The current selected file hashes are retained here to prevent accidentally comparing a later save
under the same path:

```text
stars.db          7F7AC0FE6F4146DB16B69BEB5B6A14DB15BFF3AE4FB1049C3AB8D1D889ACF9A9
planetRecords.pkp 95FF0FBA2726B61AC0267AD03FE3A048725833FECD1341C30791B7D4D6499464
```

## Real `planetRecords.pkp` result

The existing local reader was run for saved StarRecord `0x0041C023` with `planet_count=5` and
`flags=0x37`:

```text
resources 0x0041C023, 0x0141C023, 0x0241C023, 0x0341C023, 0x0441C023
all five structural records reached the nested DBPF boundary
prefix=(37,3), inner key=(1,1,1)
selected source_index=0, save_stem="Золяко"
```

This confirms the saved-row → planet/header/terrain adapter input used by
`GGE_HOST_REAL_SAVE_V1`. It does not execute the original PropManager query or candidate refill.

## CellGame lead status

The pinned fork provides a useful decoder and audit for these resource families:

```text
cell 189       structure 144       populate 23
world 13       loottable 48        look_table 9
random_creature 10   prop 1        backgroundMap 1
advect 28      density 5            xls 1
```

The current OpenSpore build directory does not contain `CellGame.package` or
`CellGame_Layers.package`, so the decoder could not be run against this workspace's shipped-data
corpus. No Cell resource count or gameplay field from the fork is promoted by this audit. When an
authorized GA `CellGame` package is available, the next bounded check is:

```powershell
$env:SPORE_DATA = '<explicit GA Data directory>'
python refs/thirdparty/Ge1i0N-OpenSpore/tools/cell_game.py `
  --json <new-audit-output>.json
```

Only byte-correlated resource layouts should enter our implementation. Position, balance, AI,
camera, growth, HP and other seed values remain host policy until GA evidence exists.

## Verdict and next action

```text
save structural shape:       corroborated on selected local save
StarRecord field walk:       corroborated (42,010/42,010 values accepted)
planet selected-row input:   corroborated by local C++ reader
native catalogue producer:   Unknown
post-load/profile fixups:    Unknown
candidate refill:            Unknown
CellGame package correlation: blocked (package absent locally)
gameplay readiness:           Unknown
```

Next useful action is not to copy the fork decoder. It is to use this known-good save as a bounded
fixture for a local aggregate report or to attach the already-typed saved rows to the host preview,
while keeping native readiness statuses unchanged.
