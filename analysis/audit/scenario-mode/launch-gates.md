# Scenario play launch: phase gates, presentation latch and countdown

Static selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native process, resource query or save-writing path was executed.

This continues [selection](selection-flow.md) and [launch-record/state routing](launch-state.md).
[REA evidence](launch-gates-evidence.json) retains the decompilation and scoped xref result;
instruction guards cover uncarved phase callbacks and the exact countdown branch.

## Launch action starts a waiting phase, not a state switch

`DFAE20` calls `DF9AA0` and `DFAAF0(0)` first, then sets:

```text
controller +0x20 = 10  (ReadyToLaunchScenarioForPlay)
controller +0x80 = 1   (launch waiting branch)
controller +0x28 = 10  (countdown, not yet consumed while +0x80 != 0)
```

It hides the GGE layout via `810630(0)` and tail-jumps to `DF9BB0`. `DF9BB0` obtains service
`B3D460`, calls `B7E010` on it, handles the key according to local state (10 uses `+0x8C`,
9 uses `+0xB0`), resets subphase `+0x78 = 0` and restarts the timer at controller `+0x60`.
Finally it calls `DD9130 → DDBA70` with the key at controller `+0x8C`.

No outer ScenarioMode state request occurs in `DFAE20` or `DF9BB0`.

## DFC210 separates waiting, presentation and completion

After its neutral/dirty-UI work, `DFC210` returns without the launch gate when local state is 11
(`DoingModeSelectZoomIn`). Otherwise, the `+0x80` branch chooses waiting versus countdown.

### While waiting (+0x80 != 0)

| Subphase `+0x78` | Condition | Observed action |
|---:|---|---|
| 0 | signed 64-bit timer result from `93A310`, converted/multiplied by timer `+0x14`, is greater than float `0.5` | `DFA760(1)`; write phase 1 |
| 1 | service `B3D460` list at `+0x20` has its first pointer equal to its own sentinel; `401010` virtual `+0x28` returns signed `<= 0`; `EC42D0` is false | write phase 2 |
| 2 | `DD9140()` is nonzero | clear `+0x80`, reset countdown `+0x28 = 10`, release optional `+0x84`, prepare record keys and loading presentation |

These are successive **independent** checks in one Update, not an `else-if` ladder: if all
conditions permit, phase 0 can advance through phase 1 and phase 2 in the same call. `DFA760(1)`
requests effect key `0x139F4A74` from GGE service `root +0x15C`; this does not itself make phase 2
ready. No missing service response is replaced with success in this audit.

`EC42D0` is an exact predicate on its service receiver: `receiver +0x1C == 1`. The semantic
meaning of all values of that service field is not inferred from this single predicate.

The final waiting action copies selected keys into the shared record and calls
`B3D510 → E3E350(1,0x01654C10,1)`. It **returns** after this presentation request; it does not
fall through to decrement the countdown or call `DFBD40` in the same Update. `DFBD40` later resets
and rebuilds the record again, so this preparation is not the final internal-play-mode selection.

### After waiting (+0x80 == 0)

```text
if countdown +0x28 is nonzero (unsigned comparison):
    decrement by 1
    return from Update
if local state is 9 or 10:
    DFBD40()
else:
    separate DE4690 / DE6FC0 path
```

Consequently countdown `1 → 0` does **not** invoke completion in that Update. Starting at 10,
there are ten decrement-and-return calls, then a further call that sees zero and can invoke
`DFBD40`, provided the earlier guards still allow the countdown branch. These are Update counts,
not seconds, refresh-rate-independent time or proof the active-controller scheduler ran.

`DFBD40` writes the play/edit launch record, calls the save-writing `DE4800` path, and requests
state `0x21851EBE`. It is not safe to execute this native path as a read-only probe.

## DD9140 latch is not universal resource readiness

`DD9140` is simply `return byte[0x016A0B04]`. `DDBA70` clears that global before building a
presentation/transform sequence. If its service `+0x14` lookup returns null on the inspected
branch, it calls `DE7470` and writes the latch to 1. Otherwise it builds an object with phase
vector pointer `+0xF8 = 0x016A0C28` and starts its first phase.

`DDD150` initializes that phase vector with stage objects including `0x015A2DB0`.
That stage object's table is `0x0147D1F0`:

```text
stage virtual +0x00 → DD96F0 (reset per-object timer)
stage virtual +0x04 → DD9740 (timer-dependent latch writer)
```

`DD9740` compares the object's elapsed timer product against `1.0`, writes global latch 1 only
when the elapsed value is greater, and returns 1. Equal or unordered comparisons skip the write.
`DDB070` runs phase `+4`, advances the phase index when that method returns false, and calls the
next phase's `+0` initializer. `DDC670` updates these objects and removes finished ones via
`DDB5A0`. This explains the separate scheduling dependency behind the latch, without claiming
that every presentation phase, asset load, task or simulator subsystem completed.

The provider's exact-address xref result for this global lists `DD9140`, `DDBA87`, `DDBDC7` but
misses the raw `A2` writer at `DD97E6`; the latter is uncarved and independently pinned by bytes.
Do not turn that partial xref list into a whole-image writer-absence claim.

## What this permits, and what remains Unknown

A reconstruction can represent each input/outcome separately: timer threshold, service list,
signed service count, EC predicate, presentation latch, countdown and local mode. Missing/Unknown
observations are not interchangeable with an empty list or zero count. The static route does not
establish resources mounted, a ScenarioPlanet created, queued state commands executed or gameplay.

No engine/host-preview readiness policy was changed in this pass. The checks in
`tools/run_scenario_mode_route_verify.bat` pin instructions and interpret only the small countdown
tail using a restricted instruction subset; they never execute the original PE or its services.
`tools/run_scenario_launch_gate_verify.bat` additionally runs controlled interpreter regression
tests (off-by-one, unsigned counter, mode split and rejection of unsupported instructions/targets).

Key REA records (all in the bundle):

- `ev_e2d54d8bcb25b9234f1b0748f0e06599f52797f7beaf1542055cb1515aa17538`:
  launch preparation and Update guards.
- `ev_e7e3e5a4c561e915db01e0ce414e9a1ce0c1af087a9a52ebe1b00a44ee48ed03`:
  latch reset/alternative write, getters and service predicate.
- `ev_1c3ae083d65719385e92fdd7b542e4ccc00171f7d59f2380e04d0908dd6339d2`:
  presentation update/removal and per-object storage.
- `ev_9d5bcb6517ba61c3f425b26ff9306f0a31dfef0c356763618ea6570c66ffedee`:
  phase runner `DDB070`.
- `ev_0b07f0ab7061b251319081a3b53b05f58660561bee584927e2588acb04346d2d`:
  scoped xrefs, not a complete raw writer scan.
- `ev_d5b960644e8c513762a5c877d9b3bc7a4cd8816827e0d7610e76e27408e65f95`:
  phase-vector initialization and the explicit uncarved `DD9740` error.
