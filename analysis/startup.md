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

## Still to trace (next steps, in order)

1. **`0x007e76e0`** — the AppMode constructor: it sets the object's vtables
   (`0x1413b50`, `0x1413b40`, `0x1413b3c`, `0x13eb384`, `0x13eb8b8`), zeroes its fields, sets a few
   default floats (1.0 from `0x1453740`, 0.9 from `0x13ec490`, 0.4 from `0x13ec414`), constructs
   sub-objects at `+0x88` (`0x7e7310`) and `+0xd0` (`0x936860`), and builds two string-keyed
   members at `+0x138`/`+0x148` via `0x579a90`. **It zeroes `[+0x159]` — the same `useDevDirs` byte
   the `-devDirs` handler sets** (`0x007e7f27`, VERIFY row 2). So the AppMode object **is** the
   config object the devDirs/shipDirs logic operates on: this ties row 2 to the object created here.
   Its full field map is the next thing to walk.
2. The rest of **`0x00f47e90`** after `0x00f481be` — the main loop and the ordered subsystem init
   (it references `0x0067dcd0`/`0x0067dc90` manager getters, `0x00fc1dd0`, `0x00b5fab0`,
   `0x00b5e160`, `0x00b3d320`, `0x00685500`).
3. **Config / state text**: the game's `trigger` resources are text (we decode them already); find
   where the run path reads them (likely through the AppMode subsystem) and where SPUI is spawned.
4. The named-object guard uses `Global\`; the beta PDB names (SP::/EA::) can label these once we
   match by identity (see `analysis/name_guesses.md`).

## Note on the beta PDB

The PDB is a different link, so it cannot supply these addresses. It can still NAME them once
identity is matched; `analysis/name_guesses.md` is the (partial) guesser for that.
