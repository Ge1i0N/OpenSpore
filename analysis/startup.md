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

## The message pump

The game uses **`PeekMessage`**, not `GetMessage` (only `PeekMessageA`/`PeekMessageW` are imported;
IAT `0x13cc5f0`/`0x13cc62c`). The only caller of `PeekMessageA` is **`0x0084a050`**, a message pump
that drains the queue (up to 0x400 messages), then does **mouse coalescing** (it merges consecutive
`WM_MOUSEMOVE` 0x200 events and rewrites duplicate `WM_LBUTTONDBLCLK` 0x203 -> `WM_LBUTTONDOWN`
0x201). `0x84a050` is referenced only as a vtable slot at `.rdata 0x0141cc98` (an input/window
handler object), not called directly — so the main loop invokes it through that vtable.

## Still to trace (next steps, in order)

1. **The main loop itself** — it is NOT in `run`; it is reached through a vtable and calls the pump
   `0x84a050`. Find the object whose vtable contains `0x141cc98`, and its loop. This is the last
   piece of "how the game runs".
2. **AppMode field map** — the ctor `0x007e76e0` zeroes fields and builds sub-objects; map them
   (it already ties `[+0x159]` = useDevDirs to VERIFY row 2).
3. **Config / state text**: the game's `trigger` resources are text (we decode them already); find
   where the run path reads them (likely through the AppMode subsystem) and where SPUI is spawned.
4. The named-object guard uses `Global\`; the beta PDB names (SP::/EA::) can label these once we
   match by identity (see `analysis/name_guesses.md`).

## Note on the beta PDB

The PDB is a different link, so it cannot supply these addresses. It can still NAME them once
identity is matched; `analysis/name_guesses.md` is the (partial) guesser for that.
