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
via `DE5A86` and from the separate `DE8A71` helper/update path. No direct caller from the bounded
UTFWin Button/CommandID route was found in this inventory.

`DE8A71` itself has no direct `E8` caller in the selected `.text`, and its exact entry/dispatch
owner was not recovered from simple `.rdata`/`.data` function-pointer scans. The bounded body still
shows a state `+0x108/+0x10C` normalization path followed by `DE44A0` and `DE48C0`. Its dispatch
mechanism therefore remains Unknown; it is not renamed to a vtable slot or per-frame callback from
this negative search.

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
