# Real saves probed (beta UserData) — DBPF 2.0 vs the 3.0-only adapter

> **Current verdict, 2026-10-16:** the four real save files in the beta `UserData/Games/`
> tree are all DBPF **version 2**, minor 0, `field_3c` 3. The read-only adapter
> (`source/engine/starsdb_dbpf.cpp`) requires exactly 3.0/index3 and refuses them —
> the version boundary is now confirmed on real files, not just constructed inputs.
> **A fresh GA save (created 2026-10-16, `%APPDATA%/SPORE/Games/Game0/`) confirms the
> other side: `stars.db` is DBPF 3.0/index3 with the exact root and context keys the
> adapter looks up, and `planetRecords.pkp` carries 65 records in the exact
> type/group/instance-stride space the planet reader expects.** No `stars.db` existed
> on either disk before; the corpus is now one fresh (+ one older) GA save, early game.

## Files (read-only; originals untouched on their own disk)

| file | size | version | records | verdict |
|---|---|---|---|---|
| `Games/EditorOfflineUser.db` (beta) | 180 B | 2.0 | 1 | DBPF_OK_NOT_STARSDB |
| `Games/planetRecords.pkp` (beta) | 96 B | 2.0 | 0 | DBPF_OK_NOT_STARSDB (empty DB) |
| `Games/planetRecords.pkt` (beta) | 96 B | 2.0 | 0 | DBPF_OK_NOT_STARSDB (empty DB) |
| `Games/snapshot.snp.pld` (beta) | 1,386,805 B | 2.0 | 285 | DBPF_OK_NOT_STARSDB |
| `Game0/stars.db` (GA fresh) | 2,408,029 B | 3.0/index3 | 7 | STARSDB_SHAPED, root + context FOUND |
| `Game0/planetRecords.pkp` (GA fresh) | 22,277 B | 3.0/index3 | 65 | STARSDB_SHAPED, type 0x05220CB8/group 1 |
| `Game0/planetRecords.pkt` (GA fresh) | 96 B | 2.0 | 0 | DBPF_OK_NOT_STARSDB (empty DB) |
| `Game0/GGEUserData.dat` (GA fresh) | 408 B | 3.0/index3 | 1 | STARSDB_SHAPED, root triple |
| `Game0/PlanetScripts.pld` (GA fresh) | 246,046 B | 3.0/index3 | 249 | STARSDB_SHAPED |
| `Game0/lastSave.pld` (GA, 2026-09-27) | 144,965 B | 3.0/index3 | 23 | STARSDB_SHAPED |
| `Game0/*.spo` creature (GA fresh) | 19,683 B | 3.0/index3 | 12 | STARSDB_SHAPED |

Machine-readable reports: [audit/save-probe/](audit/save-probe/) (one JSON per file).
Payload-level structural decode of the fresh `stars.db` (records, index, envelopes,
CRCs, frame headers): [starsdb-real](starsdb-real.md), report
[audit/starsdb-real/fresh.json](audit/starsdb-real/fresh.json).

## What the bytes say

* `EditorOfflineUser.db`: header valid under the `pkg.cpp` validator replay
  (magic, version 2 ≤ 3, `index_offset + index_size == filesize` exactly — the same
  invariant all 57 shipped packages hold). Index kind 7, 36 bytes = 4 + 4·3 + 20·1.
  Raw index dwords: kind `7`, shared `01897c18 01897c18 00000000`, record
  `instance 01897c18 / offset 0x60 / packed 0x80000030 / mem 0x4a`. Read with the
  3.0-measured kind/stride layout this is exactly the save-root key the adapter
  looks up first ([starsdb-dbpf](starsdb-dbpf.md): root
  `(0x01897C18, 0x01897C18, 0x01897C18)`), with a plausible RefPack payload
  (high bit set, packed 48 < mem 74, offset right after the header). The layout
  reading itself is labeled interpretation: the 2.0/3.0 index-layout equivalence
  is **not** established, which is why the probe reports dwords, not keys.
* `snapshot.snp.pld`: kind 5, 285 records, `12 + 24·285 = 6852` — the
  prefix/stride arithmetic holds on 2.0 data too, at least structurally. First
  record group `0x4084B000`, instance `0x278361C2` (same instance family as the
  `PlanetCache` file names). Payloads not decoded.
* `planetRecords.pkp/.pkt` (beta): valid empty DBPF 2.0 headers (count 0, no index).
  Real [planet-record](planet-record.md) containers with no rows — not absent
  format, absent content.
* GA fresh save (`%APPDATA%/SPORE/Games/Game0/`, created 2026-10-16, early game):
  * `stars.db`: 3.0/index3, kind 4, 7 records. Root `(0x01897C18 x3)` FOUND (unique)
    and context index `(0x0179D304, 0x0179D310, 0)` FOUND (unique), plus five class
    records under instance `0x0179D304` (types `0x0424EA07/0x018EB9D2/0x03A3AC9F/`
    `0x03E4353C/0x042232A1`) — exactly the root → index → per-class shape
    `inspect_dbpf_context` consumes. The adapter's exact-key design is confirmed
    on a real save; execution of the readers against it is still future work
    (needs an MSVC build for the `*_inspect` tools).
  * `planetRecords.pkp`: 3.0/index3, kind 7, **65 records, all type `0x05220CB8`
    group 1**, instances `0x044EF041, 0x034EF041, 0x024EF041, …` — the
    `star.mKey + i·0x01000000` stride from [planet-record](planet-record.md),
    confirmed byte-for-byte on a real file.
  * `GGEUserData.dat`: 3.0/index3, one record = the root triple again.
  * `PlanetScripts.pld` (249 records, kind 5) and `lastSave.pld` (23 records,
    kind 5): 3.0/index3, record type `0x00B1B104` — same family as the beta
    snapshot's. Payloads not decoded.
  * Creature `.spo`: 3.0/index3, kind 4. Six files, examined in creation order
    in the next section.
* No `stars.db` existed on either disk before this save. The corpus is now two GA
  saves (fresh + 2026-09-27 `Game0.old`), both early game — a save per stage would
  widen it, but none was requested.

## Creature saves in creation order

Six `.spo` in `%APPDATA%/SPORE/Games/Game0/`, created 21:39 → 21:46; the player
confirms they follow stage order: cell → creature → tribe → civilisation → space
(the 4th file, `snapshot.spo`, is the transition auto-save). All are DBPF 3.0/index3
with root + context records; only the class-type sets are compared here (payloads
untouched).

| stage | size | records | fingerprint |
|---|---|---|---|
| cell | 19,683 B | 12 | minimal set; unique marker class `0xABCDEF00`; no editor menagerie |
| creature | 173,834 B | 39 | editor menagerie (`0x05599E3B..0x05599F83` block, `0x0609EEDF`, `0x06ABEE7E`, `0x14D9597F`, `0x3226D7D4`, `0x34066DBA`, `0x52AA6122`, `0xD406729F`) |
| tribe | 199,943 B | 43 | same family as creature: 31 shared types, 8 only-in-creature, 12 only-in-tribe |
| auto (`snapshot.spo`) | 75,484 B | 40 | different shape: `0x018C40BC..` block, `0x0244FB08/0x02E1923C/0x032229FF/0x03D5C325`, class `0x042232A1` (the same ID growing in `stars.db`); **no StarRecord class** |
| civilisation | 78,191 B | 40 | **identical 40-type set to the snapshot** — same DB shape, different creature |
| space | 20,845 B | 26 | another shape: embeds StarRecord `0x03E4353C` + `0x03A3AC9F`; shares only 9 types with cell |

Readings (structural only): the snapshot sits between the tribe and civilisation
saves, so it reads as the tribe→civ transition auto-save, reused by the manual
civilisation save seconds later; the space save carries its home star. The cell
save's `0xABCDEF00` reads like a dev/test marker class, retained as an observation,
not a claim about its role.

## Tool

`tools/real_save_probe.py <file>` replays the `pkg.cpp` validator clauses and
the `starsdb_dbpf.cpp` strict gate in order, then reports header fields, kind,
record keys (3.0 only) or raw index dwords, and `--key INST,TYPE,GROUP` lookup.
Exit 0 = probed (any verdict), exit 2 = could not probe. Constructed-input
tests: `tools/tests/test_real_save_probe.py` (no game data required).

## Boundaries

* Beta build (2008-02-19) vs GA oracle: versions differ, so no 2.0 byte may be
  cited as GA behaviour. The version gate exists precisely for this.
* `Cells/` in the beta UserData is empty; `Creatures/` holds 3 editor-model
  files (one instance `0x28B152FA` in three type/group variants) — noted for the
  creature-stage work, not examined here.
* GA saves are now observed (one fresh + one older, both early game, both 3.0),
  but the corpus is still narrow: one galaxy each, no per-stage saves, no profile
  scan was run and none is authorized by this step. Key-shape confirmation is not
  payload decoding — no record bytes were decompressed or parsed here.
