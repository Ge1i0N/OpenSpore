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

The `00810650` receiver is also scoped: `DE5900` supplies `controller +0x144`, which is the active
`UILayout`; the wrapper reads its `+0x14` layout-object span and forwards to `008102A0` for the
ControlID search. Thus the attached object is the window from the current mounted GGE layout, not
a global owner/service registry entry. This keeps the object-74 attachment fact separate from any
later parent/context binding.

The later parent-registration step is now bounded independently:

```text
active UILayout owner/root + component span
  → 811C20
  → owner IWindow vtable +0xD8 = 00962780
  → resolved child window +0x38 owner/child linkage
```

`00962830` is the symmetric removal path. This is the native layout parent mechanism, not the
ComboBoxDrawable `+0x30/+0x34` setters and not evidence of a Play Scenario producer. The semantic
owner object and its later message callback remain Unknown.

`00962780` invokes the resolved child's **outer** vtable `+0x18` during AddWindow:

```text
WinButton outer vtable 0x01441390 +0x18 = 0x0081D530
GGE attached vtable 0x0147E030 +0x18    = DE99B0
```

If that outer lifecycle call returns false, owner `+0xE0` / `00960190` rolls back the child
registration. This parent lifecycle callback is distinct from the GGE message receiver and is not
itself a Play Scenario fallback.

`00962780` invokes the resolved child's **outer** vtable `+0x18` during AddWindow:

```text
WinButton outer vtable 0x01441390 +0x18 = 0x0081D530
GGE attached vtable 0x0147E030 +0x18    = DE99B0
```

If that outer lifecycle call returns false, owner `+0xE0` / `00960190` rolls back the child
registration. This parent lifecycle callback is therefore distinct from the GGE message receiver
and is not itself a Play Scenario fallback.

The owner acquisition lifecycle is also bounded:

```text
DE5900 → 812250 → 8121C0
  → create/select UI/LayoutObjects (+0x14)
  → copy caller-supplied layout key fields
  → 812100
  → replace owner/root at +0x88 via 8106C0 when needed
  → 811C20 window registration
```

The `8106C0` replacement lookup is an ordered-container lookup, not an opaque global callback
service. It passes the requested three-word layout/resource key into `E5C780`, which walks the
ordered tree rooted at `[container +0x0C]` against the sentinel at `[container +0x10]`. On a match,
`8106C0` converts the tree node through its `+0x14` record link and returns the record's `+0x08`
object; a sentinel/miss returns null. This explains the concrete source of the owner/root object
selected by `00812100`, while leaving the later callback-field population and Play Scenario semantic
binding separate.

`811C70` is the adjacent synchronization path: it calls `811C20`, then invokes the owner callback
at `+0x8C` when present and initializes resource key `0x2FBF2058`. These callbacks belong to the
layout/display lifecycle. They are the next native owner-callback boundary, but no static evidence
assigns them the `Play Scenario` payload or a ScenarioMode request.

The callback's subobject-relative layout is now exact. `811C70` receives the `owner + 0x08`
subobject, normalizes `owner = subobject - 0x08`, and reads:

```text
subobject +0x88 = owner +0x90  → callback context
subobject +0x8C = owner +0x94  → callback function
subobject +0x90 = owner +0x98  → callback argument/context
```

Constructor `810170` initializes the owner/subobject pair and zeros these callback-related fields
(`owner +0x90/+0x94/+0x98`, with the subobject flag at `owner +0x8C`). A candidate writer exists
in the same UI resource helper family at `810130`: when `[this +0x14]` is non-null it writes the
target's `+0x90 = this`, `+0x94 = arg1`, and `+0x98 = arg2`. This is positive setter evidence for
the dynamic callback binding, but its caller/target population is not statically joined to GGE
object 74 or `Play Scenario`.

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

The `DE317A` context is also bounded: it is reached from vtable method `DE31F0` (table
`0x0147DC50 +0x0C`) after a time/update guard, and its target expression is exact:

```text
owner +0x48
  → [owner +0x48] +0xB8
  → DF6820(new_target)
```

The second caller, `DF851E`, sits in the per-entry `DF8300` presentation/update loop and passes a
target produced by its current entry/event path. Neither bounded caller contains a direct
`0x070D7509` or `0x070D74E7` immediate, so the target identity and Play Scenario relation remain
Unknown.

The owner source is now separated from per-entry object creation. `DF9288` creates each per-entry
presentation object and initializes its own fields, including `+0x48 = &object+0x4C`. By contrast,
the known stage-caption activation branch at `DF79FE` writes:

```text
global owner 0x016A11B4 +0x48
  = controller 0x016A1344 +0xFC
```

`DE317A` later consumes that owner field through `+0xB8`. Thus the known forwarded stage-caption
target is a controller-owned resource/service object, not the Play Scenario object 74 or nested
object 87. The Play Scenario target remains a separate unresolved route.

### 👀 Excellent finding: the per-entry forwarded target is a marker effect

The `DF851E` callsite is now semantically bounded by its containing `DF8300` loop. For each eligible
per-star record, the loop:

```text
record +0xAC/+0xAD eligibility
  → choose effect resource
  → effect vtable +0x48(8, transform, 1)
  → DF6820(effect)
  → GGE object +0x34 = effect
```

The transform passed to the effect is built from the record's position (`+0x80/+0x84/+0x88`) and
the shared projection data. `DF6820` then retains that effect and sends it its internal message
`9` with the GGE owner's `+0x30` context. Therefore the `DE99B0` optional forward call through
`+0x34` is an effect/marker bridge in this recovered path, not evidence of a Play Scenario
controller or ScenarioMode launcher. Whether an effect object handles any UI payload remains
Unknown; no Play Scenario-to-effect dispatch is claimed.

### 👀 Excellent finding: one population branch is for stage-caption UI

The bounded `DF79FE` writer supplies one concrete population branch:

```text
message.type = 0x287259F6 (kMsgComponentActivated)
component/event ID = 0x03C4925E
  → owner 0x016A11B4 +0x48 = controller 0x016A1344 +0xFC
  → internal state setter 0x0000000B
```

This proves that the forwarded-target field is populated by a real activation-message branch, but
the recognized event is `0x03C4925E`, not Play Scenario `0x070D7509` and not state-feedback
`0x070D74E7`. Shipped `GGEGameStarUIv4` resources independently identify `0x03C4925E` as the
per-star **stage caption** ControlID in both `PatchData.package` and `Spore_Graphics.package`.
The branch therefore provides a concrete working example of forwarded-target population for
per-star presentation UI, while separating that known path from the unresolved Play Scenario route.

The bounded immediate inventory reinforces that separation:

```text
0x03C4925E stage-caption event:
  DF77C9, DF79B8, DF93C0

0x070D7509 Play Scenario control:
  DE491C, DE5963 only
```

Thus `0x070D7509` does not occur in the known `DF79FE` stage-caption activation branch. Its only
GA immediate occurrences remain the `DE48C0`/`DE5900` control tables; a dynamic or higher-level
producer is still required.

### 👀 Excellent finding: forwarded target is resource/property-backed

The target source is now bounded one layer earlier. In the GA resource/property load path around
`DF1A55`, a lookup with key `0xD047A893` stores its returned value into the current object field
`+0xB8`:

```text
resource/property key 0xD047A893
  → current GGE-related object +0xB8
```

That value can then be reached by the `DE317A` expression:

```text
owner +0x48
  → object +0xB8
  → DF6820
  → DE99B0 forward target +0x34
```

The semantic registry name for `0xD047A893` is not established. This is therefore a positive
data-driven registration boundary, not permission to name the target as a Play Scenario handler.

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

### 🔥 Nuclear finding: sibling Scenario launch consumer at `DF43A0`

A separate GA-era `kMsgComponentActivated` consumer is recovered at `DF43A0`:

```text
message.type = 0x287259F6
component ID = 0x053F0707
  → Preferences::SetProperty(0x080EAA93, value)
  → UI target 0x0809B720 visibility/state update
```

The shipped secondary property registry labels `0x080EAA93` as:

```text
ScenarioSpaceGameLaunched
```

This is a sibling Scenario launch surface to the already bounded GA route:

```text
DE99B0:
  0x070D74E7
    → ScenarioGAButtonClicked 0x080D7DE1
    → GGE state 13

DF43A0:
  0x053F0707
    → ScenarioSpaceGameLaunched 0x080EAA93
    → UI target 0x0809B720
```

The two consumers share the Preferences service/property-writing shape, but `DF43A0` does not
compare `0x070D7509`; it is not promoted to the Play Scenario producer. Its value is architectural
evidence that Scenario launch surfaces use sibling component IDs and property-backed handlers,
which narrows the missing Play Scenario route to another dynamic component/handler binding.

The handler's class boundary is concrete rather than inferred from code proximity:

```text
DF3A40
  → allocates a 0x630-byte object for type/resource key 0x013F0990
  → DF0BD0 initializes it
  → vtable 0x0147E61C
  → vtable +0x18 = DF43A0
```

Thus `DF43A0` is a method of a distinct GGE-era controller object, not another branch of
`DE99B0`'s vtable `0x0147E030`. The shared message format and Preferences service are real
architectural commonality; the object identity and handler semantics remain separate.

Its enclosing flow is also bounded at the vtable level. Constructor `DEEB80` installs outer vtable
`0x0147E568`; that table has `DF3A40` at slot `+0x2C`, and the nearby `.rdata` text is the UTF-16
label `New Game Flow`. `DF3A40` is the method that allocates the nested `0x013F0990`/`0x630`-byte
controller described above. This places the `ScenarioSpaceGameLaunched` consumer in the New Game
Flow family, not in the GGE `DE99B0` object-74 listener. The nearby flow identity arrays explain
why its activation IDs are easy to confuse with GGE controls, but do not establish a Play Scenario
handoff.

The sibling ID is also present in a separate GGE-side component identity registration array:

```text
DF38A7..DF38DA:
  0x053F06E1
  0x053F06EE
  0x053F06F5
  0x053F06FC
  0x053F0707
  ...
```

The array is inserted into a component/identity container; this is positive evidence that
`0x053F0707` belongs to the same GA-era registered UI/component family. It does not identify the
array as the serialized EP1 object-74 layout entry, and it does not join the sibling handler to
`Play Scenario 0x070D7509`.

### Other GGE-adjacent activation consumers

Two nearby procedures are concrete activation consumers but remain separate from the `DF43A0`
Scenario-space property route:

```text
DF5540
  vtable 0x0147E9C0 + 0x18
  message.type = 0x287259F6
  component ID = 0x06383360
    → service/mode call with key 0x06382DB7

DF5D60
  vtable 0x0147E920 + 0x18
  message.type = 0x287259F6
  component IDs = 0x05D2BDF8 or 0x05D66F78
    → service/mode calls with keys 0x05D663CD or 0x05D6714B
```

`DF5540` is installed by the object constructor at `DF5C50`; `DF5D60` is installed by the
constructor at `DF5D00`. Their bounded bodies contain none of:

```text
0x070D7509 Play Scenario
0x070D74E7 Galactic Adventures
0x080D7DE1 ScenarioGAButtonClicked
0x053F0707 ScenarioSpaceGameLaunched producer ID
```

They are therefore classified as GGE-adjacent service/UI consumers, not as hidden Play Scenario
handlers. This closes another local consumer slice without promoting unrelated component IDs to
the missing object-74 route.

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

The generic GA activable-window producer is recovered at `0x009640B0`:

```text
outer WinButton +0x84 (CommandID)
  → fallback outer +0x80 (ControlID)
  → message.type = 0x287259F6
  → IWindow::SendMsg(+0x114)
```

The SendMsg wrapper has a separate delivery gate: `00960250` returns false when embedded IWindow
`+0x30` is null. For a WinButton this is outer `+0x34`, which is zeroed by base construction at
`00962950`. `00961F10` AddWinProc only updates the procedure list at `+0x60` and sends attachment
records. The parent-registration path can later populate that gate: `00962780` calls `0095AD30`
when owner `+0x30` is present, and `0095AD30` writes child outer `+0x34 = owner +0x30`. Thus the
static binding chain is now resolved up to the parent owner's own `+0x30` source; runtime execution
and the semantic Play Scenario handler remain separate claims.

The direct-call inventory for `0095AD30` is also bounded:

```text
009627E5 → AddWindow path
0095AE04 → internal child-list propagation
0095B15A → generic window-list insertion
```

No separate GGE `DE/DF` caller was found. This is a scoped negative result: the top-level owner
`+0x30` source is generic window hierarchy/resource binding rather than a recovered GGE-specific
literal producer.

The adjacent global-root path is separately identified: `E02AC0` looks up root window identity
`0xFFFFFFFF` through `00810650` and attaches `Simulator/cSPUIGlobalUIRootWinProc`. This is a
generic top-level UI root procedure, distinct from `DE5900`'s GGE procedure attachment and from
`DE99B0`'s component-ID branches. It is a plausible higher window-hierarchy source, but no static
GGE-specific join from this root procedure to `Play Scenario` has been recovered.

The attached root procedure is a concrete parent-side receiver:

```text
Simulator/cSPUIGlobalUIRootWinProc
  → vtable 0x0147F118 +0x18
  → E02ED0
  → kMsgComponentActivated dispatch
```

The bounded `E02ED0` body contains no direct immediate for `0x070D7509`, `0x070D74E7` or
`ScenarioGAButtonClicked 0x080D7DE1`. Its comparison of `0x01654C10` occurs in a separate current-
mode branch, not as a Play Scenario payload consumer. Thus the parent/root fallback is now located,
but it does not close the GGE-specific producer.

### Root receiver dispatch classification

`E02ED0` is a generic root/global UI state machine rather than a hidden GGE Play Scenario handler.
Its bounded `kMsgComponentActivated` branch compares a separate root-ID set, including values such
as `0x0447060A`, `0x010F366E`, `0xD305CA84`, `0x010ED852`, `0x010F366D`, `0x062D88E9` and
root-adjacent paths, then updates its own state field at `+0x24` or calls root-side helpers. No GGE
EP1 Play Scenario/GA-tab ID occurs in that branch.

The later comparison with `0x01654C10` is reached after a mode-manager getter and checks the current
mode before selecting root state values (`2`, `4`, `10`, etc.). It is a mode/state synchronization
branch, not a conversion of `0x070D7509` into a ScenarioMode request. The safe route remains:

```text
DE99B0 unhandled
  → global root E02ED0
  → generic root/state handling
  → no Play Scenario ID match
```

Its later branches construct `kMsgButtonClick = 0x17` and `kMsgButtonSelect = 0x18`. The
GGE-specific unresolved part is no longer generic message construction; it is the higher-level
relationship between the Play Scenario control payload `0x070D7509` and the separate
`Galactic Adventures` control/event `0x070D74E7`.

The receiver path is also bounded one layer further:

```text
009640B0
  → IWindow +0x114 / 00960250
  → 0095A010
  → 00959240
  → 00958950 → 00958880
  → WinProc vtable +0x18
```

`IWindow::AddWinProc(+0x104)` is implemented at `00961F10`: it stores procedures in the native
window list at `+0x60`, orders them by vtable `+0x10`, and sends attachment-time records (`0x1000`
and conditionally `0x11`). The GGE interface priority method is `0x00E310C0` and returns `0`.
Thus the shared GGE attachment is a real list member, while its unhandled message result still
leaves the later list/owner route open.

The attached GGE interface at `0x0147E030 +0x18` reaches `DE99B0`, but `DE99B0` has no bounded
`0x17`/`0x18` branch. The generic dispatcher retains window `+0xC8`/owner fallback paths, so the
missing `Play Scenario` producer must still be sought among those dynamic receivers or another
attached procedure. This does not establish runtime delivery or a ScenarioMode post.

The return behavior narrows the candidate boundary: `DE99B0` falls through with `handled = false`
for unrecognized message types/component IDs, including the bounded Play Scenario identities. The
`00958880` WinProc walk continues after such a result, so the safe native route model is:

```text
DE99B0
  → unhandled Button/Play Scenario message
  → next attached WinProc
  → window +0xC8 or owner fallback
```

No later handler has been assigned a Play Scenario meaning; `Unknown` is retained.

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

This strengthens the semantic separation: `0x070D74E7` is the shipped `Galactic Adventures`
control identity actively branched inside `DE99B0`, while `0x070D7509` is the separate captioned
`Play Scenario` control found in `DE48C0`/`DE5900` setup tables. No native parent/controller route
from the latter to the former has been found.

### 👀 Excellent finding: static Galactic Adventures feedback loop

Two independent GA paths converge on the same semantic control:

```text
DE99B0:
  component ID 0x070D74E7
    → Preferences::SetProperty(ScenarioGAButtonClicked, true)
    → controller state 13

DE48C0:
  controller state 13
    → UI target/control 0x070D74E7
```

The shipped locale caption identifies that control as `Galactic Adventures`. This forms a coherent
static feedback loop for the Galactic Adventures tab/state surface. It is not runtime execution and
does not connect `Play Scenario` object 74 (`0x070D7509`) to this loop.

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
