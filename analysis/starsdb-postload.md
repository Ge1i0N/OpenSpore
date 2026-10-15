# StarRecord post-load observations

Reviewed **2026-10-15**. GA static evidence only; no resource manager or original save executed.

## Confirmed sequence

`0xBBA6E0` first deserializes the StarRecord frame, then:

1. clears the runtime cache at `record +0x80`;
2. passes the resource key at `record +0x74` through `0x4DF5C0`, which can try a default profile
   and instantiate a `Simulator/cSpeciesProfile` object;
3. compares the UTF-16 name at `record +0x60` against the global string at `0x0151AC94`;
4. on mismatch, enters `0xC8C180` and `0xBBA620` cleanup/re-registration paths.

The host does not execute steps 2 or 4. `observe_post_load` exposes only explicit observations:
`Unknown`, `Matched`, `Missing`, `Changed`, plus `position_cache_reset=true`. It never injects a
canonical name, synthesizes a profile/resource, rebuilds the cache, sends an event or marks a record
ready. This is intentionally weaker than GA behavior until resource-manager and side-effect ports
are recovered.

The source projection separately queries terrain property-list type `0xE38475D9` and property
`0x2BD4F5AC` at `0xDF7E35/0xDF80FF`. Missing/wrong type stays absent; it is not a fallback permission.

`starsdb_postload_verify` passes constructed cases for profile matched/missing/unknown, canonical
name matched/changed and side-effect suppression. Static exports and schema are under
[`audit/starsdb/postload`](audit/starsdb/postload). This does not validate a real save.
