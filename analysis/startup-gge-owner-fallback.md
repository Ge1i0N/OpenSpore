# GGE owner fallback after an unhandled window message

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime message, owner callback, click or Scenario
execution was observed. Continuation of [startup-gge-message-fallthrough.md](startup-gge-message-fallthrough.md).

## `959240`: two fallback modes

`959240` receives a mode/flag byte and a message/window record. Its bounded branches show two
distinct routes:

### Source-style dispatch (`flag != 0`)

```text
959240(flag != 0)
  → 958950(source/attached procedure dispatch)
  → if handled=false:
      manager +0xC8, when present
      → manager callback interface +0x0C(message, mode=4)
```

The manager callback is a fallback consumer only after the attached procedure walk returns
unhandled. The callback return is not used as a proof of GGE-specific command conversion in this
bounded path.

### Parent-chain dispatch (`flag == 0`)

```text
959240(flag == 0)
  → walk parent/next window chain through window +0x38
  → 958950(parent, message, mode=0)
  → if handled=true:
      manager +0xC8 callback interface +0x0C(message, mode=4)
  → if no handler:
      manager +0xC8 callback interface +0x0C(message, mode=5)
```

This is a generic owner/fallback protocol. The modes `4` and `5` are observed callback arguments,
not recovered ScenarioMode tags.

## Separation from DE99B0

The bounded sequence is therefore:

```text
generic message
  → attached WinProc walk
      → DE99B0
      → unhandled
  → parent/owner fallback through 959240
  → manager +0xC8 callback, if present
```

This fallback does not identify the callback's semantic owner or payload. In particular, no
bounded instruction here joins:

```text
0x070D7509 → 0x070D74E7 → 0x00E1132F → ScenarioMode
```

The known direct `DE99B0` branches remain separate component/state handling. The parent/owner
fallback is a possible upstream boundary for the unresolved Play Scenario route, not proof that it
executes it.

## Additional bounded observations

`959200` contains list/node insertion behavior guarded by window flags and `+0x1C`, while
`959330` recursively checks a child/parent node and may call `958950` before forwarding to the
manager callback. These are window/list lifecycle helpers. They do not expose a direct GGE
Scenario command constant in the inspected prefixes.

## Status

Established statically:

- source versus parent-chain fallback modes;
- manager callback storage at `+0xC8` in the inspected generic owner;
- callback mode arguments `4` and `5`;
- fall-through ordering after an unhandled attached WinProc.

Still Unknown:

- runtime callback population and invocation;
- semantic owner/payload for the Play Scenario control;
- CommandID/ScenarioMode conversion;
- world/stage construction and playable gameplay.
