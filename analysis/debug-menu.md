# OpenSpore Debug Cheat Console

The Debug build has a host state named `OpenSporeDebugCheatConsole`, opened with **Ctrl+Shift+C**.
It is an actual command-entry window: type a command, press **Enter**, and use **Up/Down** for
history. `Esc` returns to the state active before opening. This is an OpenSpore developer tool, not
a claim that the original EA Cheat Menu has been recovered.

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

The stronger resource lead is the exact ASCII string `UI/DebugConsole` at GA VA `0x0141775C`.
The persistent Ghidra xref probe finds three DATA references at `0x00802E3C`, `0x0080314D` and
`0x0080316C`, inside `FUN_00802DC0` and `FUN_00803110`. The first site calls `0x00951230`, tests
the returned object, then calls `0x0096B000`, while the surrounding routine performs rect/position-
style work. This is consistent with a dynamically loaded or created movable UTFWin window and with
the recollection of a closable SPUI-like widget. The exact package record, parent root, close/move
handlers and hotkey caller remain Unknown.

### Bounded decompiler result

The persistent-project probe now separates the path into useful pieces:

- `FUN_00803110` is a constructor-like initializer. It creates two `UI/DebugConsole`-keyed resource
  wrappers through `FUN_00F47240`, with the observed size/type values `64000` (`0xFA00`) and `4`.
- `FUN_00802DC0` is the load/attach path. It calls `FUN_00951230(0x870, 4, "UI/DebugConsole", ...)`,
  passes the returned object to `FUN_0096B000(..., 1)`, stores it at object offset `+0x28`, binds the
  related UI object at `+0x2C`, and performs geometry/service lookups.
- `FUN_00951230` is an allocator/object-buffer helper, while `FUN_00F47240` wraps a lower resource-key
  helper. `FUN_00802C70` is a per-frame/update-side path using timing/color-like values; it is not
  sufficient evidence for opening or closing the console.
- The vtable has overrides at `0x008030A0`, `0x00802A00`, `0x008030C0`, `0x008030E0` and
  `0x008032F0`. Several are not carved as standalone Ghidra functions, so their exact event roles
  remain Unknown; the next probe should seed/read those short bodies rather than launch a broad import.

The result strongly supports a native-created SPUI/UTFWin DebugConsole object. It still does not prove
which slot handles drag, close, visibility or command dispatch.

### Lifecycle slot result

The focused vtable seeds now resolve four previously uncarved entries:

- `0x008030E0(1)` reads `FUN_00802C10()` and calls `FUN_00802B20(!state)`;
- `0x00802C10` reads a one-bit state from the `UI/DebugConsole`-associated object;
- `0x00802B20` applies that state through UI vtable slot `+0x7C` and updates the related object via
  `+0x48/+0x4C`;
- `0x008029D0` releases the owned object and calls `FUN_00811BC0(1)`, which releases another owned
  UI/service object and clears its pointer;
- `0x00802B10` forwards to `FUN_0096AC20`, which updates a window-object field at `+0x220` and calls
  `FUN_00967610`. This is a likely show/focus/state path, but the exact semantic enum is still
  Unknown.

Therefore the remembered ability to toggle/close a movable window is now supported by a native
object lifecycle and state-toggle path. Drag geometry, close control and command dispatch still need
their event/message callers.

### Startup and hidden initial state

`FUN_00815EC0` is the global UI startup path. It creates the shared `UI/WindowManager`, allocates a
`0x50`-byte object, invokes `FUN_00803110`, stores the object at `+0x2D8`, and then performs:

```text
FUN_00802DC0()       // attach UI/DebugConsole
FUN_00802B10(50000)  // window-object state/update argument
FUN_00802B20(0)      // initial hidden/disabled state
manager +0x30(obj)   // register with the UI manager
```

This proves the native console is created during UI initialization and starts hidden. The hotkey or
event path therefore likely changes an already registered object, rather than constructing a new
window on every `Ctrl+Shift+C`. The exact meaning of `50000`, the state enum and the keyboard branch
remain Unknown.

### Native command backend

The command-side initialization is identified at `FUN_0067ECC0`. It creates `App/CheatManager`,
invokes `FUN_0067E440`, and registers the native command objects `history` and `clear`; the latter
has the GA help text **“Clears the debug console”**. It then creates `AppConsole` and registers:

```text
AppConsole  mode=0  id=100
AppConsole / Console  mode=1
```

GA also contains the CheatManager help string:

```text
[<pattern:string>] [-full] Lists help for all cheats, or those that match <pattern>
```

This confirms the native command backend and its connection to the Console UI. The exact keyboard
trigger and complete command table remain unresolved; the OpenSpore host console does not claim to
implement those native cheat services.

Additional bounded GA command leads are `history` with `[<items:int>] Lists the last n commands in the
command history` and `levels -unlockAdventures` with `Unlocks all Maxis adventures`. They are retained
as native leads only; OpenSpore does not report them as implemented cheat services.

The native `Level Cheats` object is bounded at constructor `0x00DDDEE0`, vtable `0x0147D36C` and
handler `0x00DDDF40`. Its exact option table is:

```text
-unlock             value 0  Unlocks all game levels
-unlockAdventures   value 1  Unlocks all Maxis adventures
```

The handler routes into native level state/services through `FUN_0083BAA0`; OpenSpore keeps its host
`levels -unlock` response explicit because that backend is not connected.

### SDK ↔ GA bridge leads

The pinned ModAPI SDK provides a useful method-order map: `IWinProc::HandleUIMessage` is vtable
`+0x18`, `UTFWin::Message` stores event type at `+0x08`, virtual key at `+0x10` and modifiers at
`+0x14`, while `IWindow::SetFlag` is `+0x7C`. The same SDK address table names GA routines
`GameInput_KeyDown 0x00697CA0`, `MainWindow_OnKey 0x008153C0`, `CheatManager_ProcessLine 0x0067F150`
and the window/message-manager routines. These are now focused seed targets; names remain secondary
until their GA bodies are corroborated.

A bounded immediate scan found several higher App/UI callsites carrying event id `0x1EE1001`, but
some reported offsets are inside uncarved bodies or dispatch data rather than function entries. They
are retained as callsite leads, not falsely promoted to standalone Ghidra functions.

The vtable mapping is now explicit: `AppConsole` vtable `0x0140195C + 0x00` points to
`FUN_0067EAA0` (line parser/dispatcher), while `+0x04` points to `FUN_0067E240` (help description).
The CheatManager vtable `0x01401B40 + 0x38` points to `FUN_0067EF20` (command execution/result
callback). This is native command-dispatch evidence, not merely a string match.

The output side is also resolved: `FUN_00803310` handles event id `0x10001` and routes to
`FUN_0096A7B0` for wide-string append, `FUN_0096A900` for ASCII append, and `FUN_0096AA80` for
clearing the console buffer. Thus the native chain is now demonstrated through command parsing,
CheatManager execution and text-buffer output. Only the exact hotkey-to-visibility event remains
unresolved.

### Win32 input boundary

The exact native window procedure is `FUN_00849310`. Its message routing is bounded and explicit:

```text
WM_KEYDOWN (0x100) → FUN_00848620
WM_KEYUP   (0x101) → FUN_00848700
WM_CHAR    (0x102) → FUN_008487D0
```

The key event carries `wParam`, a modifier mask (`Shift=1`, `Ctrl=2`, `Alt=4`, `Win=16`) and an
event subtype into `FUN_00847F40`. Before the normal message switch, the window procedure offers the
event to a registered listener at `window+0x5C`; that listener may consume it. This is the remaining
bounded location to identify for the exact `Ctrl+Shift+C` branch.

The owner path is also bounded: `FUN_00849BF0` registers the Win32 class **`Canvas`**, assigns
`FUN_00849310` as its `lpfnWndProc`, creates the HWND, and stores the engine object through
`SetWindowLongW(-0x15, object)`. `FUN_00849310` reads the listener at `window+0x5C` and offers it
event `0x1EE100C` before translating `WM_KEYDOWN` into `0x1EE1001`. The final hotkey probe is
therefore limited to the object assigned to that one listener field.

The listener source is now narrowed further: `FUN_007E62C0` fills outer `+0x14` through global
service lookup ID `0x0114259C`; `FUN_007E6850` passes that field to Canvas vtable `+0x68`, and
`FUN_00A169B0` stores it at `Canvas+0x5C`. The remaining task is to resolve that service object's
vtable/callback identity.

The service is now identified as the Graphics object created by `FUN_0074FEE0`/`FUN_0074FE00`, with
vtable `0x0140D768`. Its event callback is `FUN_00753D70`, which creates a queued Graphics event
object through `FUN_00753960` (vtable `0x0140D7D8`). The event object's slot `+0x174` is
`FUN_00752060`, currently classified as lifecycle/queue setup; the exact key comparison remains
below this event-object layer.

The secondary event-object vtable segment is `0x0140D938`: `+0x10` is `FUN_00753CB0` (queue/list
callback management), `+0x14` is `FUN_00752060` (initialization), and `+0x18` is `FUN_00752150`
(cleanup). This confirms that the Graphics callback is an event-object registry/queue layer rather
than the final hotkey comparison.

The Canvas vtable confirms the platform lifecycle separately: `0x00847B60` activates the HWND,
`0x00847B80` tests visibility, `0x00847BA0` calls `ShowWindow`, `0x00847BC0/0x00847BE0` test
minimized/zoomed state, `0x00847FF0` posts the custom `0x501` message, and `0x008490C0` applies
borderless show/resize. These are the host-window primitives; they should not be confused with the
higher-level `UI/DebugConsole` UTFWin object or its CheatManager state.

## Current host implementation

The keyboard path is Debug-only and ignores Windows auto-repeat (`WM_KEYDOWN` bit30). It enters a real
`AppStateManager` state, uses the existing D3D9 `Overlay`, receives `WM_CHAR`, and submits through the
same OpenSpore command registry as `-exec` and stdin. `help`, `state`, `cfg`, `txt`, `spui`, `pkg`,
`capture`, `quit` and other registered commands therefore work in the window. `levels -unlock` is
registered as an explicit host boundary and reports that the native level service is not connected;
it does not fabricate unlocks or modify saves.

This is deliberately separate from a future native `ConsoleWindow` reconstruction. The original
in-window console is still not claimed: its dynamic creation/registration/parenting chain, exact GA
keyboard branch and original command registry remain unresolved. The current window is the usable
GA-shaped host path, while the binary findings below remain evidence boundaries.

The hidden dev-only `_debugmenu` and `_debugconsole` commands toggle the same host state for unattended launch scripts;
the keyboard path is not required for automation. The switch is queued through the normal state
manager chain, so its command response reports the requested transition, not synchronous visibility.

## Try it

```powershell
cmake --build build --config Debug --target OpenSpore
build/bin/Debug/OpenSpore.exe -w -devDirs
```

Press **Ctrl+Shift+C**, or use `Run-DebugMenu.cmd` (which invokes `_debugmenu` through `-exec`). Type
`help`, `state -list`, `state DevLauncher`, `levels -unlock` or `quit`. No saves/profiles are touched.

Each run's normal and crash logs are retained under the executable's `logs/` directory with timestamp/PID names; see
[local launch modes](launch-modes.md) for the naming and fallback policy.
