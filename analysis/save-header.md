# Saved-game header — actual file versions, not StarRecord metadata

Reviewed **2026-10-15**. GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static analysis + constructed inputs, **not** a real save or original-runtime replay.

## Implemented

`engine/save_header.*` decodes the **first field frame** of an explicitly supplied save root resource.
`save_header_inspect <explicit-save-DBPF>` selects root key `(0x01897C18,0x01897C18,0x01897C18)` through
the existing [read-only DBPF adapter](starsdb-dbpf.md). Unlike the stars.db reader, it does not expect
two metadata/manager frames and does not require context/class streams to inspect this header.

The result owns decoded scalar/triple/timestamp values; frame spans are resource-local numeric
locations only. Trailing root bytes are reported **unvalidated**, not silently treated as parsed.
No save-path discovery, `.spo` filename construction, profile scan, save/database write or live
catalogue publication. Unsupported DBPF versions retain the adapter's explicit failure behavior.

## Descriptor and probe evidence

`0xB27580` uses descriptor `0x01568B08`, constructed at
[`0x131B210`](audit/starsdb/save-header/0131b210.asm.txt). This is a different schema from both stars.db
generation metadata and StarRecord's recorded saved-version fields.

| Field | Serialized ID | Original memory offset | Host decoding |
|---|---|---|---|
| `mFileVersionMajor` | `0x04162D88` | `+0x0C` | BE u32, optional until observed |
| `mFileVersionMinor` | `0x04162D89` | `+0x10` | BE u32, optional until observed |
| `mObjectCountMap` | `0x04162D8A` | `+0x14` | opaque span, not schema-validated |
| `mTimeStamp` | `0x04162D8B` | `+0x30` | nine BE u32 words, not calendar validation |
| `mGameMode` | `0x04162D8C` | `+0x54` | BE u32, no enum mapping invented |
| `mbSnapshot` | `0x064BC1CF` | `+0x58` | opaque, no bool/default inference |
| `mCurrentPlanetKey` | `0x064BE534` | `+0x5C` | three BE words read into memory +8,+4,+0 |

The recovered probe copies object `+0x0C/+0x10` to two output words. Instructions
[`0xB27838 CMP [major],0x21 / SETAE`](audit/starsdb/save-header/probe-output-words-guard.asm.txt) establish
**unsigned major >=33**, independent of minor. This is not signed comparison, file-existence proof,
successful full deserialization, ownership/identity or complete save compatibility.

The original probe does not check the frame-reader return at this call site. The host intentionally
requires successful bounded structural parsing **and both observed version fields** before exposing
`HeaderProbe`. Missing/zero-size fields remain Unknown, not inferred constructor-default zero.
Malformed/missing DBPF data is an error; it is not silently translated into an observed false/zero
probe pair. Original file-missing branches remain a separate I/O contract to implement later.

GGE compares those file words with static `(37,3)` in `0xDF7F30`. The existing catalogue projector's
neutral statuses stay unchanged. **StarRecord `savedGameVersionMajor/Minor` (`0x03F1389D/9E`) and
stars.db `savedGameVersionOnSave/Generate` (`0x03F140B6/B7`) are not substitutions for this header.**

## Saved-list producer still needs planet resources

[`0xBBA7B0`](audit/starsdb/save-header/00bba7b0.asm.txt) materializes planet records from keys
`{star.mKey + i*0x01000000, type 0x05220CB8, group 1}` for `i < planetCount`. The key arithmetic is
32-bit original behavior, not object token arithmetic. [`0xBBA900`](audit/starsdb/save-header/00bba900.asm.txt)
selects a filtered vector when star flags have `0x40`, excluding planets whose `+0x2C` bit `0x100`
is set (`0xB8DAC0`). GGE then copies the **first selected planet's name at +0x18** into the string
later used with `.spo` and the save probe. Therefore a StarRecord name alone is not a complete
save-stem producer. Follow-up [PlanetRecord subset](planet-record.md) now decodes the structural
prefix/needed fields and selection into a typed saved adapter; actual naming/query services and
catalogue discovery remain absent.

Stage remains `StarRecord.mTechLevel +0x4C`, except star flags `0x80` force 0 in the GGE entry; new
candidates use stage 5. Header `mGameMode` is not used to replace that stage. StarRecord timestamp
and the save header timestamp are retained independently, not interpreted as proof of source readiness.

## Validation

`save_header_verify`: **116 constructed checks PASS**. Includes a literal independently encoded
header, all truncations, exact field sizes, optional/unknown/duplicate fields, opaque map/snapshot,
timestamp/resource word order, unvalidated tail, unsigned predicate boundary and high-bit values,
explicit synthetic DBPF read and atomic missing-root failure. Controlled inputs feed actual decoded
file words to the existing typed GGE projector for its four statuses and flag-stage override.
This is a header-to-projection boundary test, **not a recovered live catalogue producer**.

```powershell
cmake --build build --config Debug --target OpenSpore save_header_verify save_header_inspect starsdb_postload_verify
.\build\Debug\save_header_verify.exe
.\build\Debug\starsdb_postload_verify.exe
.\build\Debug\save_header_inspect.exe <explicit-save-DBPF>
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py --save-header
python tools/starsdb_evidence.py --static --save-header
```

Optional verifier destination writes a new labelled synthetic header DBPF, refuses existing paths,
and does not modify any save. Retained [schema notes](audit/starsdb/save-header/schema.json) accompany
11 exported GA functions, three instruction extracts and [initializer seed](audit/save-header-seeds.tsv).
Also see the **corrected** [BAD_DATA post-load branch](starsdb-postload.md); the former canonical-name/
mismatch-cleanup/position-cache interpretation was wrong and is no longer implemented.

Final Debug x86 build warning-free. New 116 header checks + 15 corrected post-load decisions and
all prior stars.db/catalogue/DBCheck/Intro/binding/SPUI/package/scissor regressions PASS. Synthetic
header CLI smoke prints (37,3), predicate true, three unvalidated tail bytes. Six Python tests and
48 maintained Markdown link checks PASS. Live Main → GGE and launcher → CreatureEditor with
`-w -devDirs` exit 0, aborts/asserts 0; MoviePlayer remains Unknown. No real save was tested.
