# GA PropManager terrain lookup evidence

Static-only extracts from the selected GA-unpacked binary, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No original process was executed and no package priority was inferred from this evidence.

## Corroborated contract

- `FUN_0067DDF0` returns the GA PropManager global at `0x015FD8A8`.
- GA's PropManager vtable at `0x01409210` has `0x006A60E0` at `+0x2C`, matching the SDK
  `IPropManager::GetPropertyList` slot. The same vtable has `0x006A61F0` at `+0x34`, matching the
  SDK `AddPropertyList` slot.
- `0x006A60E0` uses the property-manager cache keyed by instance/group and, on a cache miss,
  forwards the resource request through its resource-manager/factory path. It returns a property
  list through the caller output and retains/releases the object. Its exact resource priority and
  all cache-state branches are not promoted to a host policy.
- `DF7CB0` obtains `starter_world_id` by reading StarRecord `+0x58`, calls the manager's `+0x2C`
  with group `0xE38475D9`, then asks the returned list for property `0x2BD4F5AC`. A type-10 value
  becomes the terrain image instance; a missing/wrong type yields zero.

The package probe found byte-identical type-10 property payloads for all eight selected IDs in both
explicit `PatchData.package` and `Spore_Game.package`. That is a useful data correlation, but it is
not an observation that `006A60E0` ran, that either package won a runtime priority decision, or that
candidate refill completed.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 120 tools/gge_evidence.py --terrain-prop
```

The generated C/instruction files in this directory are bounded persistent-project extracts. Run
the Ghidra wrapper sequentially; concurrent project opens can acquire the same project lock.
