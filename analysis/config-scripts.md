# Config scripts (the game's own options language)

Authority: the shipped files `Data/Config/*.txt` (and `DataEP1/Config/*.txt`) - loose text,
not packaged. They are what the disassembly loads as the `properties` / `ConfigManager`
configs (see `analysis/content-loading.md`).

## Files

- `Config/Properties.txt` - declares player options: `property <Name> <0xid> <type>`.
- `Config/ConfigManager.txt` - the rules/defaults script (per video card, CPU, sound).
- `Config/VideoCards.txt` - video-card database.

## The language (keywords seen in ConfigManager.txt)

`set`, `seti`, `setb`, `setOption`, `boolProp`, `intProp`, `setResolution`, `setVariables`,
`if` / `elseif` / `else` / `endif`, `trace`, `include`, `property`, `alert`. It is a small
imperative scripting language (values, conditions, includes, alerts).

## The option table (from Properties.txt)

`47` properties. Their ids are a MIX: `2` are FNV-1-lower hashes of the
name, the rest are explicitly assigned (a contiguous block like `0x0461709d..0x046170a7`).
So the file itself is the authoritative name->id table; do not assume a hash.

| name | id | type | id source |
|---|---|---|---|
| `OptionVersion` | `0x04754439` | uint32 | explicit |
| `OptionDefaultsSet` | `0x0461709d` | bool | explicit |
| `OptionShadows` | `0x0461709e` | uint32 | explicit |
| `OptionTextureDetail` | `0x0461709f` | uint32 | explicit |
| `OptionEffects` | `0x046170a0` | uint32 | explicit |
| `OptionBakeQuality` | `0x046170a3` | uint32 | explicit |
| `OptionLighting` | `0x046170a6` | uint32 | explicit |
| `OptionPlanetQuality` | `0x046170a7` | uint32 | explicit |
| `OptionGameQuality` | `0x05c9482d` | uint32 | explicit |
| `OptionDOF` | `0x4c0dd839` | uint32 | fnv1-lower |
| `OptionPhotoRes` | `0x0473b8cb` | uint32 | explicit |
| `OptionVideoRes` | `0x0473b8cc` | uint32 | explicit |
| `OptionAudioPerformance` | `0x7d8ed666` | uint32 | fnv1-lower |
| `OptionFullScreen` | `0x046170a2` | uint32 | explicit |
| `OptionFitToScreen` | `0x046170a5` | uint32 | explicit |
| `OptionDiskCacheSize` | `0x046170a4` | uint32 | explicit |
| `OptionScreenSize` | `0x046170a1` | uint32 | explicit |
| `OptionTutorialsEnabled` | `0x04ea96cb` | uint32 | explicit |
| `OptionLoggedInYouTube` | `0x05664a8b` | uint32 | explicit |
| `OptionShowHints` | `0x05b5bb5e` | uint32 | explicit |
| `OptionBuddiesOnly` | `0x05de7b4a` | uint32 | explicit |
| `OptionEdgeScroll` | `0x0636ec26` | uint32 | explicit |
| `OptionExpireOld` | `0x0626f940` | uint32 | explicit |
| `OptionArchiveExpired` | `0x0626f9c0` | uint32 | explicit |
| `OptionExpireDays` | `0x0626f958` | uint32 | explicit |
| `OptionDownloadSize` | `0x0685a63c` | uint32 | explicit |
| `OptionUpdateLimit` | `0x0685a785` | uint32 | explicit |
| `OptionUpdateLimitDays` | `0x0685a821` | uint32 | explicit |
| `OptionCaptureUI` | `0x0631621a` | bool | explicit |
| `OptionExplainSporepedia` | `0x0604a51a` | uint32 | explicit |
| `OptionExplainPaintLikeThis` | `0x0604a561` | uint32 | explicit |
| `OptionCellControls` | `0x0679b833` | uint32 | explicit |
| `OptionCreatureControls` | `0x0679b85e` | uint32 | explicit |
| `OptionTribeControls` | `0x0679b868` | uint32 | explicit |
| `OptionCivControls` | `0x0679b873` | uint32 | explicit |
| `OptionSpaceControls` | `0x0679b880` | uint32 | explicit |
| `UserName` | `0x040512ef` | string8 | explicit |
| `Password` | `0x040512f4` | string8 | explicit |
| `PlayOffline` | `0x040e3d98` | bool | explicit |
| `PromptOnStartup` | `0x0440a514` | bool | explicit |
| `YTUserName` | `0x05664bf5` | string8 | explicit |
| `YTPassword` | `0x05664bf6` | string8 | explicit |
| `AlwaysFullscreen` | `0x05dd4647` | bool | explicit |
| `NumFramesToBuffer` | `0x05c97448` | int | explicit |
| `MacSpecificText` | `0x061b67b6` | bool | explicit |
| `Support51Audio` | `0x063ab656` | bool | explicit |
| `HasShownExportToolEULA` | `0x087c4363` | bool | explicit |

