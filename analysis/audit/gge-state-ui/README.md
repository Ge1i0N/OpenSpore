# GGE internal state → UI synchronization

Reviewed **2026-10-17**. Static GA-unpacked Ghidra evidence only; no original runtime execution.

`DE48C0` maps the controller's internal state field `+0x108` to UI resource/control handling. The
body initializes an event/control ID table containing `0x061DD69B`, `0x0615B966`, `0x0615B616`,
`0x070D74E7` and `0x070D7509`. For state `5`, it selects `0x061DD69B`; for state `13`, it selects
`0x070D74E7`. It then looks up controls through `00810650` and applies vtable `+0x28` flags and
related UI operations. Helpers `DE4A20`, `DE4AE0` and `DE4BD0` load/configure separate GGE UI
objects and update internal mode resources.

This is a state-to-UI path, not the Play Spore catalogue rebuild itself. `DF9740` remains the
GA-specific vtable slot whose state-2 guard calls `DF95A0`; no direct `DE48C0 → DF9740` edge is
claimed. The matching IDs in `DE99B0` therefore remain event/state feedback candidates until a
specific caller and payload route is recovered.

### Additional GA byte-level cross-check

The local GA `objdump` pass found the two direct immediate references to `0x070D7509`:

```text
0x00DE4918  inside DE48C0
0x00DE595F  inside DE5900
```

Both occur beside the distinct value `0x070D74E7`, matching the existing state/UI table rather
than a button event comparison. `DE48C0` uses the table while looking up UI objects and applying
state operations; `DE5900` initializes the GGE UI path, reuses the same IDs through the object
lookup/vtable `+0x104` route, then calls `DE48C0` before continuing initialization.

This strengthens the classification of `0x070D7509` as a shipped GA GGE UI/control identity. It
does **not** identify a mouse/button producer, a CommandID route, or a post to ScenarioMode tag
`0x01654C10`; those remain Unknown.

### Native attachment cross-check

`DE5900` is now bounded as the native attachment owner for this table. It requests the GGE layout,
looks up each identity through `00810650`, and calls the resolved window's vtable `+0x104`
(`IWindow::AddWinProc`) with the GGE object subinterface at `object +0x04`. `DE6120` initializes
that subinterface with vtable `0x0147E030`, whose slot `+0x18` is `DE99B0`.

Consequently, `0x070D7509` is not merely a serialized orphan: under the observed `6854E0(2)` guard
it receives the same native GGE listener attachment as `0x070D74E7`. This proves the control-to-
listener registration edge, but not the later event payload conversion. The listener's recognized
component ID remains `0x070D74E7`, distinct from the Play Scenario ControlID `0x070D7509`.

The locale reference on EP1 object 87 resolves to **Galactic Adventures** (`table 0x3DADE1BB`,
`key 0x074BDFBA`), while object 74 has the inline caption **Play Scenario**. The state-13 UI value
is therefore semantically the Galactic Adventures control/state identity, not a hidden generic
payload name for object 74. A parent/controller route between the two controls remains Unknown.

### 👀 Static feedback-loop cross-check

The same identity is present on both sides of the bounded GA path:

```text
DE99B0 component 0x070D74E7
  → ScenarioGAButtonClicked = true
  → state 13

DE48C0 state 13
  → lookup/update control 0x070D74E7
```

Together with the resolved `Galactic Adventures` caption, this is a strong static explanation of
the GA-tab feedback surface. It remains separate from the unresolved `Play Scenario` object 74
producer and from runtime ScenarioMode entry.

The wider Scenario property family has a sibling activation consumer at `DF43A0`: component
`0x053F0707` writes `ScenarioSpaceGameLaunched` (`0x080EAA93`) through Preferences and updates UI
target `0x0809B720`. This corroborates the property-backed design of Scenario launch surfaces while
remaining distinct from object 74 / `Play Scenario 0x070D7509`.

### Direct caller inventory

The local `.text` scan for direct `E8 rel32` calls found:

```text
DE48C0 callers:
  DE5A86
  DE8A71

DE5900 callers:
  DEA1B0
```

`DE5900` is therefore only reached directly from the already mapped
`GalaxyGameEntry` activation-shaped path `DEA1B0`; this supports its lifecycle/UI-initialization
classification rather than a button producer role. `DE48C0` is reached from that initialization
via `DE5A86` and from the `DE8A71` call instruction inside `DE8710`. No direct caller from the bounded
UTFWin Button/CommandID route was found in this inventory.

`DE8A71` is an interior call instruction in `DE8710`, not a separate function entry needing its own
caller. The full Update-shaped body normalizes `+0x108/+0x10C`, selects a controller from the
ordered map at root `+0x124`, switches root `+0x38` with exit/enter virtual calls, then invokes
`DE44A0` and `DE48C0`. See the [corrected controller route](../scenario-mode/launch-state.md).

### Adjacent GGE message-handler slot

The adjacent body `DE8A80` has no direct `E8` caller, but its function address is present in the GA
vtable at:

```text
vtable 0x0147E014
slot +0x10  -> 0x00DE8A80
```

Its bounded body compares several internal message IDs and calls GGE lifecycle/UI helpers including
`DE4AE0`, `DE4A20`, `DE4BD0`, `DE5210` and `DE5280`. It does not compare the `Play Scenario`
ControlID `0x070D7509`; the only two direct immediate references to that value remain the
state/UI-table writes in `DE48C0` and `DE5900`. Therefore `DE8A80` is a real GGE message-handler
seam, but not yet the Play Scenario producer.

The bounded `DE8A80` dispatch scan also contains none of the three IDs needed to close the current
gap:

```text
0x070D7509  Play Scenario ControlID
0x00E1132F  known CommandID candidate
0x01654C10  ScenarioMode tag
```

Its role is narrowed to shared/internal GGE message handling plus lifecycle/UI helpers; the
upstream producer and payload route remain Unknown.

One bounded branch in `DE8A80` is also separated from the UI-click hypothesis:

```text
message ID 0x99EDA4CD
  → DE6E30
```

`DE6E30` performs a resource/property-shaped lookup using observed constants:

```text
type  = 0x011989B7
group = 0x4084A100
```

and works through object state around `+0x70/+0x74`. This is retained as an internal GGE
resource/state branch; it is not promoted to Button, CommandID or ScenarioMode behavior.

### Shared multi-vtable GGE object

The constructor/initializer body `DE6120` writes four vtable pointers into one object:

```text
object +0x00 = 0x0147E04C
object +0x04 = 0x0147E030
object +0x08 = 0x0147E020
object +0x0C = 0x0147E014
```

This connects the two previously separate observations:

```text
object +0x0C → vtable 0x0147E014 → DE8A80
object +0x04 → vtable 0x0147E030 → DE99B0
```

The object is therefore a multi-interface/multi-vtable GGE object, not two unrelated handler
functions. `DE99B0` remains the observed event-listener/state branch; `DE8A80` is the adjacent
internal message-handler interface. This structural connection still does not identify the
upstream `Play Scenario` producer or a ScenarioMode post.

**Historical interpretation, superseded by receiver-coordinate correction below.**
`DE99B0`'s optional forward target is also bounded separately: its controller/object `+0x34` is
zeroed with neighboring `+0x30/+0x38` by the base initialization at `DE60A0`. No direct population
write was found in the inspected GGE constructor path, so the alternate forwarded listener route
for payload `0x070D7509` remains Unknown.

The population mechanism is now recovered outside that constructor path: `DF6820` releases the
old `+0x34` target, stores and retains a new target, then invokes target vtable `+0x48`. Its direct
callers are `DE317A` (target `[owner+0x48]+0xB8`) and `DF851E` (target from the per-entry/event
path). `DE317A` is vtable method `0x0147DC50 +0x0C` after a bounded time/update guard. Their
relation to the Play Scenario control remains Unknown; no runtime delivery is claimed.

One concrete writer branch at `DF79FE` is now identified as the per-star stage-caption path: it
handles `kMsgComponentActivated` with component ID `0x03C4925E`, stores controller `+0xFC` into
owner `+0x48`, and requests internal state `11`. Shipped `GGEGameStarUIv4` data labels that ID as
the stage-caption ControlID. It is positive evidence for forwarded-target population, but not
evidence that `0x070D7509` reaches `0x070D74E7`.

The target source is also resource-backed: `DF1A55` loads a value under key `0xD047A893` into a
related object's `+0xB8`, which is the value later consumed by the `DE317A → DF6820` registration
expression. The key's semantic name and relation to Play Scenario remain Unknown.

`DF9288` independently creates per-entry presentation objects and initializes their local `+0x48`
to an internal `+0x4C` storage area. This is distinct from the stage-caption branch's global owner
write `0x016A11B4 +0x48 = controller +0xFC`. The known forwarded caption target is therefore
controller/service-owned infrastructure, not object 74 (`Play Scenario`) or object 87 (`Galactic
Adventures`).

The other direct setter caller, `DF851E`, is inside the `DF8300` per-record marker/effect loop. It
passes the selected effect object to `DF6820` after configuring that effect with the record's
projected transform. This bounds the optional GGE `+0x34` forwarding path as visual marker/effect
infrastructure in the observed per-entry route. It must not be promoted to the Play Scenario or
ScenarioMode handoff without a separate payload and target observation.

`DF9160` is the matching per-record cleanup path: it sends the effect's internal message `9`, then
releases and clears the target. This lifecycle symmetry reinforces the effect/marker classification,
while leaving the Play Scenario producer and selected-star-to-ScenarioMode boundary unresolved.

### Receiver-coordinate correction (2026-10-18)

`DE99B0` receives the attached GGE WinProc interface at **GGE root +4**, not the complete root.
Its relative `+0x34` therefore reads **root +0x38**, the active controller selected by `DE8710`.
The forwarding slot `+0x58` resolves to `DFE430` when key `13` selects the Scenario controller.
`DF6820` accesses a different receiver's relative `+0x34` in marker/effect infrastructure. Its
callers do not establish writers of the GGE active-controller field. The earlier shared-offset
interpretation above is retained only as superseded investigation history.

`DE8710` is now a resolved writer: map at root `+0x124`, key current state `+0x108`, conditional
override to key `1` when `+0x1EC` is in `1..5`; then old exit `+0x10`, store root `+0x38`, new
enter `+0x0C`. `DE7660` registers the `DFB990` Scenario instance under key `13`. The distinct
`0x070D74E7` event writes state `13`; the `0x070D7509` action remains unjoined. These are static
edges, not native execution or readiness observations.

### Constructor boundary

The direct-call scan found only one direct caller of `DE6120`:

```text
DE6E13
```

The surrounding wrapper `DE6E10` calls `DE6120`, optionally frees the receiver when its delete flag
is set, and returns the object. `DE6E10` itself has no direct `E8` callers in the selected `.text`,
and simple `.rdata`/`.data` scans found no raw function-pointer reference to this wrapper family.
The safe conclusion is a bounded constructor/lifecycle seam with an indirect or otherwise
unrecovered allocation entry; no hidden caller or registration owner is invented here.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --state-ui
```
