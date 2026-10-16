# stars.db payload decode — first real-save structural validation

> **Current verdict, 2026-10-16:** the fresh GA `stars.db` (2,408,029 bytes, early
> game, `%APPDATA%/SPORE/Games/Game0/`) decodes end to end at the structural level:
> all 7 DBPF records decompress to their index `memSize`, the context index parses
> as version 1 / 5 classes / 46,225 objects with exact count-sum equality, every
> class stream walks envelope-to-envelope with **zero CRC-32/BZIP2 failures**
> (46,225 envelopes checked), the root opens with the `0x01A80D26` tag and exactly
> the six documented metadata field IDs, and the first StarRecord body is a
> 14-field `0x01A80D26` frame. Field *semantics* (links resolution, StarRecord
> values, catalogue publication) are still static-only: nothing here executes the
> C++ readers or the original game.

## How it was read

`tools/starsdb_real_decode.py <stars.db>` (read-only, explicit file, no scans):
DBPF index walk → per-record RefPack decode via `tools/refpack.py` (itself derived
from the original's instructions) → context-index parse → envelope walk with CRC →
frame-header reads of the root and first StarRecord bodies. Report:
[audit/starsdb-real/fresh.json](audit/starsdb-real/fresh.json).
Constructed-input tests: `tools/tests/test_starsdb_real_decode.py` (CRC pinned by the
published `123456789 → 0xFC891918` vector, no game data needed).

## Numbers

| check | result |
|---|---|
| records decoded to `memSize` | 7/7 (6 RefPack + 1 raw: the 52-byte context index) |
| context index | version 1, 5 classes, total 46,225; 2107+1+42010+1+2106 = 46,225 exact |
| envelope counts vs declared | 5/5 match (2107, 1, 42010, 1, 2106) |
| CRC failures | 0 across all 46,225 envelopes |
| root first frame | tag `0x01A80D26`, 6 fields: `0x03F140B6..0x03F140BB` in documented order |
| first StarRecord frame | tag `0x01A80D26`, 14 fields: `0x03D58BF1..F7`, `0x03E95E04`, `0x03F1389A..9E`, `0x051A1188` |

The StarRecord tail (`0x03F1389D/9E`) is the pair the C++ reader decodes as
saved-game version major/minor; the other twelve are spans, not values, at this step.

## What this confirms and what it does not

* Confirms, on real bytes: the frame layout ([starsdb](starsdb.md)), the six-field
  metadata schema, the index payload shape, envelope framing, the CRC variant and
  its coverage rule ([starsdb-context](starsdb-context.md)), and the root/index/
  per-class key design ([starsdb-dbpf](starsdb-dbpf.md)).
* Does not: decode field values, resolve a single reference, run any C++ reader,
  or touch the runtime catalogue. The writers (`0xBB4D40` etc.) remain unported.
* Corpus: one fresh save plus one older early-game save, same machine. Per-stage
  saves would widen it; none exist yet.

## Boundaries carried over

* The 2.0/3.0 index-layout equivalence is still not established ([real-saves](real-saves.md));
  this decode path is 3.0-only by construction.
* CRC-checked does not mean fully integrity-protected in general: records longer
  than 1,000,008 bytes have an unchecksummed tail by the original's own cap rule.
  No envelope here hit the cap (largest is 19,706 bytes), so every CRC in this
  save covers its full record.

Note: the StarRecord class stream decodes to 10,531,246 bytes with 42,010
envelopes — large but far below every host aggregate cap, and every envelope
still chained exactly with no trailing bytes.