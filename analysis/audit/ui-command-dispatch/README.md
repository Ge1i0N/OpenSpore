# UTFWin ControlID/CommandID dispatch boundary

Reviewed **2026-10-18**. Static GA and shipped-SPUI evidence; no original UI process execution.

## What is known

The shipped E3LaunchScreen click fixture has:

```text
ControlID = 0x022D9E01
CommandID = 0x00E1132F
```

The host test reaches the expected `CreatureEditor` state through an explicitly bounded
OpenSpore reconstruction. It does not claim to have recovered the original command dispatcher.

SDK contracts relevant to the missing boundary are:

```text
UTFWin::IWindow::GetControlID  +0x1C
UTFWin::IWindow::GetCommandID  +0x20
UTFWin::IWindow::SetCommandID  +0x54
UTFWin::IWinProc::HandleUIMessage +0x18
```

These explain the serialized data shape and the expected native route, but do not identify the GA
handler address.

## Bounded negative result

`find_imm.py 0x00E1132F` finds **zero** literal immediate byte matches in the selected GA `.text`.
This is expected if the command value is read from an `IWindow` object or SPUI data rather than
compared as an immediate. The nearby seeded App message dispatcher `0x007E7160` handles other
message IDs (`0x0255ABF5`, `0x00F62ADD`, `0x0212D3E7`, `0xAE1CFE73`) and does not establish
CommandID dispatch.

The current OpenSpore `uipresent.cpp` bridge is therefore deliberately bounded:

```text
visible E3LaunchScreen hit
  → ControlID == known state ID
  → CommandID == 0x00E1132F
  → host StateManager request
```

It is not used for GGE controls, and it is not generalized to `Play Scenario`. The original
WinProc/button capture/CommandID producer remains Unknown.

## GA Window message forwarding seed

To narrow the external `button-click=0x17` / `button-select=0x18` leads, a local objdump pass checked
the already relevant GA Window boundary:

```text
0x00960250:
  this + 0x30 == null -> return false
  otherwise forward (this - 4, caller record) to 0x0095A010

0x0095A010:
  copy seven dwords from the supplied record into a bounded local message shape
  invoke the receiver callback/vtable path (one observed literal callback argument is 3)
  normalize/forward through 0x00959240
```

The inspected `0x00960250` body contains no comparison against `0x17` or `0x18`. This is useful
negative/route evidence, not proof that those values are absent from GA: the event type may be
written into the record by an upstream producer or handled in a callback reached indirectly.
No `ControlID 0x070D7509`, `CommandID 0x00E1132F`, or ScenarioMode tag `0x01654C10` was promoted
from this forwarding boundary.

### Direct-call inventory for the inspected seam

The local `.text` scan for direct `E8 rel32` calls found:

```text
0x0095A010 callers: 1
  0x00960262

0x00959240 callers: 2
  0x0095964B
  0x0095A081
```

Thus the inspected route has one direct GA entry from `0x00960250` into `0x0095A010`, while
`0x00959240` is a lower-level record/list forwarding helper used by two bodies. Nearby inspected
dispatch code contains callback values `3`, `4` and `5` and record branches for `0x0D`, `0x14` and
`0x16`; no `0x17`/`0x18` comparison was observed in these bounded bodies. This narrows the search but
is not a whole-binary absence proof: the button event may be written into the record upstream or
handled by an indirect callback.

## Immediate-search limitation

The helper `python tools/find_imm.py 0x17 0x18` reports:

```text
imm32 0x17: 107 raw matches in .text
imm32 0x18: 1068 raw matches in .text
```

These are not event counts. The helper intentionally reports raw four-byte patterns, and many hits
are ordinary bitset/table arithmetic (`value >> 5`, `value % 32`) or coincidental bytes. After
disassembly filtering, the specific forwarding body `0x0095A0A0..0x0095A500` still has no relevant
`0x17`/`0x18` event comparison or push. The whole-binary candidate search therefore remains
**inconclusive**, while the bounded Window forwarding result remains negative for that body.

## Reproduction

```powershell
python tools/find_imm.py 0x00e1132f --ctx 15
```

SDK sources: `Spore/UTFWin/IWindow.h`, `Spore/UTFWin/IWinProc.h` and `Spore/UTFWin/Message.h`.
