# stars.db — explicit read-only DBPF resource adapter

Reviewed **2026-10-15**. GA unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Original-code evidence is static; verification uses constructed files, not original saves.

## Outcome and boundary

`engine/starsdb_dbpf.*` connects a **single explicitly supplied DBPF** to the recovered prefix,
manager-link, context-index and class-envelope readers. It selects exact keys, decodes raw/RefPack
resources, checks object CRCs, and preserves typed StarRecord fields plus opaque other classes.
Output is transactional: a missing class or malformed late record cannot publish an earlier prefix
as a loaded context. There is **no profile scan, save discovery, priority merge, file/database write,
GA factory execution, name/property fixup, catalogue publication or galaxy-readiness signal**.

The workspace glob `**/stars.db` found no database. This is a bounded negative result, not evidence
that Spore has no saves elsewhere. No user profile directories were searched. No real-save compatibility
claim follows from a successful synthetic fixture. Runtime GGE still waits on its missing services.

## ResourceKey correction and exact selection

ResourceKey **memory** order is `(instance, type, group)`. DBPF **index** order is
`type, group, instance_hi, instance_lo, offset, packed_size, mem_size, compression/flags`.
These orders must not be interchanged.

Evidence chain:

1. [`0x693CA0 → 0x6938D0`](audit/starsdb/dbpf/006938d0.asm.txt) forwards root arguments as a key;
   [`0x69E090`](audit/starsdb/dbpf/0069e090.asm.txt) opens that key through manager/resource ports.
2. [`0x8DB6E0`](audit/starsdb/dbpf/008db6e0.asm.txt) compares all three key words in an index bucket;
   [`0x8DBE20`](audit/starsdb/dbpf/008dbe20.asm.txt) copies them into nodes without reordering.
3. [`0x8DB310`](audit/starsdb/dbpf/008db310.asm.txt) serializes node `+4` as type, `+8` as group,
   emits a zero high-instance word, then node `+0` as low instance. This establishes the translation,
   rather than guessing it from the context constant's name.
4. [Root load/write arguments](audit/starsdb/dbpf/root-key-load.asm.txt) are three immediate
   `0x01897C18` words. They are **not** a descriptor pointer: coincident bytes at that numeric VA
   are irrelevant to the call. [Machine-readable notes](audit/starsdb/dbpf/schema.json) retain keys.

| Resource | instance | type | group |
|---|---|---|---|
| Root serialization stream | `0x01897C18` | `0x01897C18` | `0x01897C18` |
| Context index | `0x0179D304` | `0x0179D310` | `0` |
| One class body stream | `0x0179D304` | class ID, e.g. `0x03E4353C` | `0` |

Correction: earlier host constant names `kContextResourceType` / `kContextIndexInstance` had these
roles reversed. They were not used in the prior decoded-stream readers. Current names are
`kContextInstance`, `kContextIndexType`, `kContextGroup`; the actual serializer bytes are unchanged.

`Package::find_records` returns every 32-bit-triple match with package-local record indices.
`ExplicitDbpfReader::read_unique` distinguishes **Found, NotFound, Ambiguous, PackageRejected,
ReadFailure**. It never silently picks the first duplicate. Nonzero high-instance words are rejected
for relevant resources instead of being collapsed into an apparently valid 32-bit match.

## Narrow versioned host policy

The adapter accepts **DBPF 3.0 / index-version word +0x3C = 3**, not every version accepted by the
generic validator. Preflight bounds the input before generic index allocations:

- maximum package size 256 MiB, 65,536 DBPF records;
- exact directory size for shared mask 0..7; directory must be in range and outside the header;
- relevant resources: high-instance word zero, flags `1`, compression raw `0` or RefPack `0xFFFF`;
- nonzero packed/decoded sizes, at most 64 MiB each; raw packed/decoded sizes must agree;
- decoded root + index + class-stream bytes at most 256 MiB;
- exact root stream EOF, recovered context/index/object checks still enforced.

All are explicit host subset/rejection policies, not claims that GA rejects files identically.
Older save/package/index layouts remain unsupported until evidenced. Every indexed class resource
is required by this inspector, including a zero-count class declaration: a missing stream is not
silently substituted with an empty list. Only StarRecord bodies are typed; other objects are opaque.
These checks do not run interface factories or reproduce all class-specific validation.

The file path is opened **read-only**, read with a size cap, then passed to the same in-memory DBPF
path used in tests. No application state or readiness latch is modified. `PhysicalContextResult`
owns resource bytes so its recorded spans remain valid while that result remains alive.

## RefPack safety correction

The generic decoder previously checked the final decoded size only after expansion. A malformed
stream could append more output than the declared size before failing. Literal, inline-literal,
tail and backreference paths now check remaining decoded capacity **before appending**. This does
not change legal decodes; it bounds malformed expansion. Four constructed negative cases exercise
those paths, and the shipped SPUI/package regressions remain required after this shared change.

## Validation and commands

`starsdb_dbpf_verify`: **266 constructed checks PASS** without filesystem output. Includes all input
truncations, swapped key order, duplicate records, version/mask/count/size policies, resource bounds,
raw + RefPack class bodies, missing root/index/class, bad CRC, unsupported root tail/context version,
transactional failure, and DBPF → root links → context class → StarRecord structural integration.

Optional new-path argument writes a labelled **synthetic** DBPF for CLI smoke testing; it refuses an
existing destination. That mode adds four file-path checks (**270 total**), not original-save checks.

```powershell
cmake --build build --config Debug --target OpenSpore starsdb_dbpf_verify starsdb_dbpf_inspect
.\build\Debug\starsdb_dbpf_verify.exe
# Read-only, caller-selected physical DBPF; explicit root key components are hex.
.\build\Debug\starsdb_dbpf_inspect.exe <explicit-database-file> 01897c18 01897c18 01897c18
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py --dbpf
python tools/starsdb_evidence.py --static --dbpf
```

Evidence exporter retains 11 focused GA functions (C + instructions), three bounded instruction
extracts, key/schema notes and [three new Ghidra seeds](audit/starsdb-dbpf-seeds.tsv). See also
[context envelopes](starsdb-context.md), [fields/references](starsdb.md) and [typed catalogue](gge-catalog.md).

Final regression: Debug x86 warning-free; all four stars.db suites and catalogue/DBCheck/Intro/
binding/SPUI/package/scissor suites PASS. Six Python tests and 46 maintained Markdown link checks
PASS. Live Main → GGE and launcher → CreatureEditor with `-w -devDirs` both exit 0, aborts/asserts 0;
MoviePlayer remains Unknown. These are host regressions, not a real-save compatibility result.

## Next

Validate a real **explicitly selected** database when available, without treating the synthetic DBPF
as one. [Post-load observations](starsdb-postload.md) now separate profile/name/cache findings;
recover the actual name/resource/property fixups and save-stem/header producer;
resolve the manager's catalogue references with explicit missing/unsupported status. Only then attach
metadata to the runtime catalogue. New-galaxy generation and MoviePlayer/DBCheck/scene ports remain
separate dependencies; opening a DBPF does not make Play Spore playable.
