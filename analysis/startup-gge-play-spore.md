# GGE internal state 10 → Play Spore state 2 and catalogue admission

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no original runtime, save writer, content manager or UI callback
was executed. Selected instructions are pinned by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
Continuation of [startup-gge-internal.md](startup-gge-internal.md).

## 1. Intro requests internal state 10, not Play Spore directly

`DE9550` checks controller `+0x10C`; if it is not `10`, it calls `DE8680(10,1)`.
`DE8680` has two distinct paths:

- with its second argument true, current internal values `0/1/8/9` are mapped to return value
  `2`, appended to the return stack at controller `+0x110/+0x114`, and requested state `+0x10C`
  is written to `10`;
- with the second argument false, it only writes requested state `+0x10C`.

The return stack and requested field are not the outer AppState ID and do not prove that handler
key `2` was entered. The later handler-selection code uses controller phase/current fields and
invokes the internal handler lifecycle separately.

## 2. Play Spore admission gate

`DF9740` is the Play Spore handler's `+0x38` method. Its exact first branch is:

```text
E00610()                         auxiliary/handler update
controller = [016A1344]
if controller+0x108 != 2:
    return

UILayout lookup on controller+0x144:
  ControlID 0x0615A5FF
  SetFlag/visibility operation (1, 1) if found

GlobalUI::show(1)
DF95A0()                         rebuild game-record/UI data
tail-jump DF6C10()               per-record/effect continuation
```

The branch does not test a save-list count, a resource-manager success flag, a camera, or a
Scenario readiness value. It is a native **state-2 admission gate** followed by a catalogue
rebuild request. `DF95A0` may return with missing services; the static edge remains a call, not
proof that records were populated.

## 3. What DF6C10 consumes immediately

The bounded prefix at `DF6C10` starts by querying resource/event records:

```text
0x080D7DE1 → inspect returned record word +0x12 == 1
  → 41E8E0 supplies a Boolean
  → UI target 0x0806A310 → control flag operation

0x080D97FE → inspect returned record word +0x12 == 1
  → optional UI target 0x0806AA00 → control flag operation
  → construct/forward an event payload
```

It then initializes an elapsed-time sample using `RDTSC` or `QueryPerformanceCounter`, depending
on the manager's timing mode. The prefix contains no `0x070D7509`, no `0x00E1132F`, and no
ScenarioMode tag `0x01654C10`. Therefore this continuation is not evidence that the GGE
`Play Scenario` control has been converted into ScenarioMode.

## 4. Per-record projection is downstream of the rebuild

The separate Play Spore `+0x68 = DF6010` method iterates records and reads position at record
`+0x80`. It obtains a viewer through GameModeManager virtual `+0x58`, calls `DE3C20`, and
forwards projected values to the per-record owner at record `+0xB8` through `DF6110`.

This ordering matters:

```text
state-2 admission
  → DF95A0 catalogue rebuild
  → DF6C10 effects/event continuation
  → later handler update / DF6010 projection
```

The projected UI path therefore cannot be used as proof that the catalogue rebuild succeeded,
that a viewer was produced, or that a playable world exists. Existing evidence keeps those
provider/runtime results Unknown.

## Current boundary

The internal path is now statically bounded through the first Play Spore catalogue handoff:

```text
Intro post-title gate
  → DE9550
  → DE8680(10,1)
  → internal handler transition
  → Play Spore DF9740
  → controller +108 == 2 gate
  → GlobalUI/control 0615A5FF
  → DF95A0
  → DF6C10 prefix
  → later DF6010 record projection
```

The exact source counts, saved-prefix/candidate-suffix placement, per-record UI-data creation and
marker/effect pass are documented in [startup-gge-catalogue-rebuild.md](startup-gge-catalogue-rebuild.md).

Still Unknown: the runtime conditions that make controller `+0x108 == 2`, successful internal
handler entry, MoviePlayer/DBCheck completion, live game records, viewer/camera production,
native UI attachment, Scenario launch delivery and playable gameplay tick.
