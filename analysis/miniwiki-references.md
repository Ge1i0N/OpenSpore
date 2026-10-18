# `refs/MiniWiki` reference notes

Reviewed **2026-10-17**. `refs/MiniWiki` is a local historical/reference corpus. It is not the
selected GA-unpacked authority and its beta/wiki claims are not promoted to current GA behavior
without byte-level corroboration.

## Inventory

| Path | Use | Authority |
|---|---|---|
| `refs/MiniWiki/ConsoleCommands.md` | captured/wiki cheat-console UX and command examples | historical secondary reference |
| `refs/MiniWiki/OldBetaResearch/Cheats_1.txt` | old Beta cheat notes, including `Option OptionDebugUI` | historical lead only |
| `refs/MiniWiki/OldBetaResearch/Cheats_2.txt` | large old space/editor/level cheat list | historical lead only |
| `refs/MiniWiki/commandline.txt` | old command-line option notes (`-state`, `-devDirs`, `-w`, `-safe`, etc.) | historical lead only |
| `refs/MiniWiki/OldBetaResearch/Инфа.txt` | Russian research notes and provenance hypotheses | analyst notes, not binary evidence |
| `refs/MiniWiki/OldBetaResearch/_BETA_Search_Project.txt` | historical search terms for tech/debug releases | search-history only |
| `refs/MiniWiki/IncrementalHalton.cpp` | standalone math/source reference | unrelated to current Console claim |
| `refs/MiniWiki/rigblockList.txt` | old asset/list reference | unrelated to current Console claim |

## Console findings

`ConsoleCommands.md` describes the remembered interaction directly:

- **Ctrl+Shift+C** opens the hidden console;
- `help` lists commands;
- `clear` clears console output;
- `help <command>` returns command-specific help;
- `help -full` requests the full list;
- commands are described as case-insensitive.

These observations match the native GA evidence already recovered independently: GA has `AppConsole`,
`App/CheatManager`, `help`, `history`, `clear`, a line parser, output append/clear functions and
the `UI/DebugConsole` resource. MiniWiki is corroboration of UX, not proof of the exact GA build's
keyboard branch or every command's implementation.

`Cheats_1.txt` records an older `Option OptionDebugUI` command lead. No matching GA implementation
has been claimed from that text alone.

`Cheats_2.txt` lists historical commands such as `-debugUI`, `-picking`, `-speedUI`, `-oursystem`,
`-setRelationship`, `raid -start`, `tool enable all`, mission controls and the Level Cheats section.
It is useful for future native command-name searches, but it mixes Beta/debug builds and gameplay
services that OpenSpore deliberately does not fabricate.

## Command-line findings

`commandline.txt` independently lists historical options including:

```text
-state:
-locale:
-devDirs / -shipDirs
-dataDir / -dataBaseDir
-userDataDirs
-w / -F / -r:<width>x<height>
-safe
-multipleInstances
```

Several of these are already corroborated by the GA binary or current OpenSpore command-line model;
the file itself remains only a lead. Unknown entries (`-demo`, `-noDev`, `-preload`, `-dumpTelemetry`,
etc.) stay Unknown.

## Boundary

The native GA console chain is stronger than the MiniWiki material: its resource, constructors,
startup registration, Canvas input transport, AppConsole/CheatManager dispatch and output buffer are
all tied to the selected GA SHA-256 in [debug-console audit](audit/debug-console.json). MiniWiki
does not establish the unresolved listener callback that checks `Ctrl+Shift+C`, and it does not
establish native readiness for `levels -unlock` in OpenSpore.
