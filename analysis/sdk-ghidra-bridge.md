# SDK ↔ Ghidra bridge and next probes

Reviewed **2026-10-17**. This is a working bridge between secondary SDK names and the selected
GA-unpacked binary. SDK names are hypotheses until the GA body, vtable order or shipped resource
corroborates them.

## Current authority order

1. GA-unpacked bytes and persistent Ghidra project;
2. shipped GA package/SPUI data;
3. bounded objdump/PE/xref results;
4. pinned `Spore-ModAPI` SDK names and `SDKtoGhidra` tables;
5. SporeModder-FX class/property registry;
6. MiniWiki/Beta/wiki command and UX notes.

An SDK address is never accepted merely because the name is plausible. The address must be checked
against the GA SHA-256 and a focused body/vtable/resource observation.

## Confirmed useful SDK contracts

| SDK contract | Secondary source | GA-side use |
|---|---|---|
| `UTFWin::IWindow::SetFlag` at vtable `+0x7C` | `Spore/UTFWin/IWindow.h` | useful candidate for DebugConsole visibility state; native `FUN_00802B20` applies a state through a UI slot at `+0x7C` |
| `UTFWin::IWinProc::GetEventFlags` `+0x14`, `HandleUIMessage` `+0x18` | `Spore/UTFWin/IWinProc.h` | method-order hypothesis for listener callbacks; current Graphics service callback remains version-specific until its vtable is mapped |
| `UTFWin::Message` event type `+0x08`, key `vkey +0x10`, modifiers `+0x14` | `Spore/UTFWin/Message.h` | matches the need to interpret GA event `0x1EE1001`; payload offsets still require GA corroboration |
| `KeyModifiers`: Shift=1, Ctrl=2, Alt=4 | `Spore/Input.h` | exactly matches GA Win32 transport modifier mask |
| `IWindow::AddWinProc/RemoveWinProc` around `+0xB0/+0xB4` | `Spore/UTFWin/IWindow.h` | candidate registration path for the unresolved listener; not yet mapped to the GA Canvas `+0x5C` field |
| `IWindowManager::SendMsg` at `+0x10` | `Spore/UTFWin/IWindowManager.h` | candidate for higher-level event delivery, separate from Win32 Canvas transport |
| `App::IMessageManager::MessageSend` `+0x14`, `AddListener` `+0x20`, `AddHandler` `+0x28` | `Spore/App/IMessageManager.h` | `FUN_00812D80` copies an 8-dword payload, uses event `0x1EE1001` and calls global service vtable `+0x14`; strong GA match for `MessageSend` |
| `App::IMessageListener::HandleMessage(messageID,payload)` | `Spore/App/IMessageListener.h` | GGE vtable `0x0147E030 +0x18 = DE99B0` consumes the object/message shape; GA-specific vtable placement is retained separately from SDK inheritance order |
| `App::IGameMode` method order | `Spore/App/IGameMode.h` | comparison only: SDK has `OnEnter +0x18`, `OnExit +0x1C`, `OnMouseWheel +0x38`, `Update +0x3C`; GA vtable `0x147EF50` instead has `DF9620 +0x0C`, `DF85B0 +0x10`, `DF8700 +0x14`, `DF9740 +0x38`, `DF5F00 +0x3C`, `DF6010 +0x68`. GA `DF9740` is not promoted to SDK `OnMouseWheel`. |
| `App::IPropManager::GetPropertyList` `+0x2C`, `AddPropertyList` `+0x34` | `Spore/App/IPropManager.h` | GA vtable `0x01409210` contains `0x006A60E0`/`0x006A61F0` at these slots; `DF7CB0` uses the `+0x2C` contract for terrain property lookup |
| `CheatManager.AddCheat`, `ICommand::ParseLine`, `GetDescription` | `Documentation/Cheats.md` | corroborated by GA `App/CheatManager`, `help`, `history`, `clear`, line parser and output buffer |

## GA-correlated results

- `UI/DebugConsole` is an observed GA string/resource key at `0x0141775C`.
- GA creates/registers a hidden DebugConsole during UI startup, then connects `AppConsole` and
  `App/CheatManager`.
- `AppConsole` vtable `0x0140195C +0x00` is the line dispatcher `0x0067EAA0`; `+0x04` is help
  `0x0067E240`.
- CheatManager vtable `0x01401B40 +0x38` is execution callback `0x0067EF20`.
- `levels` native option table is recovered at `0x00DDDEE0`/`0x00DDDF40`:
  `-unlock` value 0 and `-unlockAdventures` value 1.
- GA WinProc `0x00849310` routes keydown/up/char to `0x00848620/0x00848700/0x008487D0` and
  offers a pre-dispatch listener at `Canvas+0x5C`.
- The listener source is service ID `0x0114259C`, created by the Graphics service path and copied
  into Canvas by `0x00A169B0`.
- `0x00697CA0` from the SDK table did not have an instruction boundary in the persistent GA project;
  its `GameInput_KeyDown` name remains a secondary mismatch, not a recovered GA fact.
- `0x008153C0` seed/decompile is a tiny indirect call stub; the SDK name `MainWindow_OnKey` is useful
  but its exact body semantics remain incomplete.
- `0x00812D80` is a stronger current GA match for message posting: it copies an eight-dword payload,
  sets event `0x1EE1001`, obtains the global message service and calls a vtable `+0x14` method. This
  corroborates the SDK message-manager model without proving the SDK's exact method address.
- The SDK table's `0x0067F150 CheatManager_ProcessLine` does not replace the recovered GA
  `AppConsole` dispatcher `0x0067EAA0`; the seeded body is a small helper around command execution.
- SDK `MessageManager_AddListener/AddHandler/PostMSG` addresses around `0x008844D0`/`0x00884520`/
  `0x00885F10` also require version-specific reconciliation: current GA bodies at those addresses
  include allocator/cleanup-shaped routines rather than a direct one-to-one name match.

## Next focused ideas

1. Resolve the Graphics service event object callback created by `0x00753D70` and callback entry
   `0x00753870`; inspect the dynamic vtable slot `+0x170` without assuming it is `IWinProc`.
2. Map GA vtable slots against `IWinProc`/`IWindowManager` only where signatures and payload offsets
   agree; record mismatches explicitly.
3. Inspect `0x00812D80`, the App/UI consumer that forwards event `0x1EE1001` through the global
   manager. This is currently a stronger hotkey candidate than low-level Graphics code.
4. Compare `0x0067F150` with the SDK's `CheatManager_ProcessLine` and native `FUN_0067EAA0`; preserve
   the possibility that SDK addresses refer to a nearby version/build variant.
5. Use `MiniWiki` command names only as search seeds. Promote a command to host behavior only after
   GA registration, handler and service side effects are observed.
6. Once the listener callback is mapped, add a small read-only native-console audit fixture rather
   than implementing the EA cheat services wholesale.

## Reusable commands

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_callers.py 0x00812D80 0x0067EAA0
python tools/ghidra_run.py --timeout 180 tools/ghidra_decompile.py 0x00812D80 0x0067F150
python tools/ghidra_run.py --timeout 180 tools/ghidra_field_refs.py "+ 0x5c" 120 0x00800000
```

The bounded runner keeps the persistent project reusable, emits heartbeats and distinguishes timeout
from ordinary probe failure. No broad new Ghidra import is needed for these ideas.
