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
