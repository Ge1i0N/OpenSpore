# stars.db context — index, object envelopes and structural lookup

Reviewed **2026-10-16**. GA unpacked SHA-256:
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static instructions only; no original target execution. One fresh save has since been
structurally cross-checked read-only in [starsdb-real](starsdb-real.md); field semantics
and the live protocol stay static-only.

## Implemented boundary

`engine/starsdb_context.*` reads an **explicit decoded context index** and one **explicit decoded
class stream**, checks record lengths/checksums, decodes StarRecord fields when the class matches,
and looks up a wire reference by **(class_id, token)**. Unknown class bodies remain opaque.
`starsdb_context_inspect` exercises that path read-only. No scanning, magic search, original factories,
pointer dereference, property/name fixups, gameplay readiness or runtime catalogue publication.

This extends [the prefix and reference readers](starsdb.md); a separate [physical DBPF adapter](starsdb-dbpf.md)
now supplies decoded resources with strict version/key checks. The default `starsdb_inspect` input is a serializer stream, not a
universal claim that every physical save starts with the shared frame tag.

## Context identity and resource boundary

- [Context writer `0x69FB90`](audit/starsdb/context/0069fb90.asm.txt) emits an index resource whose
  ResourceKey memory words are `(instance, type, group) = (0x0179D304, 0x0179D310, 0)`; per-class
  keys are `(0x0179D304, class_id, 0)`. These key words select resources, not embedded byte offsets.
- [Context reader `0x69F1D0`](audit/starsdb/context/0069f1d0.asm.txt) opens that index, then one
  resource per declared class. It creates/reuses objects and invokes their virtual read method.
- `0x69F950` is a generic class-bucket lookup/insert helper. **Correction to an earlier intermediate
  description:** the outer map key is class ID, not token. `0x69E9E0` first finds that class bucket,
  then the nested token entry, and creates/reuses an object. `0x6A0110` registers writer objects;
  the port does not execute those factories or rebuild original intrusive ownership.
- [Loader construction](audit/starsdb/context/loader-context-construction.asm.txt) creates
  `0x8DA2A0`'s DBPF implementation, wraps it in `0x69FA60`'s serializer manager, and opens context.
  The [file-backed vtables](audit/starsdb/context/vtables.json) and forwarding methods connect
  manager slots to DBPF open/resource paths (`0x8D8CA0`, `0x8DA6F0`, `0x8D8F70`). The physical-package
  adapter and exact key translation are now implemented as the bounded [DBPF subset](starsdb-dbpf.md);
  save-version coverage beyond it remains open.

An object token is the original writer's pointer-shaped word, **not** StarRecord's `mKey`. The host
retains it as an opaque class-scoped identity. A reference with the right token but wrong class
cannot resolve to a star; null, missing class, missing object and unsupported class stay distinct.

## Context index

Payload, all BE `u32`:

```text
version (=1), class_count, total_objects
class_count * { class_id, object_count }
```

The [writer's final-index instructions](audit/starsdb/context/writer-context-final-index.asm.txt)
establish the third word as total emitted objects; do not treat it as a global default class.
Host checks require exact EOF, unique class IDs, count-sum equality, at most 4096 classes and
1,048,576 aggregate objects. These are explicit safe-subset rejection policies, **not** reproduced
GA recovery behavior. The original class-reader guard is `< 1,000,000` records; the host cap is not
a claim about valid original galaxy size or limits. A zero-count class remains observable.

## Class object envelopes

Each record at offset `O`:

| Offset | Encoded value |
|---|---|
| `O+0` | BE total record byte size, **including** the 12-byte header |
| `O+4` | BE CRC word |
| `O+8` | BE nonzero object token |
| `O+12` | object-specific body; StarRecord is the shared-tag field frame |
| `O+total` | next record |

Unlike field-frame length, envelope length includes its own header. The writer reserves length/CRC,
writes token and object body, patches length at `O`, seeks to `O+8` to calculate CRC, then patches CRC
at `O+4`. The reader compares CRC before object creation/read. This is sequential length-driven
framing; there is no search for `0x01A80D26` inside opaque payloads.

For class `0x03E4353C`, the host checks the StarRecord frame's length **before** passing it to the
field reader, so it cannot consume bytes from the next envelope. Unknown class bodies retain spans
and CRC only. Duplicate tokens within one class and zero body tokens are rejected as host policies.
Failure never publishes earlier valid records from the same failed class stream.

## CRC variant and coverage limit

[`0x932CD0`](audit/starsdb/context/00932cd0.asm.txt) builds a normal/MSB-first table using polynomial
`0x04C11DB7`. [`0x932D70`](audit/starsdb/context/00932d70.asm.txt) starts with `0xFFFFFFFF`, updates
from the high CRC byte and complements at the end. This parameter set matches **CRC-32/BZIP2**,
not zlib's common reflected CRC32; independent `123456789` check value is `0xFC891918`.

[`0x69EC20`](audit/starsdb/context/0069ec20.asm.txt) saves/restores stream position and calculates
over the requested bytes. The caller uses **min(total−8, 1,000,000)** bytes starting at `O+8`
([cap instructions](audit/starsdb/context/checksum-length-cap.asm.txt)), including token but excluding
length/CRC. Consequently a large record's tail is **not checksum-protected**. The verifier tests
both sides of this boundary; a passing CRC is not advertised as full-file integrity.

## Controlled validation

`starsdb_context_verify`: **126 constructed checks PASS**. Independent literal index/envelope/CRC,
every truncation and single-byte mutation of the short literal, count/duplicate/size bounds,
CRC-correct malformed field frames, adjacent-record confinement, unsupported class bodies, same
`mKey` with different tokens, reference-list → indexed StarRecord lookup, aggregate index limits,
the million-byte CRC boundary and atomic output failure. Not a real save or original-runtime replay.

```powershell
cmake --build build --config Debug --target OpenSpore starsdb_context_verify starsdb_context_inspect
.\build\Debug\starsdb_context_verify.exe
# Exact decoded resource streams selected by caller; no profile/database discovery.
.\build\Debug\starsdb_context_inspect.exe <decoded-index> 03e4353c <decoded-StarRecord-class-stream>
refs/ghidra/venv/Scripts/python.exe tools/starsdb_evidence.py --context
python tools/starsdb_evidence.py --static --context
```

Optional verifier argument creates a **new** caller-selected directory containing labelled synthetic
decoded streams for CLI smoke testing; it refuses an existing directory. It never writes a save.
Evidence: 23 functions (C + instructions), vtables, bounded extracts and [schema notes](audit/starsdb/context/schema.json).

Final regression: Debug x86 warning-free; prefix/link/context and catalogue/DBCheck/Intro/binding/
SPUI/package/scissor suites PASS. Six Python tests and 46 maintained Markdown link checks PASS.
Live `-w -devDirs` Main → GGE and launcher → CreatureEditor both exit 0, aborts/asserts 0;
MoviePlayer remains explicitly Unknown. These are host regressions, not real database loading.

## Root serialization resource

The loader and writer pass three equal words `0x01897C18` to the context serializer factory at
`0x693CA0/0x693D10`. Static inspection shows these are ResourceKey arguments used as a value, not a
pointer to a descriptor or a file offset. The host therefore records the key tuple but does not read
bytes from virtual address `0x01897C18`. A coincident file-backed mapping at that numeric address
does not turn these immediate key values into a descriptor pointer.

## Next dependency

The [explicit read-only DBPF adapter](starsdb-dbpf.md) now supplies these streams on constructed
fixtures. Structural validation of a real selected database is done in
[starsdb-real](starsdb-real.md) (index/envelopes/CRCs/frame headers, one fresh save).
Require all needed classes/objects and resource/name fixups before feeding typed catalogue
metadata. Missing classes or unsupported versions stay Unknown/error, not observed-empty
or ready. Field-value decoding, new-galaxy generation, save discovery/probing,
MoviePlayer/DBCheck and scene/UI/effects remain separate work.
