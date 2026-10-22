# GGE generic Button dispatch and CommandID boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native button event, callback, CommandID bridge or Scenario
launch was observed. Continuation of [startup-gge-owner-fallback.md](startup-gge-owner-fallback.md).

## `00950E50`: generic message dispatch

The generic UTFWin dispatcher reads `message +0x08`, bounds-checks the message type, and dispatches
through a jump table at `0x0095110C`.

The bounded Button entries are:

```text
message type 0x17
  → 00950FD0
  → receiver vtable +0x68

message type 0x18
  → 00950FE7
  → receiver vtable +0x6C
```

The dispatcher is generic. It does not itself inspect GGE ControlID `0x070D7509`, GGE component
ID `0x070D74E7`, CommandID `0x00E1132F` or ScenarioMode tag `0x01654C10` in this bounded path.

## Generic producer payload source

`009640B0` selects the activation payload from the generic WinButton outer fields:

```text
outer +0x84 != 0 ? CommandID : outer +0x80 (ControlID)
  → message.type 0x287259F6
  → IWindow::SendMsg +0x114
```

The embedded IWindow setters establish:

```text
IWindow +0x7C → outer +0x80 = ControlID
IWindow +0x80 → outer +0x84 = CommandID
```

For the inspected EP1 `GGEZoomPanelV2` object 74:

```text
ControlID  = 0x070D7509
CommandID  = 0
generic activation payload = 0x070D7509
```

Therefore the generic producer does not statically transform object-74 ControlID into
`0x070D74E7`. The latter remains a distinct serialized/component identity.

## Consequence for Play Scenario

The strongest bounded generic route is:

```text
object 74 ControlID 0x070D7509
  → generic activation message payload 0x070D7509
  → attached WinProc walk
  → DE99B0 / owner fallback
```

The missing join remains:

```text
0x070D7509
  → ?
  → 0x070D74E7 or another GGE-specific payload
  → ? CommandID/ScenarioMode
```

No static promotion is made for that conversion. Generic Button dispatch is now bounded, but the
GGE-specific Play Scenario producer and runtime delivery remain Unknown.

## Status

Established statically:

- generic message jump-table dispatch;
- ButtonClick/ButtonSelect callback slots;
- activation payload precedence CommandID-over-ControlID;
- object-74 serialized `CommandID=0` and resulting generic payload;
- negative separation from component ID `0x070D74E7`.

Still Unknown:

- runtime click and WinProc callback delivery;
- GGE-specific payload conversion;
- CommandID `0x00E1132F` dispatch;
- ScenarioMode conversion, world/stage creation and playable gameplay.
