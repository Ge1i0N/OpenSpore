# GGE message-to-state dispatch

Reviewed **2026-10-17**. Static persistent Ghidra evidence from the selected GA-unpacked binary;
no original process was executed. The generated C/instruction pairs are retained in this directory.

## Handler contract

The GGE object vtable at `0x0147E030` contains `0x00DE99B0` at slot `+0x18` (`0x0147E048`).
The body has the shape of a GGE message listener.
It first forwards the message to an optional object at controller `+0x34`, requires UTFWin message
type `0x287259F6` (`kMsgComponentActivated`) at payload word `+0x08`, then calls the component
payload's vtable `+0x1C` to obtain the component/event ID.

Observed GA branches:

| Event ID | Observed state-side effect |
|---:|---|
| `0x036C4A10` | Game-mode/state manager getter `0x0067DCE0`, vtable `+0x28`, called with `1`. SDK `IStateManager` describes `+0x28` as `SwitchToState`; the GA object/argument are observed, higher-level reason is not guessed. |
| `0x062D88E9` | controller `+0x104 = 2`, `+0x108 = 2` |
| `0x061DD69B` | controller `+0x104 = 2`, `+0x108 = 2` |
| `0x0615B966` | controller `+0x104 = 5`, `+0x108 = 5` |
| `0x07BFB76E` | controller `+0x108 = 2` |
| `0x070D74E7` | after its guards, controller `+0x104 = 13`, `+0x108 = 13` |
| `0x02F1CD64` | calls controller-side `E00640(0x42,0)` when `+0x34` exists |
| `0x036850D4`, `0x043B72D0`, `0x0447060A` | call an object obtained from `DF5F10` through vtable `+0x60`; purpose remains Unknown |

`FUN_00DE8680` is the observed internal state setter/queue helper: with its boolean argument false
it writes requested state `+0x10C`; with true it normalizes current states 0/1/8/9 to 2, queues
the current value at `+0x114/+0x118`, and writes the requested value. `FUN_00DF9740` separately
checks controller `+0x108 == 2` before showing GlobalUI, rebuilding cards through `DF95A0` and
continuing through `DF6C10`. Thus the static route is:

```text
DE99B0 event branch → controller state/request fields
  → GGE state/update lifecycle
  → DF9740 (+0x108 == 2)
  → DF95A0 catalogue publication
```

The exact indirect caller that invokes the Play Spore vtable `+0x38 = DF9740` remains a separate
boundary. Setting fields in a static branch is not runtime execution or catalogue readiness.

## ✔️ Milestone: native WinProc attachment for the GGE controls

The GA body `DE5900` is the native attachment path that follows the GGE layout request. After
calling `812250` with layout tag `0x0510A95B`, group `0x40464100` and mount `0x05B598FA`, it builds
the following UI-identity table:

```text
0x0615B966
0x061DD69B
0x0615B616
0x070D74E7
0x070D7509
```

For each resolved window, `DE5900` performs:

```text
00810650(identity)
→ resolved IWindow
→ IWindow vtable +0x104 (AddWinProc)
→ attach object +0x04
```

The first three identities are attached unconditionally; the final two, including the serialized
`Play Scenario` ControlID `0x070D7509`, are attached under the observed `6854E0(2)` feature guard.
`DE5900` is called by `DEA1B0`, not by the generic UTFWin Button dispatcher.

The attached `object +0x04` is not an analyst-invented callback: constructor `DE6120` writes
`0x0147E030` at that subobject, and vtable `0x0147E030 +0x18` is `DE99B0`. Therefore the static
attachment edge is now:

```text
GGE layout/control identity
  → 00810650
  → IWindow::AddWinProc(+0x104)
  → GGE multi-vtable object +0x04
  → vtable +0x18 = DE99B0
```

This closes the **native attachment** gap and explains why the SPUI `WinProcs` property is absent.
It does not yet prove that a click on `0x070D7509` produces component ID `0x070D74E7`: `DE99B0`
still compares those values separately, and no original runtime message has been observed.

## Generic producer boundary now recovered

The GA generic activable-window body at `0x009640B0` constructs and sends the message consumed by
this listener:

```text
object +0x84, fallback +0x80
  → message.type = 0x287259F6 (kMsgComponentActivated)
  → IWindow::SendMsg (+0x114)
```

Its later state branches construct `kMsgButtonClick = 0x17` or `kMsgButtonSelect = 0x18` and send
those through the same window message route. The bounded direct callers are `0x0081CF31` for the
activation producer and `0x0081CA4E`/`0x0081CC90` for the adjacent mouse/button path.

This upgrades the earlier “producer Unknown” result only at the **generic UTFWin layer**. The
attached GGE object and `DE99B0` listener are now connected to a real producer contract, but the
dynamic source of object `+0x80/+0x84` is still not statically joined to `0x070D74E7`. The Play
Scenario ControlID `0x070D7509` therefore remains distinct and the GGE-specific conversion is still
Unknown.

### Payload-field ownership boundary

The generic producer's object-field source is now resolved through the embedded IWindow interface.
GA `0x009640B0` reads:

```text
object +0x84; if zero, object +0x80
```

The relevant WinButton vtable entries are:

```text
0x01441390 +0x94 → 0x009640B0
0x01441270 +0x50 → 0x0095F980 (SetControlID)
0x01441270 +0x54 → 0x0095F990 (SetCommandID)
```

The GA setter bodies are exact:

```text
IWindow +0x7C = ControlID
IWindow +0x80 = CommandID
embedded IWindow is outer WinButton +0x04
outer +0x80 = ControlID
outer +0x84 = CommandID
```

The producer's precedence is therefore:

```text
CommandID if nonzero, otherwise ControlID
```

### DE99B0 forwarded-listener field

`DE99B0` first checks controller/object field `+0x34` and, when non-null, forwards the incoming
message through that object's vtable `+0x58` before applying its own component-ID branches. The
bounded base initialization at `DE60A0` sets the related fields to zero:

```text
object +0x30 = 0
object +0x34 = 0
object +0x38 = 0
```

No direct write that populates this `+0x34` forward target was recovered in the bounded GGE
constructor path. This does not prove that indirect registration never occurs, but it means the
possible alternate handler for payload `0x070D7509` remains Unknown rather than being invented.

### ✔️ Milestone: forwarded-listener setter recovered

The indirect registration boundary is now concrete at `DF6820`:

```text
old_target = object +0x34
  → release old target
object +0x34 = new_target
  → add-ref new target
  → new_target vtable +0x48(...)
```

Bounded direct callers are:

```text
DE317A
  → target = [owner +0x48] +0xB8
  → DF6820(target)

DF851E
  → target from the current per-entry/event path
  → DF6820(target)
```

This closes the setter/registration mechanism for `DE99B0`'s optional forward listener. It does
not yet identify which caller/target is active for the EP1 `Play Scenario` control, and neither
caller body contains a direct `0x070D7509`/`0x070D74E7` join in this bounded pass.

The shipped EP1 layout places the two relevant identities on different branches:

```text
object 138
  ├─ object 74  ControlID 0x070D7509 (Play Scenario)
  └─ object 114 → object 87  ControlID 0x070D74E7
```

Object 114 is the `0x0806AA00` shared GGE target used by the state-2 continuation. This makes the
`0x070D7509` → `0x070D74E7` relationship a possible higher-level sibling/parent route, not a direct
WinButton payload conversion. Its producer and runtime delivery remain Unknown.

The confirmed owner is instead the GA WinButton family initialized by `0x00966EB0`:

```text
object +0x00 = 0x01441390
object +0x04 = 0x01441270
0x01441390 +0x94 = 0x009640B0
```

Thus `0x009640B0` is a native WinButton/activable-window vtable method. This closes method ownership,
and its payload source is now statically tied to the WinButton's `ControlID`/`CommandID` fields.

For EP1 object 74 (`Play Scenario`), the serialized pair is `ControlID=0x070D7509`, `CommandID=0`,
so its generic activation payload is statically `0x070D7509`. EP1 object 87 separately carries
`0x070D74E7`; the direct same-listener conversion to the `DE99B0` property branch is not proven.

The seven state/UI constants used by the bounded handler/helper bodies were also found as actual
SPUI `ControlID` properties in the shipped corpus; see [the data cross-check](control-id-correlation.md).
The bounded GA immediate scan found no separate MessageManager producer for them, so they remain
ControlID/handler identities rather than independently proven message producers.

Do not conflate `ControlID 0x070D7509` (`Play Scenario`, EP1 `GGEZoomPanelV2`) with handler/state
value `0x070D74E7`; the values differ and the bounded native scan finds the former only in UI setup.

Conversely, `0x036C4A10` is directly corroborated as the `Dev` ControlID in `GlobalUIGGE-9` and as
the `DE99B0` branch that invokes StateManager-compatible vtable `+0x28(1)`. This is the current
strongest shipped-UI-to-state link. The state script identifies target id 1 as `DevLauncher`, not
Play Spore state 2; it is still static evidence, not a runtime click observation.

The related state-to-UI helper `DE48C0` maps controller state 5/13 to UI IDs `0x061DD69B` and
`0x070D74E7`, respectively, via `00810650` and control vtable `+0x28`. Its retained evidence is
separate in [gge-state-ui](../gge-state-ui/README.md); matching IDs are not treated as a proven
message feedback loop without a caller/payload observation.

## Exact `0x070D74E7` feedback branch

The local GA instruction pass further bounds the separate `0x070D74E7` message/state route. After
the ID comparison, the body:

```text
guard 0x6854E0(2)
→ object/resource lookup
→ 0x00810650 lookup with UI identity 0x0806A310
→ resolved UI vtable +0x7C(1,0)
→ controller +0x104 = 13
→ controller +0x108 = 13
```

This is a concrete state-feedback branch for `0x070D74E7`, subject to its guards. It does not use
`0x070D7509`: the latter remains present only in the bounded `DE48C0`/`DE5900` UI setup tables,
with no click producer or ScenarioMode post established.

## Shared UI target `0x0806A310`

The same UI identity used by the feedback branch is also referenced from `DF6C10`, the common
continuation reached by the Play Spore state-2 enter/refresh path. It is close to `DF6D80` in the
address space, but it is **not** the `DF6D80` phase-1 body:

```text
DE9C88  → 0x00810650 → 0x0806A310 → UI vtable +0x7C
DF6C56  → 0x00810650 → 0x0806A310 → UI vtable +0x7C
```

Direct-call inventory for the continuation is:

```text
DF6C10 direct call: DF9620
DF9740 → unconditional tail jump → DF6C10
DF6D80: separate function with its own eight direct callers
```

This corroborates `0x0806A310` as a shared GGE control/state target across feedback and the
Play-Spore state-2 common continuation. It still does not identify the upstream `Play Scenario`
click producer, and it does not turn `0x070D7509` into the state-13 event value `0x070D74E7`.

The related UI target `0x0806AA00` has two bounded GA references:

```text
DF6CBA  → state-2 continuation branch
DF9112  → separate GGE per-entry/UI lifecycle cleanup path
```

This is another shared UI target rather than a unique Play Scenario control. Its semantic name and
the exact relationship between the two lifecycle uses remain Unknown.

## ✔️ Milestone: semantic names for the state-2 Scenario properties

The GA bytes use two previously opaque property IDs in the `DF6C10` continuation:

```text
0x080D7DE1
0x080D97FE
```

Secondary registry/config references independently label these IDs as:

```text
0x080D7DE1 = ScenarioGAButtonClicked
0x080D97FE = ScenarioGAFirstLaunched
```

Sources for the names are the local SporeModder-FX `reg_property.txt` and the user-supplied final
`DataEP1/Config/Properties.txt`; they are semantic corroboration, not GA address authority. The
primary GA fact is stronger and separate: `DF6C10` actually queries both IDs, reads their boolean
property payloads, and changes the GGE UI targets `0x0806A310` and `0x0806AA00` accordingly.

This is the first concrete static link between the “Scenario GA button” semantic surface and the
Play Spore state-2 continuation. It narrows the missing route from an unknown generic Button path
to a property-mediated path. It is **not** yet a runtime click observation and does not prove that
the property write enters `ScenarioMode` or posts tag `0x01654C10`.

## ✔️ Milestone: GA Scenario property family mapped statically

The same local registry/config names can now be correlated with a broader set of immediate
references in the selected GA `.text`:

| Semantic property (secondary name) | ID | Bounded GA reference(s) |
|---|---:|---|
| `ScenarioStarEffectsEnabled` | `0x08069A6C` | `DE5900` |
| `ScenarioGAEditorEffectsEnabled` | `0x0806DBBA` | `DFA8F3` / nearby continuation |
| `ScenarioGAButtonClicked` | `0x080D7DE1` | `DF6C10`, `DE9C` |
| `ScenarioGAFirstLaunched` | `0x080D97FE` | `DF6C10` |
| `ScenarioSpaceGameLaunched` | `0x080EAA93` | `DEE316`, `DF4028` |
| `ScenarioCaptainEditorLaunched` | `0x080D8460` | `DE114A`, `DE1A81` |
| `ScenarioAdventureEditorLaunched` | `0x080D8470` | `DE114A`, `DE1A81` |

The names come from the local SporeModder-FX registry and user-supplied EP1 `Properties.txt`; the
GA authority is the independent byte-level presence/use of the IDs in the selected unpacked
binary. This family shows that the Scenario launch/editor/UI surface is wired through shared
property state across GGE and scenario-related paths.

The milestone is static reverse-engineering progress only. It does not claim that a click was
observed, that any property writer is attached to the current host UI, or that the property family
automatically enters `ScenarioMode`.

### Generic property-writer boundary

The SDK/GA address cross-check supplies the generic write surface that the missing producer must
eventually reach:

```text
App::PropertyList::SetProperty         candidate 0x006A2E20
  GA vtable entry at 0x0140885C

App::DirectPropertyList::SetProperty  candidate 0x006A30C0
  GA vtable entry at 0x014088AC
```

These are method/address and vtable corroborations, not a recovered Scenario-specific writer. The
Scenario property IDs do not occur beside a direct call to either setter in the bounded searches;
the actual writer may pass the ID through a generic property service, loaded definition or indirect
callback. The safe status is therefore:

```text
generic setter contract: corroborated
ScenarioGAButtonClicked writer: Unknown
ScenarioGAFirstLaunched writer: Unknown

A direct-caller inspection of the GA `PropertyList::SetProperty` candidate `0x006A2E20` found
several ordinary setter callsites (`BAF169`, `BED976`, `BEDBE6`, `10588B9`, and others), but their
visible key argument is `0x049B48EE`, not either Scenario property ID. This is a bounded negative
result against the inspected direct setter users only; it does not exclude a virtual/generic writer
whose key arrives from a loaded definition or another data path.

### Published property-service pointer

`DF6C10` obtains its property/query service through global `DAT_015FD91C`. The selected GA binary
also contains the small setter stub:

```text
0x0067DED0:
  argument → DAT_015FD91C
```

This confirms a runtime-published service-pointer layer between the Scenario helpers and the
generic property system. The service's semantic owner and the caller that publishes it remain
Unknown; no Scenario-specific write is inferred from the setter stub alone.

## ✔️ Milestone: Scenario properties owner resolved to Preferences

The publisher boundary is now identified in the GA startup/teardown path. The selected binary uses
the wide string `Preferences` at `0x01409938`; the surrounding startup body:

```text
7E6E2D  → 0x006B3FA0 service/resource lookup using "Preferences"
7E6E4D  → 67DED0([object]) → DAT_015FD91C
```

The teardown path later calls:

```text
7E97E0  → 67DED0(0) → clears DAT_015FD91C
```

The static Scenario property path is therefore:

```text
Preferences property object
  → DAT_015FD91C
  → DF6C10 property/query calls
  → ScenarioGAButtonClicked / ScenarioGAFirstLaunched
  → GGE UI state targets
```

This resolves the semantic owner of the published property/query service for the selected GA
build. It still does not identify the exact writer of either boolean, observe a runtime click, or
prove entry into `ScenarioMode`.

## Property-name string search

The selected GA binary contains no matching plaintext strings for:

```text
ScenarioGAButtonClicked
ScenarioGAFirstLaunched
```

This is consistent with the observed numeric-ID path and does not disprove the properties. The
retail writer must be sought through numeric PropertyList/config lifecycle, loaded definitions or
indirect setters rather than name-string xrefs.

The helper `0x005DD790`, used by the nearby Scenario launch/editor paths, is also not the missing
Preferences writer. Its bounded body clears the temporary property/value record words at `+0x10`
and `+0x12`, calls `0x00422E20`, and returns the prepared value object. It does not carry either
Scenario property ID and does not call the GA `PropertyList::SetProperty` candidate `0x006A2E20`.
This is retained as value-record construction/normalization, not a Scenario property mutation.
```

## ✔️ Milestone: direct Preferences writers recovered

The missing Scenario-specific writer is now observed statically in the GA bytes.

### `ScenarioGAButtonClicked`

Inside the `DE99B0` listener body, the guarded `0x070D74E7` branch reaches `DE9C4E` and performs:

```text
lookup Preferences object: DAT_015FD91C
build boolean Property value: 0x005DD790
load Preferences vtable +0x14
call SetProperty(0x080D7DE1, value)
```

The surrounding branch first updates UI target `0x0806A310`, then writes the property through the
same Preferences object. The setter call is visible at the `DE9CC5..DE9CCC` instruction sequence.

### `ScenarioGAFirstLaunched`

The `DF6C10` state-2 continuation performs the same generic setter shape:

```text
build boolean Property value: 0x005DD790
load Preferences vtable +0x14
call SetProperty(0x080D97FE, value)
```

The setter call is visible at `DF6D05..DF6D0C`, after the `0x080D97FE` property query and related
UI target `0x0806AA00` handling.

### Corroborating launch flags

The same `Preferences vtable +0x14` setter shape is used by nearby Scenario editor paths for:

```text
0x080D8460 ScenarioCaptainEditorLaunched
0x080D8470 ScenarioAdventureEditorLaunched
```

This closes the generic writer gap for the observed property family. It does **not** yet prove that
the `Play Scenario` ControlID `0x070D7509` is the producer of the `0x070D74E7` event. What is now
static-confirmed is:

```text
0x070D74E7 listener branch
  → Preferences::SetProperty(ScenarioGAButtonClicked, true)
  → controller state 13
```

Runtime click execution and the final `ScenarioMode` transition remain separate gates.

## ✔️ Milestone: GGE listener tied to UTFWin component activation

The SDK semantic cross-check identifies message type `0x287259F6` as:

```text
UTFWin::kMsgComponentActivated
```

The same SDK vocabulary defines:

```text
kMsgButtonClick  = 0x17
kMsgButtonSelect = 0x18
```

The static GA route is now better described as:

```text
UTFWin component-activation message
  → GGE listener DE99B0
  → component/event ID 0x070D74E7
  → Preferences::SetProperty(ScenarioGAButtonClicked, true)
  → controller state 13
```

This is a major semantic correction: `0x287259F6` is not an unidentified event-object class but a
known UTFWin activation message type. The exact upstream route from `ControlID 0x070D7509` through
Button click/select handling into this activation message is still Unknown, as is runtime execution
of the chain and final ScenarioMode entry.

## Component-activation producer boundary

The selected `.text` contains many consumers that validate UTFWin message type `0x287259F6`, including
the GGE `DE99B0` listener. A bounded raw-form scan found:

```text
push imm32 0x287259F6: 0
obvious mov [object+offset], 0x287259F6 constructor writes: 0
```

This is not a whole-binary semantic absence proof. It does show that the component-activation
message is not constructed by a simple local literal in the inspected consumer paths; the producer
likely uses the generic UTFWin message path, registry or indirect UI callback. `DE99B0` is now
identified as a component-activation consumer, while the exact `ControlID 0x070D7509` → activation
producer remains Unknown.

## `0x070D74E7` belongs to the `DE99B0` listener body

The apparent continuation at `DE9AD0` is not a separate message handler: the preceding `int3` run
places the function entry at `DE99B0`. The exact branch is therefore:

```text
DE99B0 listener body
  ...
  DE9C15: compare event ID with 0x070D74E7
  DE9C4E: guarded state/UI feedback path
  DE9D0C: controller +0x108/+0x104 = 13
```

This strengthens the existing classification of `0x070D74E7` as a listener/event state value. It
also makes the separation from `0x070D7509` sharper: the latter has only the two setup-table
references in `DE48C0` and `DE5900`, while the former is actively branched inside the `DE99B0`
listener. No producer for the `0x070D7509` control has been found.

## SDK cross-check

`refs/Spore-ModAPI/Spore/App/IStateManager.h` puts `SwitchToState` at vtable `+0x28`, matching the
GA call shape in the `0x036C4A10` branch. This is a method-order/contract corroboration; the SDK
address table is not used as a literal GA address map.

`IMessageListener` supplies the semantic `HandleMessage(messageID, payload)` contract, while
`IMessageManager::AddListener` is at SDK vtable `+0x20` and is the slot used by `DEA1B0` when it
registers the GGE listener object (`controller + 8`). The GGE object vtable placement `+0x18` is
kept as a GA-specific observation; it is not substituted for a standalone SDK interface layout.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --message
```
