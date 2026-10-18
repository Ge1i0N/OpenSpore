# Local launch modes

These launchers are generated beside the current Debug executable in `build/bin/Debug`. They use
`OpenSpore.exe`, not the original `SporeApp.exe.unpacked.exe` reference binary. All commands below
are Debug/dev workflows; they do not claim native GGE readiness.

Each run writes unique normal and crash logs under the executable's directory:
`build/bin/Debug/logs/OpenSpore-YYYYMMDD-HHMMSS-mmm-pid<id>.log` and the matching `.crash.log`.
This is independent of the process working directory, so PowerShell launches and batch files use the
same folder. Existing logs are never overwritten. If that directory cannot be created, the app falls
back to legacy `OpenSpore.log` and `OpenSpore.crash.log` in the working directory.

## Batch files

Run from Explorer or a terminal:

| File | Command / result |
|---|---|
| `build/bin/Debug/Run-Loader.cmd` | `OpenSpore.exe -w -devDirs`; normal test loader path |
| `build/bin/Debug/Run-DevLauncher.cmd` | `OpenSpore.exe -w -devDirs -state:DevLauncher`; directly opens the real DevLauncher state |
| `build/bin/Debug/Run-GGEHostPreview.cmd` | `OpenSpore.exe -w -devDirs -state:GGEHostPreview`; controlled 3-card GGE host preview |
| `build/bin/Debug/Run-DebugMenu.cmd` | starts DevLauncher and opens the OpenSpore Debug Cheat Console automatically; `Ctrl+Shift+C` also toggles it |

The existing `_RelGaStart.cmd` is deliberately different: it launches the original unpacked GA
authority, not this reimplementation. It remains available for reference comparisons.

## Direct commands

```powershell
build/bin/Debug/OpenSpore.exe -w -devDirs
build/bin/Debug/OpenSpore.exe -w -devDirs -state:DevLauncher
build/bin/Debug/OpenSpore.exe -w -devDirs -state:GGEHostPreview
```

For a repeatable controlled click smoke without manual input:

```powershell
build/bin/Debug/OpenSpore.exe -w -devDirs -state:GGEHostPreview `
  -exec spui:-click,640,360 -execAfter 10 -quitAfter 30
```

Expected log evidence includes `picked host card 20481 (host only)`. The preview uses original shipped
SPUI/atlas bytes, but catalogue entries and camera observations are controlled host inputs.

## Debug Cheat Console

In a Debug build, run `Run-DebugMenu.cmd` to open automatically, or press **Ctrl+Shift+C** once the
window is active. Type a command and press **Enter**; **Up/Down** recalls history and **Esc** returns.
Try `help`, `state -list`, `state -next`, `state DevLauncher`, `levels -unlock` or `quit`.
This is a host command window, not the recovered native EA console. The original Beta README
documents the same shortcut and the separate Ctrl+Shift+Alt+C in-window shortcut, but GA's native
creation/dispatch chain is still unresolved; see [debug-menu](debug-menu.md) and
[additional references](additional-references.md).

## State notes

`DevLauncher` is a shipped/script-declared state whose primary `GlobalUIGGE-9` layout is now rendered
by the existing UI path. Its launcher transition remains a real state/UI boundary; unsupported
command/message backends stay explicit.

`GGEHostPreview` is registered only in Debug/dev builds. It is the best current manual visual demo of
the reconstructed card layer, but it is not the original automatic GalaxyGameEntry catalogue.

The normal loader command is still useful for checking the boot chain, package loading, locale and
state selection before entering any controlled preview. No launcher writes saves or scans unrelated
profiles.

## Rebuild

From the repository root:

```powershell
cmake --build build --config Debug --target OpenSpore
```

The batch files live in the build output directory and may be regenerated/removed by a clean build;
the command list in this document is the source-of-truth for reproducing them.
