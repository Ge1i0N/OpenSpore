# Community reference repositories

Reviewed **2026-10-17**. These are secondary community sources, not original Maxis source, target
execution evidence, or permission to substitute SDK declarations for the selected GA executable.
Clones reside in ignored `refs/`; their source trees are not vendored into OpenSpore's Git history.
Nothing from either repository was built, injected into Spore, or imported into Ghidra.

## Selected references

| Local clone | Repository | Inspected revision | Purpose |
|---|---|---|---|
| `refs/Spore-ModAPI` | [Spore-ModAPI](https://github.com/Spore-Community/Spore-ModAPI) | `69b27b5b407a2c76e1e701ba27630a77a9ce5d01` | Community native interfaces, member offsets, versioned address tables |
| `refs/SporeModder-FX` | [SporeModder-FX](https://github.com/Spore-Community/SporeModder-FX) | `a482c51d8d37fc8cf416acecc3c00a5b0723f2e5` | Resource/type registry and format-reader comparison |

Both were shallow-cloned at the user's request. Their code carries GPL notices; use them as cited
references, and review licensing before any future code reuse. This pass copies no implementation
from these repositories into the runtime. Future fetches must record the newly inspected revision.

The user-supplied [Community Hub index](https://github.com/Spore-Community/.github/blob/main/profile/README.md)
was also reviewed. PNG/Adventure decoders may be useful when creation PNGs become an active input;
SporeServer is relevant to later online features. They were **not** cloned in this pass because neither
addresses the present offline GGE catalogue boundary. Mod managers/downloaders are not dependencies
of this investigation.

## Useful findings and their authority

### Window lookup, not an unidentified provider registry

At the pinned ModAPI revision, `Spore ModAPI/Spore/UTFWin/IWindow.h` declares:

- `TYPE = 0xEEEE8218`;
- `+0x1C`: `GetControlID`;
- `+0x34`: `GetArea`;
- `+0x38`: `GetRealArea`;
- `+0x64`: `SetLocation`;
- `+0xD8`: `AddWindow` (see the declaration preceding `RemoveWindow`);
- `+0xF0`: recursive `FindWindowByID`.

`Spore ModAPI/SourceCode/DLL/AddressesUTFWin.cpp` maps `UILayout::FindWindowByID` to the address
pair `0x810C10 / 0x810650`. The second matches our selected GA address. This does not make all SDK
address pairs universally interchangeable across binary versions.

These labels fit our retained GA assembly: `810650 -> 8102A0` queries `0xEEEE8218`, compares
`+0x1C`, and optionally searches through `+0xF0`. Earlier notes called these objects generic
"providers" and described a "service ID". The follow-up [per-card lookup investigation](gge-window-lookup.md)
now verifies both the implicit `record+B8` receiver and the exact default asset: `0x02E5A4A8` is
ControlID of root object177 in each `GGEGameStarUIv4` resource, not a camera/global display service.
Native component registration/mount execution remains unobserved. Do not use this declaration to
resolve duplicate ControlIDs with a first-match host heuristic.

### Persisted starter references already exist

`Spore ModAPI/Spore/Simulator/SubSystem/StarManager.h` declares `mStarterStarRecords` at `+0xDC`
and `mSavedGameStarRecords` at `+0xF0`. `AddressesSimulator.cpp` maps the StarManager getter to
`0xB3D200 / 0xB3D3A0`; the latter matches the selected GA.

Crucially, the primary static evidence already agrees: retained GA descriptor
`analysis/audit/starsdb/01326120.c.txt` identifies `mStarterStarRecords`, field ID `0x03EA8676`,
offset `+0xDC`, and `mSavedGameStarRecords`, ID `0x03EA8679`, offset `+0xF0`. The retained
`analysis/audit/gge-catalog/00ba6450.asm.txt` returns `this +0xDC`; GGE consumes this vector.
Our DBPF inspector already resolves the persisted starter references to StarRecord objects.
The current three-corpus check resolves all six per corpus; the before/after key sequences and
their exact scope are recorded in [real-save observations](gge-real-save.md).

Therefore "candidate data absent" is too broad. **Persisted candidate-source references are
available; a post-load/post-refill live vector is not yet observed.** Resolving these saved references
can advance a static capture adapter without claiming that `BB61C0` ran, inventing terrain query
results, or setting refill status Available. The snapshot/scene admission gate is unchanged.

### Property framing and logical resource types

At the pinned SporeModder-FX revision:

- `src/sporemodder/file/prop/PropertyKey.java` reads ResourceKey LE words and skips four more bytes
  for a scalar, but not for an array; array element size is 12.
- `src/sporemodder/file/ResourceKey.java` reads LE words in instance/type/group order.
- `reg_type.txt` labels `0x011989B7` as `plt` and `0x00B1B104` as `prop`.

The padding agrees with exact byte consumption of our 12 selected script payloads; see
[real-save observations](gge-real-save.md). `plt` provides a useful logical-type name, **not proof
of an automatic plt-to-prop remapping rule**. ModAPI's resource manager describes type mapping APIs,
but no concrete mapping for this ID was executed or independently recovered in this pass.

## Next investigation

1. Resolve every persisted starter reference in order, retaining missing/null/unsupported outcomes.
2. Keep that result separate from the runtime refill observation and terrain/property queries.
3. Exact `0x02E5A4A8` default window identity and per-card lookup are now verified; preserve its
   separation from native mounting, unknown runtime geometry and camera acquisition.
4. Use the SDK names to target future REA probes, not to trigger a broad Ghidra import/reanalysis.
   Its included `SDKtoGhidra` importer remains unused until binary-version matching and monitoring
   are arranged explicitly.
