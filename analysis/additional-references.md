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

The newer bounded GA xref adds a concrete resource lead: ASCII `UI/DebugConsole` at `0x0141775C`,
referenced by Ghidra at `0x00802E3C`, `0x0080314D` and `0x0080316C` in two functions. The surrounding code calls the same UI/resource helpers
used for object setup and applies geometry work. This supports the user's recollection of a movable,
closable SPUI-like window, while the exact layout record and parent/message dispatch are still Unknown.

The bounded decompiler pass now identifies a constructor-like `0x00803110` and attach path
`0x00802DC0`, with `0x00951230`/`0x00F47240` resource helpers and a `0x0096B000` UI-object
initialization call. Vtable event roles remain unresolved; no claim is made yet for drag/close or
cheat command dispatch.

The next slot pass resolved a native state toggle: `0x008030E0` reads `0x00802C10`, inverts the
state through `0x00802B20`, and applies it through UI slots `+0x7C/+0x4C`. Object release flows through
`0x008029D0` and `0x00811BC0(1)`. This is strong lifecycle evidence, but the state enum and exact
drag/close/command meanings remain Unknown.

`FUN_00815EC0` then closes the creation loop: it allocates the `0x50`-byte DebugConsole object,
calls `0x00803110`, stores it at `+0x2D8`, attaches `UI/DebugConsole`, calls `0x00802B10(50000)`,
calls `0x00802B20(0)` to start hidden, and registers it with the UI manager. This is the strongest
current evidence that GA creates one registered hidden console during UI startup and later toggles it.

The Canvas listener source is now narrowed to service lookup ID `0x0114259C`: `FUN_007E62C0` stores
that service at outer `+0x14`, and `FUN_00A169B0` copies it to Canvas `+0x5C`. Its callback vtable
identity remains the final hotkey-specific Unknown.

The command side is separately corroborated by `0x0067ECC0`: `App/CheatManager` registers `history`
and `clear`, with the observed help text `Clears the debug console`, then `AppConsole` is registered
with `Console` mode/id bindings. The native command backend is real; only the exact hotkey branch and
full command registration remain unresolved.

The persistent vtable dump resolves `AppConsole +0x00` to the line dispatcher `0x0067EAA0`,
`AppConsole +0x04` to help `0x0067E240`, and `App/CheatManager +0x38` to execution callback
`0x0067EF20`. The remaining uncertainty is the keyboard-to-visibility event, not whether a native
command parser exists.

Event id `0x10001` reaches the ConsoleWindow output buffer through `0x0096A7B0` (wide append),
`0x0096A900` (ASCII append) and `0x0096AA80` (clear), dispatched by `0x00803310`.

The native WinProc `0x00849310` maps `WM_KEYDOWN/UP/CHAR` to `0x00848620/0x00848700/0x008487D0`
and sends modifier bits through `0x00847F40`; a listener at `window+0x5C` can consume the event
before default routing. `0x00849BF0` owns this `Canvas` class and stores the engine object via
`SetWindowLongW(-0x15, object)`. The identity of the listener assigned to `window+0x5C` that checks
`Ctrl+Shift+C` remains Unknown.

The listener service is the Graphics object from `0x0074FEE0/0x0074FE00`, vtable `0x0140D768`.
Its callback `0x00753D70` creates a queued event object with vtable `0x0140D7D8`; slot `+0x174`
(`0x00752060`) is currently lifecycle/queue setup, so the key comparison remains below that layer.

Canvas slots independently expose HWND activation/visibility (`0x00847B60/0x00847B80/0x00847BA0`),
minimized/zoomed tests (`0x00847BC0/0x00847BE0`), custom message posting (`0x00847FF0`) and
borderless show/resize (`0x008490C0`). These are platform transport semantics, not a claim that the
UTFWin DebugConsole itself is a native HWND.

## Next native probe

1. Resolve bounded callers of the newly seeded vtable lifecycle slots and `0x0096AC20`.
2. Identify native creation/parenting and message/WinProc registration.
3. Trace key-state callsites only far enough to identify the Ctrl+Shift+C branch.
4. Compare dynamic object slots with SDK names if a version-matched creation path appears.

No new Ghidra import is required for this first pass.
