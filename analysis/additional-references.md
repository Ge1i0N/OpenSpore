# Additional reference corpus: `refs/_Also`

Reviewed **2026-10-17**. The user-supplied `refs/_Also` tree is an additional local reference corpus,
not an authority replacement for the selected GA-unpacked executable. Its beta documentation, package
sets and SporeMaster-derived sources are useful for names, historical workflows and comparison;
every binary/format claim still needs GA-version corroboration.

## Important contents

| Path | Use |
|---|---|
| `refs/_Also/SporeGames/SporeBeta190208/readme.txt` | Beta controls, developer cheats, console shortcuts, command-line options and properties |
| `refs/_Also/SporeUnpackedData` | Historical unpacked Data/DDFList/config/property/format corpus |
| `refs/_Also/_PackageFilesBetaVer` | Beta package comparison |
| `refs/_Also/_PackageFilesFinalVer` | Additional final/expansion package comparison |
| `refs/_Also/_AlsoOnlyIfNeed/SporeMaster` | Extracted SporeMaster source/format helpers |
| `refs/_Also/CheckAll`, `EAPackageBrowser-main`, `SporeEdit` | Community package/resource tooling references |

The tree is already under ignored `refs/`; no source or binary from it is copied into the runtime.
No supplied executable was launched and no user/save data was modified.

## Beta README findings relevant to debug UI

`SporeBeta190208/readme.txt` explicitly records:

```text
Ctrl-H                    Toggle UI
Ctrl-Alt-Home             Back out from game/editor to Galaxy Game Entry
control-shift-C           Bring up cheat console
control-shift-alt-C       Bring up in-window cheat console
control-shift-X           Show property editor
control-shift-W           Toggle wireframe
control-shift-S           Toggle stats window
control-shift-H           Toggle hide UI
```

It also documents `state`, `-state`, `-devDirs`, `-userDataDir`, `-dataOverlayDir`, `-state:<name>`
and `-mode:<name>`. These are **Beta190208 documentation**, not proof that every shortcut or command
has identical GA retail behavior. The README gives a strong historical lead for an in-window console,
but does not replace GA binary evidence.

## GA/SDK cross-check

The selected GA binary imports both `GetKeyState` and `GetAsyncKeyState`, and its string pool includes
`Cheat`, `cheat`, `console` and `Console`. Bounded callsite disassembly was retained around keyboard
helpers at `0x8D3140`, `0x8D35E0`, `0x848080`, `0x848100` and `0x7E6010`.

The pinned community references define `UTFWin::ConsoleWindow`, proxy/type `0xEFBC56D8`, and an
imported SDK symbol `_ZN2UI13ConsoleWindowC2Ev` at `0x0096B3E0`. Complete parsing of **748 shipped
GA SPUI layouts** found **zero serialized objects** with class `0xEFBC56D8`; GA has one visible type
write at `0x00969A9F` in a Cast-style method. The bounded conclusion is:

> The original in-window console is likely dynamically created/native-managed rather than an ordinary
> shipped SPUI layout. Its exact GA constructor/registration/hotkey chain remains unresolved.

The SDK symbol address is not treated as a GA constructor proof. The current `Ctrl+Shift+C` state is
explicitly host-side and controlled.

## Next native probe

1. Resolve bounded xrefs around GA ConsoleWindow type/cast and the SDK-suggested address area.
2. Identify native creation/parenting and message/WinProc registration.
3. Trace key-state callsites only far enough to identify the Ctrl+Shift+C branch.
4. Compare dynamic object slots with SDK names if a version-matched creation path appears.

No new Ghidra import is required for this first pass.
