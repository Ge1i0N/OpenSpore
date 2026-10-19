# ScenarioPlayMode player-factory candidate and update phases

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump`/PE-byte inspection only; no Ghidra analysis or native execution in this pass.
No save writer was executed. Static calls do not establish resource completion or playable input.

Continuation of [startup-gge-scenarioplay-initialization.md](startup-gge-scenarioplay-initialization.md).

## Entry is separate from ordinary Update

`EF2EF0` first waits for global loading phase `0x015ACC34 == 4`. Its nonzero enter latch
`ScenarioMode +0xDC` selects one-shot setup and returns after lighting selection. The ordinary
play-side update is a later invocation, with latch clear and strategy `ScenarioMode +0xCC == 2`:

```text
play = ScenarioMode +0x78 pointer
manager byte +0x24 = (play phase +0x90 != 0 && != 1)
F1F960(converted time argument)
EF0D00(converted time argument)
additional mode-side helpers
```

This is a static ordinary-update call site (`EF308C`), not an observed gameplay tick. The manager
byte is written before dispatch; `F1F4E0` can change the local play phase during that dispatch.

## `EF0E00 → B20F40`: player-object request

At play-side entry, `F67D40` simply returns object-manager field `+0x74`. If null, `EF0E00` calls
`B20F40` on the same manager. That helper:

1. rechecks manager `+0x74`;
2. requests identity `0x02C21781` through `B20BF0`;
3. retains/replaces manager `+0x74` with the result;
4. calls returned-object virtual `+0x28(0)`.

`B20BF0` calls global factory/service getter `920320` (global `0x01667B84`) and its virtual
`+0x20(identity, 0x017F243B, 0, 0)`. It then allocates a **list node**, not an avatar, with
`F47240(0x0C, "Simulator", ...)`, stores the returned object in node `+8`, and links it to owner
`+0x10C`. It also assigns object `+0x24` from a global sequence counter, links object `+0x0C/+0x10`
to the owner's `+0x78` list, and calls `B200F0`. Native code subsequently dereferences the returned
object without a comprehensive null-failure exit; a host must not invent success for an Unknown
factory result.

## Concrete registered factory candidate: `Simulator/cPlayer`

The identity-byte scan led to an explicit registration-side association:

```text
B24C3F: key 0x02C21781
B24C47: B475C0(key) → pointer to table value
B24C5B: *value = B1E960

B1E960:
  F47240(size 0x12D8, label "Simulator/cPlayer" at 0x0145DF38, ...)
  null → return null
  non-null → tail-jump C7B930
```

`B475C0` looks up a keyed table and inserts a zero-valued entry on a miss, then returns the value
address. Constructor `C7B930` calls `B185A0`, installs table pointers `0x01472CC8`, `0x01472CB4`
and `0x01472CA4`, and initializes embedded data. `C755F0` independently returns type identity
`0x02C21781`; `C755D0` accepts interface identity `0x02C216ED`.

This proves a **cPlayer-labelled allocation/constructor candidate registered for the requested
identity**. The virtual factory/service `+0x20` implementation and its join to that registration
table have not been resolved in this pass. Successful instantiation, avatar population, world
attachment and input ownership are still Unknown. The nearby `B1E930` allocation is labelled
`Simulator/cMissionAdventure` and is not this `cPlayer` factory.

## Lazy singleton setup is not two separate objects

Later in `EF0E00`, global `0x0167A690` is checked twice. Each absent branch allocates `0x40` bytes
with allocation label `Simulator/SimSingleton`, calls `AE69F0`, and stores the global. Between
checks it calls `AE72D0`. The result of the second check is passed to manager virtual `+0x50`.
These are repeated **lazy obtain checks of one global**, not proof of two distinct allocations.

Global `0x0168D824` similarly has repeated lazy checks, size `0x24`, constructor `F30A60`, and
the same allocation label. A string address supplied to `F47240` is an allocation label, not a
hashed class identity or a world-ready result.

## `F1F960`: exact local phase table

Before phase dispatch, `F1F960` synchronizes a service byte into `play +0xB4` and calls `F1F4E0`.
It then reads `play +0x90`, subtracts one, and uses an unsigned `<= 5` table bound:

| Local play phase | Target from `0x00F1FF60` | Observed bounded work |
|---:|---|---|
| 1 | `F1F9CF` | Requires byte `+0xFC`; subtracts unsigned time from `+0xF8`, updates object `+0xB54/+0x80`, eventually clears `+0xFC` and selected service bits, traverses service objects. |
| 2 | `F1FF53` | Return from phase-specific dispatch. |
| 3 | `F1FB3B` | Service/pause predicates, timers `+0xBC/+0xC0`, possible `F1F290` or phase-6 request; `EF72C0` once after `+0xC0 > 1000` and latch `+0xFD == 0`. |
| 4 | `F1FF53` | Return from phase-specific dispatch. |
| 5 | `F1FBE3` | Requires `+0x94 == 0`; timed service/UI work, resource selection and a state/request descriptor. |
| 6 | `F1FD7A` | Separate `+0x94 == 0` timed path, resource selection and a state/request descriptor. |

Phase 0 or an out-of-range value skips phase-specific dispatch. **All these cases are after
`F1F4E0`**, so a return entry does not mean the entire Update performed no work. Phases 5/6 can
reach `ADDE10` with descriptor hash `0x21851EBE` and set `+0x94 = 1`; this does not establish an
accepted AppState switch. Timer helpers and resource selection are not world construction.

`F1A9C0` is the local phase setter: identical requests return, and a request for phase 4 is rejected
when the current phase is 5 or 6. Otherwise it writes `+0x90`. Phase 4 reaches `809E60`; phases
5/6 have separate cleanup, timer and UI/service paths. Their local `+0x94` latch must not be
confused with global loading phase, ScenarioMode strategy or GGE state numbers.

## Result and next unresolved joins

- **Established:** ordinary-update call site, precise six-phase table, lazy singleton setup,
  object-manager player request, and a registered cPlayer-labelled allocation/constructor candidate.
- **Partially answered:** requested identity versus registered factory. The service `+0x20`
  implementation-to-table join remains unresolved.
- **Unknown:** successful resource population, world/terrain/stage construction, avatar attachment,
  input activation, native gameplay tick and playable readiness.
- Object-74 `Play Scenario 0x070D7509` still has no new producer join to ScenarioMode here.

Next focused candidates are factory/service `+0x20` for identity `0x02C21781` and the ordinary
phase-3 continuation `EF72C0`. These are not authorization to execute the native game/save paths.
