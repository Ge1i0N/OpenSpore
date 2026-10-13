# Config scripts (the game's own options language)

Authority: the shipped files `Data/Config/*.txt` (and `DataEP1/Config/*.txt`) - loose text,
not packaged. They are what the disassembly loads as the `properties` / `ConfigManager`
configs (see `analysis/content-loading.md`). OUR engine reads them too:
`source/engine/config.cpp` parses both files and exposes them via the `cfg` console verb.

## Files

- `Config/Properties.txt` - declares options as `property <Name> <0xid> [type]`.
  `Data/Config/Properties.txt` has 63 (47 typed + 16 untyped); `DataEP1/Config/Properties.txt` adds 76 more.
- `Config/ConfigManager.txt` - the rules/defaults script (per video card, CPU, sound).
- `Config/VideoCards.txt` - video-card database.

## The language (keywords seen in ConfigManager.txt)

`set`, `seti`, `setb`, `setOption`, `boolProp`, `intProp`, `setResolution`, `setVariables`,
`if` / `elseif` / `else` / `endif`, `trace`, `include`, `property`, `alert`. It is a small
imperative scripting language with `$`-variables (e.g. `$Medium`, `$resolutionTarget`) and
hardware-conditional blocks. Our reader records assignments but does NOT evaluate the
conditionals (they depend on the video card / CPU / sound card).

## The option table (from Data/Config/Properties.txt)

`63` properties. Their ids come in THREE forms: 49 explicit (hex or decimal,
including a contiguous block like `0x0461709d..0x046170a7`) and 14 `(hash(Name))` where
the hashed name can DIFFER from the property name (e.g. `property AudioMasterVolume
(hash(mastervolume))`). So the file is the authoritative name->id table; never just hash the
property name. (Our C++ reader resolves all three forms.)

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
| `OptionDOF` | `0x4c0dd839` | uint32 | explicit |
| `OptionPhotoRes` | `0x0473b8cb` | uint32 | explicit |
| `OptionVideoRes` | `0x0473b8cc` | uint32 | explicit |
| `OptionAudioPerformance` | `0x7d8ed666` | uint32 | explicit |
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
| `PrefsClearGraphicsCache` | `0x043f2ae3` | bool | hash(PrefsClearGraphicsCache) |
| `AudioMasterVolume` | `0xaded51d5` | float | hash(mastervolume) |
| `AudioSFXVolume` | `0x3af239c4` | float | hash(sfxvolume) |
| `AudioMusicVolume` | `0x8bdd39ec` | float | hash(musicvolume) |
| `AudioVOXVolume` | `0x09a0474a` | float | hash(voxvolume) |
| `AudioSpeakerMode` | `0x49d4c74d` | uint32 | hash(speakermode) |
| `AudioMuteAll` | `0x899eb157` | bool | hash(muteall) |
| `UserName` | `0x040512ef` | string8 | explicit |
| `Password` | `0x040512f4` | string8 | explicit |
| `PlayOffline` | `0x040e3d98` | bool | explicit |
| `PromptOnStartup` | `0x0440a514` | bool | explicit |
| `YTUserName` | `0x05664bf5` | string8 | explicit |
| `YTPassword` | `0x05664bf6` | string8 | explicit |
| `ShaderPath` | `0x1a34e253` | int | hash(ShaderPath) |
| `EffectsInstancing` | `0xdcd55da1` | bool | hash(effectsInstancing) |
| `MRT` | `0x00000029` | bool | explicit |
| `AlwaysFullscreen` | `0x05dd4647` | bool | explicit |
| `RenderTargetCorrection` | `0x00000044` | bool | explicit |
| `dropShadowQualityText` | `0xe30a842c` | int | hash(dropShadowQualityText) |
| `dropShadowQualityImage` | `0xa5bbb508` | int | hash(dropShadowQualityImage) |
| `NumFramesToBuffer` | `0x05c97448` | int | explicit |
| `terrainGenerateBrushesPerFrame` | `0xd40905a7` | int | hash(terrainGenerateBrushesPerFrame) |
| `terrainGenerateSingleStep` | `0x31989aaf` | bool | hash(terrainGenerateSingleStep) |
| `terrainGenerateTimeLimit` | `0xab52b057` | int | hash(terrainGenerateTimeLimit) |
| `MacSpecificText` | `0x061b67b6` | bool | explicit |
| `Support51Audio` | `0x063ab656` | bool | explicit |
| `HasShownExportToolEULA` | `0x087c4363` | bool | explicit |

## In the binary

The config-language commands are registered by the function at **`0x007c85e7`**. It allocates
a 16-byte command object per keyword and registers it through `[obj]->vtable+0x14` with
`(name, handler)`. Each command has a handler vtable; the first method is the executor:

| keyword | string | command vtable | executor |
|---|---|---|---|
| `setVariables` | 0x0141105c | 0x0141172c | `0x007c7f50` |
| `setOption` | 0x01411030 | 0x01411744 | `0x007c8e90` |
| `alert` | 0x01411028 | 0x0141175c | `0x007c8f00` |
| `vendor` / `card` | 0x01411008 / 0x01411010 | 0x014116f0 | `0x007c90f0` |

The keyword table is at `.data 0x153d1d8` (`cardVendor`, `cardName`, `isCardFound`,
`cardConfig`, `setVariables`, `boolProp`, `intProp`, `floatProp`, `keyProp`, `option`,
`setting`, `props`, `setOption`, `alert`, `setResolution`, `card`, `vendor`). So the loose
`Config/*.txt` scripts are run by a small command interpreter with exactly this vocabulary.

Our `source/engine/config.cpp` reads the same files but only records `property`/`set*`/
`setOption` assignments (and skips conditionals). A faithful port would implement the command
handlers above on top of our parser; that is future work.

### The `setOption` flow (why name->id matters)

The `setOption <Name> <Value>` executor (`0x007c8e90`) resolves the option NAME to an id
(`0x007c8c60`), reads the current value through `[this+0x4]->vtable+0xa0`, then applies the
new value by id (`0x007c7b60`). So options are addressed BY ID, and the name->id table is
`Properties.txt` - which is exactly why our reader keeps that table and never hashes the name.

