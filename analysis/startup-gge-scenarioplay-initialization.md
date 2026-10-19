# ScenarioPlayMode initialization and act/goal update boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native play update, resource completion, world/stage creation or
gameplay tick was observed. Continuation of [startup-gge-scenariomode-preplay-update.md](startup-gge-scenariomode-preplay-update.md).

## `EF0E00 → F1F060`

The play-side entry calls `F1F060` on ScenarioPlayMode storage `+0x78`. The bounded initialization
does the following:

```text
F1F060
  → F1DF00 cleanup/reset
  → play byte +0xB4 = 0
  → obtain service pointer through EED7E0 → play +0xC4
  → if play phase +0x90 == 2:
      clear +0x90
      F1EE70(0, 0)
  → inspect ScenarioMode launch context +0xD0
```

For launch context `1` or `2`, the branch creates/obtains service-side state, sets a service bit
`+0xB58 |= 0x800`, sets a manager bit selected by identity `0x06669387`, clears a selected timer
at `+0x80`, and walks eligible objects with virtual `+0x10/+0x2C/+0x30` calls. For other context
values it calls `F1C390` and may advance local phase `+0x90` to `3` before a separate continuation.

The tail selects an entry from global table `0x016C82BC..0x016C82C0` and writes play phase
`+0x90 = 1`. This is phase transition/service setup, not proof of a loaded stage.

## `F1EE70`: current act setup

`F1EE70` stores the requested act/index at play-mode `+0xB8`, calls local setup helpers, resolves
the act object through `F3BD00`, and replaces the current play resource when identity differs:

```text
play +0xF4 = act-object +0x80
play +0xF0 = A20670(act-object +0x80)
```

It then reads position data, performs setup calls with native IDs `0x12D2A4D4` and `0xBA134192`,
and calls `F0E5A0/F11380`. The semantic resource names and completion results remain Unknown.

## `F1C390`: act/goal state machine

The scan runs only when play phase `+0x90 == 3`. It iterates act records using stride `0x1AC`.
Handled records can:

- write play state `+0xB0 = 4`;
- build/request a `ScenarioMode`-name buffer through `6B5440/6B5550`;
- call `F1A9C0(6)`;
- map record states `10/11` to statuses `3/2` and call `F1B610`;
- increment progress counters.

At the final native act count it calls `F1A9C0(5)`; otherwise it requests a two-phase transition,
sets phase `+0x90 = 3`, advances the index and calls `F1EE70` again.

These are concrete act/goal and service state transitions. They do not prove terrain, stage,
avatar, input or gameplay readiness.

## Boundary result

```text
ScenarioMode +0xCC == 2
  → EF0E00
  → F1F060 service/phase setup
  → F1EE70 current act/resource context
  → F1C390 goal/act scan
  → local play phases and requests
```

The next world-related boundary is the successful result/consumer of these service and object
paths, not the existence of the ScenarioPlayMode methods themselves.
