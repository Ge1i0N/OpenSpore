# Real saves probed (beta UserData) — DBPF 2.0 vs the 3.0-only adapter

> **Current verdict, 2026-10-16:** the four real save files in the beta `UserData/Games/`
> tree are all DBPF **version 2**, minor 0, `field_3c` 3. The read-only adapter
> (`source/engine/starsdb_dbpf.cpp`) requires exactly 3.0/index3 and refuses them —
> the version boundary is now confirmed on real files, not just constructed inputs.
> One record in `EditorOfflineUser.db` carries the exact documented save-root triple
> `(0x01897C18, 0x01897C18, 0x01897C18)`, so the root identity survived beta to GA.
> No GA save has been observed yet; nothing here authorizes treating 2.0 bytes as 3.0.

## Files (read-only; originals untouched on their own disk)

| file | size | version | records | verdict |
|---|---|---|---|---|
| `Games/EditorOfflineUser.db` | 180 B | 2.0 | 1 | DBPF_OK_NOT_STARSDB |
| `Games/planetRecords.pkp` | 96 B | 2.0 | 0 | DBPF_OK_NOT_STARSDB (empty DB) |
| `Games/planetRecords.pkt` | 96 B | 2.0 | 0 | DBPF_OK_NOT_STARSDB (empty DB) |
| `Games/snapshot.snp.pld` | 1,386,805 B | 2.0 | 285 | DBPF_OK_NOT_STARSDB |

Machine-readable reports: [audit/save-probe/](audit/save-probe/) (one JSON per file).

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
* `planetRecords.pkp/.pkt`: valid empty DBPF 2.0 headers (count 0, no index).
  Real [planet-record](planet-record.md) containers with no rows — not absent
  format, absent content.
* No `stars.db` anywhere on either disk (Steam install or beta tree). The
  `starsdb` wire/context readers therefore still have no real-save corpus.

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
* GA saves (DBPF 3.0, `stars.db`, `*.spore_*`?) remain unobserved: no profile
  scan was run and none is authorized by this step.
