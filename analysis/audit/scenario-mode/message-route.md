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
