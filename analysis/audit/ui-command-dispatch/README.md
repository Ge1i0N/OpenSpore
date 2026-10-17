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

## Reproduction

```powershell
python tools/find_imm.py 0x00e1132f --ctx 15
```

SDK sources: `Spore/UTFWin/IWindow.h`, `Spore/UTFWin/IWinProc.h` and `Spore/UTFWin/Message.h`.
