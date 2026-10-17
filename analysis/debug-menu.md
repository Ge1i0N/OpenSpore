# OpenSpore Debug Menu

The Debug build has a host state named `OpenSporeDebugMenu`, opened with **Ctrl+Shift+C**. `Esc`
returns to the state active before opening. This is an OpenSpore developer tool, not a claim that the
original EA Cheat Menu has been recovered.

## Reference findings

The supplied Beta190208 README explicitly documents:

```text
control-shift-C       bring up cheat console (type help for help)
control-shift-alt-C   bring up in-window cheat console
control-shift-X       show property editor
control-shift-W       toggle wireframe
control-shift-S       toggle stats window
control-shift-H       toggle hide UI
```

It also documents `state`, `-state`, `-devDirs`, `-userDataDir` and `-dataOverlayDir`. These are
Beta190208 facts, not automatic proof of identical GA retail dispatch/layout behavior. The selected
GA binary independently imports `GetKeyState`/`GetAsyncKeyState` and contains `Cheat`, `cheat`,
`console` and `Console` strings, so native keyboard/console infrastructure is real.

The community SDK defines `UTFWin::ConsoleWindow` proxy/type `0xEFBC56D8` and an imported symbol named
`UI::ConsoleWindow` at `0x0096B3E0`. GA's bounded byte search finds one visible proxy type write at
`0x00969A9F`, in a Cast-style method. Complete parsing of all **748 shipped GA SPUI layouts** finds
**zero serialized objects** with class `0xEFBC56D8`. The console window is therefore likely created
dynamically by native code, not loaded as an ordinary SPUI resource. The SDK symbol address is not
treated as a GA constructor proof without a version-matched call chain.

## Current host implementation

The keyboard path is Debug-only and ignores Windows auto-repeat (`WM_KEYDOWN` bit30). It enters a real
`AppStateManager` state, uses the existing D3D9 `Overlay`, and is not painted over a hidden state.
Actions are: return; open the controlled `GGEHostPreview`; next/previous state; reload; quit. Leaving
the menu clears modal bookkeeping. The GGE action remains controlled and cannot promote synthetic
catalogue/camera data into native readiness.

This is deliberately separate from a future native `ConsoleWindow` reconstruction. The next native
work item is the ConsoleWindow creation/registration/command-dispatch chain, followed by a decision
about a real SPUI/WinProc bridge. Until then, this host menu is the usable manual entry point and the
original hotkey README is an evidence lead, not a native cheat backend.

## Try it

```powershell
cmake --build build --config Debug --target OpenSpore
build/bin/Debug/OpenSpore.exe -w -devDirs
```

Press **Ctrl+Shift+C**, choose `Open GGEHostPreview (controlled)`, and press `Esc` to return. Existing
console commands (`state`, `ggepreview status`, `capture`, `quit`) remain available. No saves/profiles
are touched. Native Cheat Menu layout, exact GA hotkey handling, command registry and original
console text/input behavior remain unresolved.
