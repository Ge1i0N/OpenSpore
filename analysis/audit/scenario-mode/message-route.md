# ScenarioMode message route

Static GA evidence, reviewed 2026-10-17. `EF2250` is installed as the handler for message
`0x0212D3E7` by `EF2290`. Its relevant logic is:

```text
payload +0x08 == 0x01654C10 (ScenarioMode)
  and controller +0xCC == 0
    → EF1D20

payload +0x08 == 0x00DBDBA1 (Editors)
  and controller +0xCC != 0
    → EF1380
```

This payload is the mode tag from the GameModeManager message route. The SDK `cScenarioMode` tag
and the GA registration tag agree. The route does not consume ControlID `0x070D7509`; the producer
that would turn the `Play Scenario` UI action into a state/mode request remains Unknown.

The handler returns handled=1 on the observed branches. Static instructions do not establish that
`EF1D20` ran in the original process or that its services/resources were available.

### Same event ID, separate global handler

The selected GA binary also registers the same event ID `0x0212D3E7` through a separate global
controller path:

```text
E81DFA
  → GameModeManager/AddHandler(+0x28)
  → event 0x0212D3E7
  → handler E81C90

E82050
  → corresponding removal/teardown path
```

`E81C90` is not the ScenarioMode handler `EF2250`. Its bounded checks use adjacent mode-state
values `0x01654C00` at payload `+0x08` and `0x01654C08` at payload `+0x10`, then call its own
global-state helpers. It contains neither `0x01654C10` nor `0x070D7509`. Therefore the event ID is a
shared MessageManager channel with multiple consumers; its presence alone cannot join the GGE
`Play Scenario` control to ScenarioMode.

### Additional app-level consumer

The app-level handler at `0x007E7160` is another concrete consumer of the same event channel. Its
bounded dispatch is:

```text
event 0x0212D3E7
  → zero two app/controller float fields at +0xB4/+0xB8
  → handled = 1
```

The same handler has separate branches for `0x00F62ADD`, `0x0255ABF5` and `0xAE1CFE73`; on the
`0x00F62ADD` branch it performs a retry/update path and later forwards `0x00F62ADE`. It does not
compare `0x01654C10`, `0x070D7509` or `0x080D7DE1`. This is an app-lifecycle consumer, not a
ScenarioMode entry route.

### Bounded shared-event producer

The lifecycle method at `0x0054DF60` publishes `0x0212D3E7` through its current service object:

```text
service vtable +0x24(this, 0x0212D3E7)
```

The same method publishes neighboring lifecycle events `0x01EE100A` and `0x02319914` through the
same service interface. The payload is the lifecycle object's context; no literal
`0x01654C10`, `0x070D7509` or `ScenarioGAButtonClicked` is constructed at this producer. Its
bounded role is therefore generic lifecycle/event publication, not the missing GGE Play Scenario
conversion.

### Explicit `kMsgEnterMode` producer

The selected binary contains a concrete mode-enter producer at `ED57A0`. Its message construction
uses the mode-manager enter ID:

```text
ED57A0
  → MessageManager service
  → event/tag 0x074BAF67 (kMsgEnterMode)
  → dynamic mode/state value from the caller context
```

The GGE lifecycle class invokes this producer at several bounded callsites:

```text
ED628A → ED57A0(-1)
ED6429 → ED57A0(-1)
ED672A → ED57A0(2)
ED67F0 → ED57A0(1)
ED6836 → ED57A0(0)
```

Those callsites are lifecycle/state transitions and contain no literal `0x01654C10` or
`0x070D7509`. The producer therefore proves a native enter-mode channel and a GGE-side caller, but
not the semantic conversion `Play Scenario → ScenarioMode`. The mode value remains a dynamic
state/context field, not an inferred ScenarioMode tag.

### Dynamic-value source is bounded for this producer

The producer's dynamic value is the caller argument copied into object field `+0x1C`:

```text
ED57A0:
  argument → EDI
  if EDI != object +0x1C:
    publish kMsgEnterMode
    object +0x1C = EDI
```

Constructor `ED5130` initializes both `object +0x1C` and `object +0x20` to `-1`. A complete direct
caller inventory for `ED57A0` in the selected `.text` gives:

```text
ED628A → -1
ED6429 → -1
ED672A →  2
ED67F0 →  1
ED6836 →  0
```

No bounded direct caller supplies a resource/property-derived value or `0x01654C10`. Teardown
method `ED6910` consumes `+0x20`, falls back to `+0x1C` when it is `-1`, and passes the resulting
value to `ED6620`, whose local lifecycle dispatch is limited to branches `0`, `1` and `2`. This
closes the dynamic-source question for this producer: it is a small numeric lifecycle state
machine, not the missing Play Scenario-to-ScenarioMode tag conversion.

### 🔥 Native SpaceGame → ScenarioMode transition seam

A separate mode-transition coordinator in the `0x00B33xxx` family contains the exact mode-pair
guard:

```text
current mode = 0x01654C05 (Game_Space / SpaceGame)
target mode  = 0x01654C10 (ScenarioMode)
  → B335F0
```

The guard is visible at `B337E6..B337F4`, and the transition continuation invokes `B335F0` at
`B33C94`. `B335F0` then performs the ScenarioMode-specific bootstrap using the tag `0x01654C10`:

```text
B335F0
  → mode/service lookup
  → ScenarioMode tag 0x01654C10
  → mode object/service vtable calls
  → common mode-transition continuation
```

This is the first direct static join between the native `Game_Space` mode and `ScenarioMode`. It
does **not** identify the upstream UI producer: the guard already receives target mode
`0x01654C10`, while object-74 `Play Scenario 0x070D7509` still occurs only in GGE UI setup tables.
The recovered chain is therefore:

```text
Play Scenario producer: Unknown
  → mode target 0x01654C10: still unresolved
  → SpaceGame → ScenarioMode coordinator: recovered
  → B335F0 ScenarioMode bootstrap: recovered
```
