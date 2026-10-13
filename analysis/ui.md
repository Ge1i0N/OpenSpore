# UI (SPUI) — how the game's menus are built

Authority: disassembly of `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe` + the shipped packages.

## The UI is data-driven: `spui` layouts

Menus/screens are not code — they are `spui` resources (type **`0x250fe9a2`**), binary UI layouts.
Our inventory finds **524 distinct `spui` screens**, **407 with real names** (FNV-1-lower of
`reg_file.txt` names). The name list is in `analysis/spui_screens.txt`. It includes the menu family:
`TitleScreen`, `LaunchScreen`, `GameLaunchScreen`/`2`/`3`, `GameLaunchScreen-EP1`, `SporeTitleSeq`,
`CSATitleScreen`, `E3LaunchScreen`, `GameSettings*`, `GlobalUIOptions-2`, `MessageBox`,
`NetworkLoginDialog`, plus every in-game HUD (`GlobalUI*`, `EditorVerbTray`, `PosseBar`, ...) and the
`timelineEvent*` icons.

## Resource types the UI uses

The UI loads its assets by (type, group, instance). The relevant types (resolved via
`reg_type.txt`):

| type | id | meaning |
|---|---|---|
| `spui` | `0x250fe9a2` | a compiled UI screen layout |
| `layout_bin` | `0x050f4731` | UI layout (binary) |
| `layout` | `0xefbda3ff` | UI layout (source) |

The binary builds a `{spui, layout_bin, layout}` descriptor and asks the resource manager for it:
`0x0080fff0` does `mov [esp]=0x250fe9a2; mov [esp+4]=0x50f4731; mov [esp+8]=0xefbda3ff;
call 0x8de4b0 (GetResourceManager); [mgr]->vtable+0x24(...)` with an id (e.g. `0x510a95b`). So a
screen is requested by id and may exist in any of the three layout types (`layout`/`layout_bin`
sources are not present in our packages; `spui` is).

## The SPUI window classes are registered from one place

`UTFWin/cSPUI*Win` class names live at `.rdata 0x1416c40..` (`cSPUIFrameSequencer`,
`cSPUILaunchScreenWinProc`, `cSPUIAnimatedIconWin`, `cSPUIProgressBarWin`,
`cSPUIProgressBarGradientWin`, `cSPUIPopupMenuWin`, ...), and `UI/cSPUIAnimator` /
`UI/cSPUIAnimationSequence` at `0x14155e4..`. They are registered by the function cluster around
**`0x8009f0`..`0x800b60`**: each allocates a fixed-size object (`call 0x951230`), constructs it
(`call 0x804f10` / `0x80cfb0` / ...), and registers it against a name string and a class id (e.g.
`push 0x1416c50` = `UTFWin/cSPUIFrameSequencer`, then `[obj+0x8]->vtable+0xc`).

## Path to "the Loader shows the menu"

1. Parse the `spui` binary layout (the screen tree: windows, their classes from the table above,
   positions, and referenced image/font resources). This is the missing piece — like `gmdl`, it must
   be derived from the data + the reader.
2. Implement the small set of SPUI window classes the title/launch screens use (buttons, images,
   text), with the UTFWin/immediate 2D path.
3. Feed them the decoded `png`/`raster` UI art and the `locale` strings we already decode.

Our engine already has the 2D overlay (rects + text + images), the `png`/`raster` decoders and the
`locale` text; the remaining work is the `spui` layout parser and the window-class behaviours.
