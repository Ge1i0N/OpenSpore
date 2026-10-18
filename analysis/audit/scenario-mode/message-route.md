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
