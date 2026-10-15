# GGE input boundary — world ray selection vs UI window picking

Reviewed **2026-10-15**. GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static GA instructions + actual SPUI layouts and controlled host rays; no original input runtime.

## Two distinct input paths

The native Play Spore handler table stores `+0x5C → DF7BF0` hover, `+0x60 → DF7AC0` selected-entry,
`+0x64 → DF6110` projection/update and `+0x68 → DF6010` per-record UI update. They are not invoked
just because a host window or world ray was hit.

`StarUiScene::pick(frame_serial,x,y)` is the UI path: current projected frame, reversed layer order,
visibility/enabled/IgnoreMouse/Clip and per-instance geometry. It returns value identity (ticket,
frame, host ID, entry index, serialized object, ControlID/CommandID) but does not dispatch CommandID,
alter selected StarRecord/controller pointers or call F8/FC.

`DF6490` is separate. It obtains the active projection camera via renderer `+0x58`, unprojects pointer
coordinates through `0x7C45E0`, then tests each record position (`+0x80`) using `0x698B30`.
The distance helper is point-to-segment; the closest candidate below the native initial threshold
**15.0** (`0x01400864`) wins. `DF65B0` calls this picker under a guard and routes the result through
the hover handler; full tooltip/input scheduling remains unresolved.

`StarWorldRayObservation` requires an explicit ActiveProjection source, nonzero caller token, finite
nonzero direction and positive finite threshold. `world_pick` returns host ticket/entry/ID/distance
only. It never derives a Windows pointer, acquires the camera, writes native +F8/+FC, calls F8/FC,
shows a tooltip, updates phase or dispatches CommandID. Negative/behind-ray behavior follows the
retained point-to-segment calculation rather than an invented front-face/depth cull.

`DF7800` raw callback event identities remain separate: event tag `0x4F5527E8`, `(type10,subtype1)`
tooltip userdata, type7 controls `04656452/0638491D`, and tag `0x287259F6` controls
`03C4925E/0375408C/03754098`. No generic click-to-selected callback is fabricated.

## Verification

`gge_star_input_verify`: **29 checks PASS** on both real 152-object/58-window layouts:

- nearest point-segment hit, alternate ray/entry, threshold miss and explicit behind-ray behavior;
- ActiveProjection vs ControllerRetained/zero-token/zero-direction/nonfinite guards;
- stale/closed scene tickets and transition/input separation with no F8/FC/CommandID dispatch.

Inputs/catalogue positions are controlled typed metadata, not a real save/catalogue. Existing
scene204/transition195 tests cover layers and explicit case2..5 visual application. No new GPU,
callback or native galaxy capture is claimed.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_input_verify
.\build\Debug\gge_star_input_verify.exe <SPUI-fixture-directory>
```

Evidence: [facts and extracts](audit/gge-input/facts.json), [input seeds](audit/gge-input-seeds.tsv),
static extracts for `DF6490`, `DF65B0` and `DF7800`, plus [phase boundaries](gge-star-transition.md).
REA MCP was not connected; these
are local Ghidra/objdump results, not REA Evidence IDs or original runtime captures.

Next: preserve raw callback event records and explicit current-entry observations in a host event
port, then connect only proven tooltip/selection service subsets. Native input, controller ownership,
camera producer and gameplay remain unresolved.
