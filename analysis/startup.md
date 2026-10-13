# The original's startup chain (how SporeApp.exe actually boots)

Authority: disassembly of `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`. This is the "start from
main" path, traced from the PE entry point. Addresses are VAs in the oracle's layout.

```
PE entry  0x011e11e1   (Subsystem=2, GUI)
  call 0x011e1588      ; _security_init_cookie (CRT init)
  jmp  0x011e0f21      ; __tmainCRTStartup

__tmainCRTStartup  0x011e0f21
  call 0x13cc29c       ; GetStartupInfoA  (IAT slot, ref 0x11e0f39)
  0x11e1021..0x11e1099 ; the argv walk: cmp al,0x20 / cmp al,0x22 (space and quote handling)
  push 0x400000        ; hInstance  (the exe's HMODULE == image base)
  push 0              ; hPrevInstance
  push esi            ; lpCmdLine
  push eax            ; nShowCmd
  call 0x00f489e0      ; WinMain            <-- 0x11e105c

WinMain  0x00f489e0
  call 0x0092c420      ; ctor(nShowCmd)
  call 0x00f48810      ; app_main            <-- the real main
  call 0x005d74e0      ; dtor
  ret 0x10

app_main  0x00f48810
  arg "multipleInstances" 0x0148da14 -> 0x0092b570 (arg lookup)
    if absent: create the named object "Global\SporeEditorOrGame" (0x0148d9f8);
      if it already exists (0x922440 == 0xfffffffe) ->
        "Another instance of the game is already running" (0x0148d9c8) and return -1
  "Spore" 0x0140a5d8 ; argv again 0x0092b420 ; path ops 0x00931080 / 0x00930f90 (0x104 = MAX_PATH)
  alloc 0x48 bytes, tag "App" 0x013ebc48 -> 0x00f47240 (the allocator)
  set vtables (0x0148d79c, 0x0148d78c, 0x013ec438) and zero the fields
  call 0x00f47e90      ; run                 <-- the run method

run  0x00f47e90
  call 0x006b4eb0      ; early init check (false -> call 0xc2e640 and bail)
  call 0x0081d530      ; register 0xc2e640
  arg "demo" 0x01413db0 -> 0x0092b570 (bool)
  call 0x008055e0
  alloc 0x148 (328) bytes, tag "App" -> 0x00920960 -> [this+0x24]  ; a large App object
  0x0067cb50, 0x00692ea0, 0x006879c0(table 0x0148d760, count 5)
  arg "devDirs" 0x013f8dc8 / "shipDirs" 0x013f8db4 -> 0x00687bd0   ; FILESYSTEM MODE (dev vs ship)
  call 0x007e8a00      ; CreateAppMode (alloc 0x1f0 = 496 bytes, tag "App", ctor 0x007e76e0)
                       ;   -> [this+0x20]  (the app-mode / state-manager host)
  0x00579a90 x2, 0x006abab0 x2   ; construct + register
  ... subsystem setup, then the main loop (vtable dispatches through [this+0x20])
  arg "baseGame" 0x014081a8 -> 0x0092b570  (content selection)
  ...
```

## What this pins down

- **WinMain is `0x00f489e0`**, a thin wrapper; the real main is **`0x00f48810`**, and the run method
  is **`0x00f47e90`**. That is the answer to "where does the game start".
- **Single-instance guard**: `app_main` checks `-multipleInstances`; without it, a named object
  `Global\SporeEditorOrGame` enforces one instance, with the message
  `Another instance of the game is already running`. (Matches the `multipleInstances` arg in
  `analysis/arg_vocab.md`.)
- **The filesystem mode is chosen in `run`**, from `-devDirs` / `-shipDirs`, via `0x00687bd0`. This
  is the same contract we reversed at `0x007e7e00..0x007e8420` (VERIFY row 2).
- **`-demo` is read early in `run`** (`0x01413db0`), and **`-baseGame`** later (`0x014081a8`).
- **The app-mode / state-manager host is created by `0x007e8a00`** (496-byte object, ctor
  `0x007e76e0`) and stored at `[app+0x20]`; the whole run method then dispatches through its vtable.
  This is the object whose state machinery we reversed (`AppStateManager`, the `state` verb).

## run `0x00f47e90` — full extent and what it does

`run` spans **`0x00f47e90`..`0x00f4880a`** (`ret 0x4`, returns 1). It does NOT itself contain the
message loop; it initialises subsystems and dispatches through the AppMode object's vtable.

- **args it reads** (all through `0x92b570`): `demo` 0x01413db0, `devDirs` 0x013f8dc8,
  `shipDirs` 0x013f8db4, `baseGame` 0x014081a8, `automation` 0x0148d898, `mouse` 0x0148d88c,
  `hostIP` 0x0148d870, `clientIP` 0x0148d85c, `isHost` 0x0148d84c.
- Allocates several objects through `0xf47240` (tag `App`): 0x148-byte (0x920960), 328-byte,
  0x48-byte `Editor` (0x5d68a0), 0x48-byte (0x6249e0), 0x124-byte (0x7da070), 0x2e0-byte (0x8142a0).
- **Creates the AppMode / config object** via `0x007e8a00` (496 bytes, ctor `0x007e76e0`) and
  stores it at `[app+0x20]`.
- **Dispatches through the AppMode vtable `0x01413b50`**:
  | slot | method | what it is |
  |---|---|---|
  | +0x14 | `0x007e6850` | **subsystem init** (allocs 0x40/0x70-byte objects, `0x68db60`, `0x696400`, `0x67e040`, `0x68c0c0`, `0x6895b0`, `0x7ec170`) |
  | +0x18 | `0x007e93d0` | plugin/script setup: if neither `-noPlugins` (0x1413e28) nor `-safe` (0x1411b8c) -> `0x7e8810` |
  | +0x1c | `0x007e7430` | **create the graphics device**: tries 640x480 (0x280/0x1e0); on failure `Could not create the Graphics Device.` 0x1413ac8 |
  | +0x20 | `0x007eaae0` | shader/script config: `-dumpShaders` 0x14142a8, `-dumpShadersForPIX` 0x1414284 / `-pix` 0x141427c, `-dumpFragmentShaders` 0x1414254, `-dumpDirectShaders` 0x1414230, `-noScripts` 0x141421c, `-noMaterialScripts` 0x14141f8, `-noEffectScripts` 0x14141d8 |
- Other manager getters used: `0x67dc90/0x67dcb0/0x67dcd0/0x67dce0/0x67dcf0/0x67dda0`, `0x6a17e0`
  (a profiler/marker call), `0x951220`/`0x951230` (allocator pair), `0x5a7560`, `0x456230`.
- **`run` loads the app's state list** at `0xf48505`: `[app+0x34]->vtable+0x1c` is called with the
  group id **`0xd6396913` = `SporeAppStates`** (resolved via FNV-1-lower over `reg_file.txt`). So the
  state machine's states come from a `trigger` resource of group `SporeAppStates` (see
  `analysis/content-loading.md`).

## The main loop — FOUND: `0x008482f0` (window-class vtable `+0x84`)

It is a method of the **window class** (vtable `.rdata 0x141cc10`, slot `+0x84` = `0x141cc94` ->
`0x8482f0`), and `run` reaches it at **`0xf4840b`** (`call eax`, where
`eax = [[0x67dcb0()]->vtable + 0x84]`; `0x67dcb0` returns the window object from global `0x15fd89c`).

**CORRECTED by the Ghidra audit (session 13).** This doc previously said the call site was
`0xf48405`. It is not — that address only LOADS the slot, and the call is the very next instruction:

```
f48401  mov edx,[eax]         ; the window object's vtable
f48403  mov ecx,eax           ; this = the window object
f48405  mov eax,[edx+0x84]    ; LOAD slot +0x84        <-- what we used to cite
f4840b  call eax              ; THE CALL SITE
```

This is not pedantry: it is the shape of every virtual dispatch in this binary, and it is *why* the
main loop was hard to find. A text search for `call [reg+0x84]` matches nothing here, because the
loader split it into a `mov` and an indirect `call`. Pin the difference with the two `insn_at` checks
in `analysis/claims.json` (`main.loop.caller`).

The loop body (verified instruction by instruction):

```
8482f0  push ebp; mov ebp,esp; and esp,-8; sub esp,0x30; push ebx/ebp/esi/edi
8482fc  mov esi, ecx                 ; this = window
848300  test [esi+8], 0x8 ; if set, focus/activate handling (0xc8 ms sleep, SetForegroundWindow 0x13cc678)
84833e  test [esi+8], 0x20 ; if clear -> return 1 (loop disabled)
848348  mov edi, ds:0x13cc62c        ; edi = PeekMessageW
84834e  PeekMessageW(&msg, 0, 0, 0, PM_REMOVE=1)   ; call edi
84835a  test eax,eax ; je 0x84840a (no message -> return 1)
LOOP:
848362  cmp [msg.message], 0x12      ; WM_QUIT
848367  je 0x848414                  ; -> return 0
        ...build an app event {id 0x1ee100b / 0x1ee1001, msg} and dispatch it to the handlers at
        [esi+0x5c]/[esi+0x60]/[esi+0x64] via their vtables (0x30 / 0x14 / direct)...
8483e5  call TranslateMessage(&msg)  ; IAT 0x13cc5cc
8483f0  call DispatchMessageW(&msg)  ; IAT 0x13cc630
8483f6  PeekMessageW(&msg, 0, 0, 0, PM_REMOVE=1)   ; call edi
848402  test eax,eax ; jne LOOP      ; more messages -> keep draining
84840a  return 1
848414  return 0                     ; (WM_QUIT path)
```

So the engine's frame/event pump is: `PeekMessageW` -> (WM_QUIT check) -> dispatch to registered
window-event handlers -> `TranslateMessage` -> `DispatchMessageW`, repeated until the queue is
empty, then return 1 (the caller loops). `PM_REMOVE = 1`.

Note there are **two** PeekMessage users: this main loop uses **`PeekMessageW`** (via `edi`), while
the mouse-coalescing helper `0x84a050` uses **`PeekMessageA`** directly (IAT 0x13cc5f0). That is why
a search for direct callers of the pump missed the loop: the loop calls `PeekMessageW` through a
register, and `0x84a050` is a separate mouse helper reached via vtable slot `+0x88`.

## The window/input class

- **vtable `.rdata 0x141cc10`**; **ctor `0x849b20`**, **Create `0x849bf0`** (RegisterClass/CreateWindowEx,
  HWND stored at `[obj+0x70]`), **dtor `0x849e80`**.
- **CORRECTED by the Ghidra audit**: vtable **slot 0 is `0x849e80`, the deleting destructor** — not the
  ctor. The ctor `0x849b20` is real (the audit PASSED the check that it is called from `0x7e6f21`) but
  it is **not a vtable slot**, so trying to prove it by reading slot 0 was a category error. Slots
  `+0x84` (`0x8482f0`, the main loop) and `+0x88` (`0x84a050`, the mouse pump) both verified as claimed.
- **`0x849bf0` (Window::Create) is one of the seven addresses Ghidra's auto-analysis never carved** —
  it is reachable only through the vtable. It is now seeded (`analysis/audit/seeds.tsv`).
- **`+0x84` `0x8482f0` = the main loop**; **`+0x88` `0x84a050` = the mouse/PeekMessageA pump**.
- wraps mouse tracking (`TrackMouseEvent` 0x141cbfc, `user32.dll` 0x141cbf0).
- The window object is created in the AppMode setup function at **`0x7e6f21`** (`call 0x849b20`),
  stored at **`[AppMode+0x10]`** and registered in global **`0x15fd89c`** (getter `0x67dcb0`); that
  same function calls `Window::Create` (`[win]->+0x04`), sets window flags `0x2a`/`0x6a` (`-flock`
  0x1413a2c ORs `0x80`), inits the renderer, reads `-vSync` (0x14139d4) / `-noVSync` (0x14139c4), and
  on renderer failure shows `Could not start the renderer.` (0x14139e0).
- **`0x7e7160`** is the app **event dispatcher** (switches on hashed event ids 0x255abf5 / 0xf62add /
  0x212d3e7 / 0xae1cfe73); the 0xf62add branch has a retry loop (10x, `Sleep(0xfa)`).

## AppMode state / event names (from `.data 0x153f858..0x153f868`)

The AppMode object drives a state machine; **`.data 0x153f858..` is a table of POINTERS into
`.rdata`**, not a table of strings. (CORRECTED by the Ghidra audit, session 13: this doc previously
presented the `.data` addresses as if the names lived there, and an audit check that read them as
strings failed. The names themselves were right.)

| `.data` slot | points at | string there | note |
|---|---|---|---|
| `0x153f858` | `0x1413870` | `f` | (single char) |
| `0x153f85c` | `0x1413868` | `quit` | an AppMode event/state |
| `0x153f860` | `0x1413864` | `app` | an AppMode event/state |
| `0x153f864` | `0x1413828` | `Copyright (c) 2006, Electronic Arts. All` | banner |
| `0x153f868` | `0x1413814` | **`AppModePluginLoader`** | the **initial** AppMode state |

Two names this doc previously attributed to `.data` slots were at the wrong address:

* **`UIWorld_Loading`** is stored **UTF-16LE** at `.rdata 0x01418274` (bytes `55 00 49 00 57 00 ...`).
  A 1-byte ASCII read of it yields only `U`, which is exactly how the audit caught this. The pointer
  to it is at `[0x1544cec]`. There is **no** ASCII `UIWorld_Loading` anywhere in the binary.
* **`EventLoadScreenBufferPercentage`** is at `.rdata 0x0148227c`, and the only `.text` reference to
  it is `0x01373538`. The address previously cited here (`0x1550498`) is a pointer to
  **`EAReportUploader`** (`0x01446c18`) — a different subsystem entirely. `analysis/claims.json`
  keeps this as an explicit NEGATIVE control so the mix-up cannot silently return.

So the app starts in **`AppModePluginLoader`** and drives a state machine whose other names are
`app`, `quit`, `UIWorld_Loading`. This is the AppStateManager family we reversed (VERIFY rows 3-6).

**And the state SET is data we DO have** — session 13 initially recorded it as missing, which was
wrong in two ways at once (the `Locale/` search-path class of error, and probing only the `group`
field of a DBPF record that has three id fields). The full recovered machine is in
**`analysis/states.md`**: `SporeAppStates` is a `trigger` resource with **instance** id
`0xD6396913`, 8,499 decoded bytes of plain ArgScript, giving **60 states and 17 modes**, entry state
`Main -id 0` ("Spore Launch Screen") -> `GalaxyGameEntry`.

## Still to trace (next steps, in order)

1. **AppMode field map** — the ctor `0x007e76e0` zeroes fields and builds sub-objects; map them
   (it already ties `[+0x159]` = useDevDirs to VERIFY row 2).
2. **Config / state text**: the game's `trigger` resources are text (we decode them already); find
   where the run path reads them (likely through the AppMode subsystem) and where SPUI is spawned.
3. The named-object guard uses `Global\`; the beta PDB names (SP::/EA::) can label these once we
   match by identity (see `analysis/name_guesses.md`).

## Note on the beta PDB

The PDB is a different link, so it cannot supply these addresses. It can still NAME them once
identity is matched; `analysis/name_guesses.md` is the (partial) guesser for that.
