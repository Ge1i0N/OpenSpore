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

### 👀 Exact WinProc receiver chain

The bounded call chain from the producer to an attached interactive procedure is now explicit:

```text
009640B0
  → IWindow vtable +0x114
  → 00960250
  → 0095A010 (message snapshot/normalization)
  → 00959240
  → 00958950
  → 00958880 (WinProc-list walk)
  → receiver vtable +0x18
```

The GGE attached interface is a concrete receiver in this family:

```text
GGE interface vtable 0x0147E030 +0x18 = DE99B0
```

### Native `AddWinProc` list ownership

The `IWindow::AddWinProc(+0x104)` body is recovered at `0x00961F10`. It inserts the supplied
procedure into the window's native procedure list at `window +0x60`, ordering entries by the
procedure's vtable `+0x10` priority. The GGE interface uses:

```text
0x0147E030 +0x10 = 0x00E310C0
0x00E310C0 → priority 0
```

After insertion, `00961F10` sends the new procedure initialization records with message types
`0x1000` and, when the window has an owner/context, `0x11`. This is a native attachment/list-order
fact, not runtime click evidence; it explains why a false return from `DE99B0` can expose later
procedures in the same window chain.

### Runtime owner/context binding remains a later seam

The generic WinButton/base construction path at `0x00962950` initializes the relevant base fields
as empty:

```text
object +0x30 = 0
object +0x34 = 0
object +0x38 = 0
```

The WinButton constructor `0x00966EB0` then installs the outer/embedded vtables and does not supply
a serialized Play Scenario owner. A related GA UI interface table at `0x01441868` contains later
setter bodies:

```text
0x00967980 → writes interface +0x30, then invokes base callback +0x8C
0x00967DA0 → writes interface +0x34, then invokes base callback +0x8C
```

The exact interface-to-window instance join and semantic owner are still Unknown. This is a useful
runtime-binding seam, not evidence that either setter launches ScenarioMode or handles
`0x070D7509`.

Therefore `DE99B0` can be reached by the common window message path, but its bounded dispatch
body only recognizes the component-activation message type and its internal branches; no
`0x17`/`0x18` comparison is present there. `00959240` also retains fallback paths through the
window's `+0xC8` callback/owner chain. The unresolved Play Scenario route is consequently narrowed
to that parent/owner or another attached WinProc path, rather than being attributed to the known
`DE99B0` component-ID branch.

### `DE99B0` is a fall-through receiver for Button/Play Scenario payloads

The bounded `DE99B0` switch returns unhandled for message types/component IDs outside its explicit
branches. In particular, no `0x17`/`0x18` branch and no direct `0x070D7509` branch is present.
The `00958880` WinProc walk tests that result and continues to the next procedure when it is not
handled:

```text
Play Scenario WinButton
  → generic component/button message
  → attached GGE DE99B0
  → handled = false for bounded click/Play Scenario cases
  → next WinProc or window +0xC8/owner fallback
```

This is static route evidence only. It removes the assumption that `DE99B0` must consume every
message attached by `DE5900`; the later producer/owner callback remains Unknown.

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

## ✔️ Milestone: GA generic Button dispatch recovered

The selected GA binary contains the generic UTFWin message dispatcher at `0x00950E50`, matching
the SDK semantic `InteractiveWinProc::HandleUIMessage` contract. Its bounded dispatch shape is:

```text
message.type = payload +0x08
→ jump table at 0x0095110C
→ event-specific InteractiveWinProc callback
```

The table entries for the two Button events are exact:

```text
message type 0x17 (kMsgButtonClick)
  → 0x00950FD0
  → receiver vtable +0x68 (OnButtonClick)

message type 0x18 (kMsgButtonSelect)
  → 0x00950FE7
  → receiver vtable +0x6C (OnButtonSelect)
```

This closes the **generic** GA Button dispatch boundary that was previously Unknown. It does not
yet prove that the GGE `Play Scenario` window has an attached WinProc at these slots, nor that its
callback writes `ScenarioGAButtonClicked` or emits component-activation ID `0x070D74E7`. Those
GGE-specific attachment/producer edges remain separate gates.

## ✔️ Milestone: GA component-activation producer recovered

The selected GA binary also contains the generic activable-window producer at `0x009640B0`. Its
bounded body is an actual message construction/send path, not merely a consumer comparison:

```text
window +0x84, falling back to window +0x80
  → UTFWin message type 0x287259F6 (kMsgComponentActivated)
  → IWindow::SendMsg vtable +0x114
```

The same body constructs the ordinary Button messages on its later branches:

```text
state != 2/3 → message type 0x17 (kMsgButtonClick)
state == 2/3 → message type 0x18 (kMsgButtonSelect)
```

The direct GA call inventory is:

```text
0x0081CF31 → 0x009640B0
0x0081CA4E → 0x00964370
0x0081CC90 → 0x00964370
```

This closes the generic producer half of the route:

```text
activable-window/button handling
  → kMsgComponentActivated / kMsgButtonClick / kMsgButtonSelect
  → InteractiveWinProc::HandleUIMessage
```

The remaining GGE-specific gap is now explicitly the dynamic payload binding: `0x009640B0` reads
the component ID from object fields `+0x80/+0x84`, while the Play Scenario SPUI identity
`0x070D7509` is serialized and attached dynamically by `DE5900`. No static immediate joins those
fields to `0x070D74E7`, so the final `0x070D7509 → 0x070D74E7` conversion and runtime click remain
Unknown.

### Native vtable-cluster cross-check

The producer and its adjacent lifecycle methods are also present in several GA vtable tables. This
is a structural cluster, not yet a proof that every table belongs to one C++ class:

```text
0x0141941C → 0x009640B0  generic activable-window message producer
0x01419654 → 0x0081CF20  GGE-side activation/update entry
0x01419600 → 0x00964370  adjacent mouse/button activation path
0x014198F4 → 0x009640B0  second compatible interface table
```

The broader tables additionally contain the adjacent lifecycle entries `0x0081C9C0`, `0x0081CF20`
and `0x00964370`. This is a static vtable join only. It does not identify the constructor input
that fills object fields `+0x80/+0x84`, and it does not prove that the `WinButton` object with
ControlID `0x070D7509` receives component ID `0x070D74E7` at runtime.

## ✔️ Milestone: WinButton activation payload source resolved

The GA object layout and embedded-interface relationship are now concrete:

```text
base activable-window constructor 0x00962950
  → object +0x80 = 0
  → object +0x84 = 0

WinButton constructor 0x00966EB0
  → object +0x00 = vtable 0x01441390
  → object +0x04 = embedded IWindow vtable 0x01441270
```

The embedded GA `IWindow` vtable has the identity setter slots:

```text
0x01441270 +0x50 → 0x0095F980 = SetControlID
0x01441270 +0x54 → 0x0095F990 = SetCommandID
```

The bodies are exact:

```text
SetControlID:
  [IWindow +0x7C] = parameter
  => [outer WinButton +0x80] = ControlID

SetCommandID:
  [IWindow +0x80] = parameter
  => [outer WinButton +0x84] = CommandID
```

The derived WinButton vtable contains the producer:

```text
0x01441390 +0x94 → 0x009640B0
```

`0x009640B0` chooses the activation payload with this precedence:

```text
outer +0x84 != 0 ? CommandID : outer +0x80 (ControlID)
→ kMsgComponentActivated
```

For the explicit EP1 `GGEZoomPanelV2` object 74:

```text
ControlID  = 0x070D7509
CommandID  = 0
→ generic activation payload = 0x070D7509
```

This is a major static negative result: the generic payload for `Play Scenario` is not
`0x070D74E7`. The latter belongs to separate EP1 object 87. A direct
`Play Scenario 0x070D7509 → DE99B0 branch 0x070D74E7` route is therefore not established; any
property path must use another listener/forwarded route or another control identity.

## Reproduction

```powershell
python tools/find_imm.py 0x00e1132f --ctx 15
```

SDK sources: `Spore/UTFWin/IWindow.h`, `Spore/UTFWin/IWinProc.h` and `Spore/UTFWin/Message.h`.
