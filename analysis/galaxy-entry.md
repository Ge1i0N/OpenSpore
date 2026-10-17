# GalaxyGameEntry — GA driver boundary and primary UI subset

Reviewed **2026-10-15**. GA-unpacked SHA-256:
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static instructions and persistent Ghidra pseudocode are retained in `audit/galaxy-entry/`.
Static evidence does not observe the original runtime. All addresses below are GA VAs.

## What is answered

- Registrar `0xDDDE30` allocates a **0x10-byte driver**, installs vtable `0x147D2B0`, zeroes fields
  +8/+C and registers `(driver, 0x02CCD1D2, "GalaxyGameEntry")` at registry slot +0x20.
- `0xDDDBE0` allocates `UI/GGE/PatchManager`, **0x2080 bytes**, and retains it at driver +0xC.
- `0xDDDF80` allocates a **0x274-byte controller** through `0xDE6B90`, stores it at global
  `0x16A1344`, links the patch manager at controller +0xF0, sets up property defaults/overrides,
  initializes state objects via `0xDE7660`, sets controller +0x1EC to 1 and registers a `levels` verb.
- `0xDDD960` calls `0xDEA1B0` on that controller. It activates effects/UI (`0xDE46D0`,
  **`0xDE5900`**), subscribes message IDs and activates a current internal state handler.
- `0xDDD9E0` delegates controller deactivation to `0xDE8160`; `0xDDDC50` removes additional
  registrations/resources and releases the patch manager. These method-role labels are inferred
  from side effects, not recovered original symbols or a fully traced mode-manager caller.
- The GGE message listener `0xDE99B0` is now bounded: event-object type `0x287259F6` is checked,
  event IDs are read through object vtable `+0x1C`, and IDs `0x062D88E9`/`0x061DD69B` assign
  controller `+0x104/+0x108 = 2`. ID `0x0615B966` assigns 5, `0x07BFB76E` assigns current 2,
  and `0x070D74E7` can assign 13 after guards. ID `0x036C4A10` calls the StateManager getter
  `0x0067DCE0` vtable `+0x28(1)`, matching the SDK `IStateManager::SwitchToState` slot. The
  later `DF9740` indirect vtable invocation remains a separate static boundary; these assignments
  are not runtime observations or readiness claims. See [message dispatch evidence](audit/gge-message-dispatch/README.md).
- `0xDDDB80` forwards two float inputs to `0xDE8710`, converts the second to an integer using
  the binary's scale constant, then updates two further managers. Controller +0x108/+0x10C are
  current/requested internal state fields, not the outer AppState ID. Our host does not execute this.

## Vtable slots retained, not guessed into a renderer

| Offset | Target | Observed side effect / boundary |
|---:|---|---|
| +0x00 / +0x04 | `0xC6AB00` / `0x7B87B0` | reference count at +8 |
| +0x08 | `0xDDDE90` | destructor / patch-manager release |
| +0x0C | `0xDDDBE0` | patch-manager creation |
| +0x10 | `0xDDDF80` | controller and auxiliary registration setup |
| +0x14 | `0xDDDC50` | cleanup/release |
| +0x18 | `0xDDD960` | controller activation path, includes UI |
| +0x1C | `0xDDD9E0` | controller deactivation path |
| +0x20 | `0x1082A20` | immediate `ret 4`; not a discovered galaxy render routine |
| +0x24 / +0x28 | `0xDDDA40` / `0xDDDA30` | controller delegates |
| +0x2C / +0x30 / +0x34 | `0xDDDAC0` / `0xDDDA50` / `0xDDDB50` | input-shaped controller delegates |
| +0x38 | `0x6F2F90` | not attributed here |
| +0x3C | `0xDDDB80` | update-shaped forwarding described above |
| +0x40 | `0x111CD70` | returns zero, `ret 4` |

Nine previously uncarved vtable targets were explicitly seeded into the persistent project using
`analysis/audit/gge-seeds.tsv`. No full reanalysis or beta addresses were needed. Seeded names ending
in `_candidate` are analyst labels, not proof of original symbol identity.

## The default UI is established by code, not filename similarity

Wide-string constructors at `0x136BCE0/0x136BD00/0x136BD20` initialize:

| Runtime string global | Default | Hash / corpus |
|---|---|---|
| `0x16A0D24` | `GlobalUIGGE-9` | **0x52C07413**; PatchData rec 10642 / Spore_Graphics rec 10399; 249,031 bytes, 1,368 objects / 320 windows |
| `0x16A0D34` | `GGEZoomPanel` | **0xCB6A4669**; PatchData rec 3113 / Spore_Graphics rec 2987; 17,091 bytes, 67 objects / 13 windows |
| `0x16A0D44` | `GGEZoomPanelV2` | **0x28F6D535**; EP1_Data rec 12415; 28,910 bytes, 117 objects / 21 windows |

The record numbers are measured for the project's package corpus. `audit/galaxy-entry/ui-corpus.json`
retains decoded-resource hashes and counts. Corresponding string bytes/addresses are in `binding.json`.

`0xDE5900` selects ZoomPanel/V2 using `0x6854E0(2)` (branch interpretation not fully reversed),
then hashes `0x16A0D24` with seed `0x811C9DC5`, mode 1 (`0x932F00`) and loads GlobalUI. The descriptor
also carries request tag `0x0510A95B` and `0x40464100`. These are **not** PNG type/group IDs.
GlobalUI vtable +8 -> `0xE035F0` -> `0x8121C0`; the loader path forwards a mount operand
`0x2EDD95CA` through `0x812100` to `0x8106C0` for parent lookup. We do not replace the serialized
root ControlID with this operand or treat it as a SPUI object index.

REA corroborates the layout-object bridge: `DE5900` calls `812250` with the selected
`GGEZoomPanel`/`GGEZoomPanelV2` name, tag `0x40464100`, mode `1`, and request `0x5B598FA`; `812250`
builds the three-word layout key and forwards it to `8121C0` (`ev_34068f9104b19813a3645104c02fb607788751fbe79b5dca154ae7e978743635`,
`ev_4319d8ea585fb38746db054f3e40a2ee71cd433e9ef30fc667e867f732422452`). `E035F0` separately
calls `8121C0(param_1,1,0x2EDD95CA)` while constructing/loading `UI/GGE/GlobalUI`
(`ev_1e4b5b023c205f50f186157c64950c166ebeb1cbb9ab7a417a9294ef6fa65b14`). Thus the native
`UI/LayoutObjects` owner path is confirmed for GGE layouts. The per-card `0x02E5A4A8` root lookup
is separately resolved below; it is not an additional unidentified global service.
REA's function bodies make the GGE layout handoff explicit: `DE5900` calls `812250`, which parses
the selected panel name with `0068D830` and invokes `8121C0`; `E035F0` constructs `UI/GGE/GlobalUI`
and calls `8121C0(param_1,1,0x2EDD95CA)` directly
(`ev_34068f9104b19813a3645104c02fb607788751fbe79b5dca154ae7e978743635`,
`ev_e9016910838a25659be2b71d1a0138bba47ee996508b74a58d6bb5ab21171410`,
`ev_e8b06532d45258569292eca19dbad6b337cc9722fa1584b4b2b5e86e207097`). The resulting
`UI/LayoutObjects` owner is therefore a real GGE layout path. Native registration of its components
is not observed merely by finding the exact per-card root in SPUI.
The broader layout command path is not a mount-completion shortcut: REA identifies `812590`
as the UI layout console/diagnostic handler, with list/load/unload behavior and parsed hex arguments;
it forwards layout requests to `812250`, but has no resolved direct callers. This preserves the
distinction between layout loading and native window/component registration.
The key parser `0068D830` is now independently corroborated: it hashes the name before the first
dot into word 0, writes caller tag `0x0510A95B` into word 1, and writes descriptor `0x40464100`
into word 2; suffixes after a dot are parsed separately when present
(`ev_1774a78a641dee54932082d8e71e8772f970dde3def6d9faccd4b2e5476ab3c3`). This confirms the
GGE layout key framing without treating these words as SPUI root IDs or PNG group/type IDs.

**Defaults can be overridden:** `0xDDDF80` reads property strings including `0xAF963138`,
`0xD6F8F4A4`, `0x9E64A562` into these globals. Current subset uses the recovered defaults, not an
implemented GGE configuration property-list consumer. BSS runtime globals are not file-backed
values: `PE.read` beyond a section's raw length can misleadingly expose unrelated bytes.

## Implemented now — deliberately bounded

**2026-10-17 correction to the historical provider interpretation above:** `0x02E5A4A8` is now
identified as per-star layout root177. `DF6110` calls `810650` on the UI receiver loaded from that
record's `+B8`; the wrapper searches that UILayout's components, not a global service registry.
See [per-card window lookup](gge-window-lookup.md) for instructions and exact asset identity.
This removes an alleged unknown display-service identity, not the missing original GGE lifecycle,
native mounting/component registration, camera producer or catalogue readiness.

`engine/mode_ui.cpp` exposes only the recovered **default primary UI binding**. A state whose
script names `mode GalaxyGameEntry` now presents `GlobalUIGGE-9` through the existing typed
drawable renderer. Script-owned `uilayout` requests retain priority; other modes remain unsupported.
The immutable layout/root IDs are not modified and missing resources are not replaced with a
made-up screen. The visible launcher control/state bridge is restricted to DevLauncher/E3LaunchScreen;
GGE's command IDs are not interpreted using that shortcut.

This removes the generic renderer-less placeholder for the GGE primary UI, **not** the mode's
missing controller lifecycle. The screen explicitly says that the galaxy scene, zoom panels and
controller events are unimplemented. [Diagnostic capture](shots/gge_primary_ui.png).
Only **9** serialized windows are drawn by current visibility policy; the many hidden branches
are not forced visible. This sparse result is a baseline for actual WinProc/controller work,
not an approximation of a working galaxy selector.

## Validation and remaining work

- `mode_ui_verify`: exact case-insensitive mode matching, unsupported/null names, primary name/hash,
  citation/mount operands PASS. This is a binding test, not driver execution.
- Live `-w -devDirs -state:Main -quitAfter 30`: Main -> GalaxyGameEntry -> primary layout;
  capture written, boot 10/10, exit 0, aborts/asserts 0.
- Existing SPUI corpus, malformed-input, atlas and clipping regressions remain required.
- Live round trip `Main -> GGE`, console `state:DevLauncher`, then visible click `(311,550)` ->
  CreatureEditor passes, with no stale GGE screen, aborts or asserts. Full GGE driver teardown
  is not simulated: this checks our stateless primary-UI binding only.

**Follow-up:** the shared handler for states 0/1/8/9 (`0xDEA4B0`) is now identified as **Intro**.
Its title path, MoviePlayer/phase guard, DBCheck notification and post-intro request are traced in
[gge-intro.md](gge-intro.md). A bounded gate model is tested; missing runtime services do not
auto-complete it. The primary frame remains a diagnostic view, not the handler's original visibility
lifecycle. Next: actual MoviePlayer/no-movie contract, DBCheck consumers and the state-2 scene handler.
Scene/camera/effects, save selection, zoom composition and messages remain open; no playable claim.
The state-2 follow-up is now [Play Spore / DBCheck](gge-dbcheck-play.md): its live game-record and
per-star UI/effect dependencies are identified; they are not replaced with fabricated star data.

Reproduce retained evidence:

```powershell
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py
python tools/gge_evidence.py --static
```
