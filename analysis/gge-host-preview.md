# GGEHostPreview — first physically clickable controlled card screen

Reviewed **2026-10-18**. This is an explicitly named **Debug/dev-only host reconstruction**, not
the original GGE state, native scene or live catalogue.

## Run it

```powershell
cmake --build build --config Debug --target OpenSpore
.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:GGEHostPreview
```

The reproducible launcher is retained at:

```text
tools/run_gge_host_preview.bat
```

It builds `OpenSpore` if needed and starts the Debug/dev-only controlled preview. It is not a native
GA runner. It also injects the existing `(640,360)` host-picking smoke click and exits after 30
frames.

### Explicit real-save host preview

The same Debug-only state accepts a separate host format built from explicitly selected structural
save observations and an equivalent terrain-property correlation:

```powershell
python tools/gge_real_save_host.py `
  C:\captures\gge-observations.json `
  C:\captures\gge-terrain-correlation.json `
  C:\captures\gge-real-save-host.txt

.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:GGEHostPreview `
  -exec "ggepreview:load,C:\captures\gge-real-save-host.txt" -execAfter 10
```

`GGE_HOST_REAL_SAVE_V1` is not `GGE_CATALOG_V1`: it contains saved rows only, explicitly empty
candidates and Unknown manager/stars.db/planet/refill statuses. The scene allows this caller-labelled
host format solely to render real saved cards; no native catalogue readiness, candidate refill,
camera ownership, WinProc/CommandID dispatch or gameplay transition is implied.

The snapshot inspector labels this input as `host_only=true`; ordinary `GGE_CATALOG_V1` admission
rules remain unchanged.

It presents three controlled cards using the actual `GGEGameStarUIv4` SPUI layout, the actual atlas
PNG from explicitly selected `Spore_Graphics.package`, typed in-memory candidate metadata, an
explicit identity camera observation, the host scene/lifecycle, renderer and normal input latch.

The screen labels itself **GGE HOST PREVIEW (CONTROLLED)** and says that no native catalogue/camera/
callback is attached. It is registered only when `OS_ENABLE_DEV_COMMANDS` is set; release builds do
not register it.

`ggepreview:status` reports the current host status, structural catalog source, card count and
producer observations in order `manager,starsdb,planet,refill`. Built-in entries have empty source
and Unknown producer statuses; caller snapshots report their explicit path and statuses.
The status command also reports optional capture observations in order
`resource_root,resource_index,resource_context,resource_class` and `refill_append`; absent optional
fields remain `Unknown`. The Debug overlay shows the same values compactly as `R/I/C/K` plus append.
An explicit snapshot with any producer status other than Available is rejected with
`caller catalogue producer observations are not fully Available`; the existing built-in scene remains
active rather than being replaced by a blocked or empty scene.

Automated click through the same frame input path:

```powershell
.\build\bin\Debug\OpenSpore.exe -w -devDirs -state:GGEHostPreview `
  -exec spui:-click,640,360 -execAfter 10 -quitAfter 30
```

Observed controlled log:

```text
[gge-preview] picked host card 20481 (host only) at 640,360
```

The click is injected by `input::inject_click`, latched by the normal frame loop and consumed by
`StarUiScene::pick`; it is not a parallel test-only hit function. The selected card receives a
visible host-only border and identity label. The result intentionally ends at host identity: no
DF7BF0/DF7AC0, CommandID, selected StarRecord or state transition is called.
Hover is shown separately with a cyan host-only border; click selection is yellow. This tests pointer
feedback without treating hover as the native F8 route or selection as native FC.

## Explicit host-only ScenarioMode diagnostic

After a card has been selected, the Debug-only console command:

```text
ggepreview:scenario
```

copies the selected card's host metadata into an explicit diagnostic request and queues the separate
`ScenarioHostDiagnostic` state. The diagnostic displays:

```text
selected host star
requested GA mode tag = 0x01654C10 (ScenarioMode)
native ScenarioMode = NotExecuted
```

This is deliberately a **HostOnly** handoff. It does not call the native mode registrar, create a
`ScenarioMode` driver, load a planet/stage, emit `ScenarioGAButtonClicked`, or claim that the original
Play Scenario control was clicked. It exists to make the selected-star → requested-mode boundary
executable and inspectable without promoting the unresolved native producer path.

The preview still reports the selected card first; the command must be issued after that selection.
Returning with `state GGEHostPreview` reopens the controlled card scene. The diagnostic is registered
only under `OS_ENABLE_DEV_COMMANDS`.

The direct empty-request smoke launcher is retained at:

```text
tools/run_scenario_host_diagnostic.bat
```

It intentionally enters `ScenarioHostDiagnostic` without a selected-star request, so the visible
result is the explicit failed/no-request branch. The successful HostOnly handoff still requires a
card selection followed by `ggepreview:scenario`.

## Boundary

The preview uses explicit controlled camera/catalogue data because no real `stars.db`/save producer,
native active-camera owner or callback/message bus is available. It is useful for testing card
appearance, atlas loading, visibility, pointer coordinates and host picking, but is not recovered
native GGE gameplay.

The original star layout remains mostly hidden under the current phase-1 subset. The preview requests
an explicit case-2 host transition to reveal the observed panel subset; it does not clear root
IgnoreMouse, force every panel visible or synthesize a native phase latch. Normal GGE is unchanged.

## Verification

- Debug x86 build warning-free.
- `GGEHostPreview` smoke entered, real frame loop reached, atlas/camera/scene setup succeeded,
  controlled click returned host card **20481**, clean exit code0, aborts/asserts0.
- Scene204, transition195, lifecycle232, streaming95, AnimatedIcon159 and image80 suites remain
  passing; launcher capture remains pixel-identical to `analysis/shots/spui_e3launch_uv.png`.
- No native-GGE/full-galaxy screenshot claim: art is real shipped SPUI/PNG, catalogue/camera are
  controlled typed observations.

Next: attach genuine catalogue and active-camera/input owners, then compare against original
phase/callback behavior. The new host-only selected-star handoff does not change that boundary.
MoviePlayer/DBCheck, factories, effects and gameplay remain unresolved.

Final Debug x86 warning-free. The controlled preview smoke entered the state, rendered 30 frames,
processed `spui:-click,640,360`, logged host card20481 and exited code0 with aborts/asserts0.
The full prior suites remain passing; this state is deliberately excluded from release registration.
