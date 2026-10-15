# StarRecord post-load decisions — corrected BAD_DATA branch

Reviewed **2026-10-15**. GA static evidence only; no resource manager or original save executed.

## Correction to the previous observation model

Earlier code/report called `0x0151AC94` a canonical-name pointer, treated name mismatch as cleanup,
and labelled runtime word `+0x80` a position cache. **Those interpretations were incorrect.**

- File-backed `.data` word `0x0151AC94` initially points to `0x013F9738`, UTF-16 **`BAD_DATA`**.
  This is static initial-image evidence, not a capture of a live writable global.
- `0xBBA77D JNZ 0xBBA7A0` skips regeneration for unequal names. **Equality** enters regeneration.
- `0xC8C180` generates/assigns a name, choosing locale/random-name services by record `mType`.
  `0xBBA620` is an apply-name/notification path, not container cleanup. Old analyst-seed names in
  retained pseudocode are superseded by the instructions and this correction.
- `+0x80` is cleared, but its purpose is not established by that write. `mPosition +0x3C` is untouched.

History is retained in prior local commits; the incorrect observation API has been replaced, not
kept as a compatibility layer. In particular **unavailable name input never becomes Changed**.

## Confirmed order (`0xBBA6E0`)

1. Deserialize the StarRecord fields, retaining the returned status.
2. Write `+0x80 = 0`; call species-profile lookup `0x4DF5C0` with `citizenSpeciesKey +0x74`.
   On a null result write byte `+0x50 = 1`. A nonnull result does not explicitly clear that byte here.
3. If the runtime name vector is empty, call name generator `0xC8C180` once.
4. Compare resulting UTF-16 text with `BAD_DATA`, case-sensitive and zero-terminated.
5. If equal, call the generator again; if the resulting vector is empty, call it one further time.
   Then call the name setter `0xBBA620`, even if the retry is empty. There is no unbounded retry loop.
6. Return the original deserialize status. Running post-load code is not itself a success proof.

`0x4DF5C0` can use manager default key `+0xA4`, materialize a species profile, and update its cache.
`0xC8C180` reads type via `0x801A60 -> +0x48`: type 1 uses locale group `0x2DB6DAD3`/instance
`0x0591BD91`; type 2 uses name-source ID `0xDEB2D943`; types 3..12 use `0x7CAB064C`. These dependencies
are **not executed** by the host decision helper.

`observe_post_load` now returns profile **Unknown/Found/Missing** and ordered `NameCall` requirements.
Caller-supplied observations after each requested generation allow the helper to examine the next
branch. Missing observations block that decision subset; omitted serialized name stays Unknown
rather than silently using original constructor defaults. All original services, generation, setter,
notification and writes remain suppressed. `name_blocked=false` is not full post-load/catalogue readiness.

The separate terrain query uses list type `0xE38475D9` and property `0x2BD4F5AC`. A loaded list with
missing/wrong-typed property yields image instance 0; a failed list query keeps the prior image triple.
It is not permission to fabricate a loaded terrain service.

## Checks and evidence

`starsdb_postload_verify`: **15 corrected controlled checks PASS**. Ordinary names, empty versus
omitted, BAD_DATA equality/case/suffix/embedded zero, maximum ordered calls, empty retry, unknown
generation result, profile null/non-null and no mutation. No real resource services executed.

Evidence: [`audit/starsdb/postload`](audit/starsdb/postload), refreshed sentinel/schema notes and
[`save-header/postload-empty-and-equality.asm.txt`](audit/starsdb/save-header/postload-empty-and-equality.asm.txt).
See [actual file-header decoding](save-header.md) for the separate producer boundary.
