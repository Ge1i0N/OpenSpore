# GGE message-to-state dispatch

Reviewed **2026-10-17**. Static persistent Ghidra evidence from the selected GA-unpacked binary;
no original process was executed. The generated C/instruction pairs are retained in this directory.

## Handler contract

`FUN_00DE99B0` is referenced by data at `0x0147E048` and has the shape of a GGE message listener.
It first forwards the message to an optional object at controller `+0x34`, requires event-object
type `0x287259F6` at payload word `+0x08`, then calls that event object's vtable `+0x1C` to obtain
the event ID.

Observed GA branches:

| Event ID | Observed state-side effect |
|---:|---|
| `0x036C4A10` | Game-mode/state manager getter `0x0067DCE0`, vtable `+0x28`, called with `1`. SDK `IStateManager` describes `+0x28` as `SwitchToState`; the GA object/argument are observed, higher-level reason is not guessed. |
| `0x062D88E9` | controller `+0x104 = 2`, `+0x108 = 2` |
| `0x061DD69B` | controller `+0x104 = 2`, `+0x108 = 2` |
| `0x0615B966` | controller `+0x104 = 5`, `+0x108 = 5` |
| `0x07BFB76E` | controller `+0x108 = 2` |
| `0x070D74E7` | after its guards, controller `+0x104 = 13`, `+0x108 = 13` |
| `0x02F1CD64` | calls controller-side `E00640(0x42,0)` when `+0x34` exists |
| `0x036850D4`, `0x043B72D0`, `0x0447060A` | call an object obtained from `DF5F10` through vtable `+0x60`; purpose remains Unknown |

`FUN_00DE8680` is the observed internal state setter/queue helper: with its boolean argument false
it writes requested state `+0x10C`; with true it normalizes current states 0/1/8/9 to 2, queues
the current value at `+0x114/+0x118`, and writes the requested value. `FUN_00DF9740` separately
checks controller `+0x108 == 2` before showing GlobalUI, rebuilding cards through `DF95A0` and
continuing through `DF6C10`. Thus the static route is:

```text
DE99B0 event branch → controller state/request fields
  → GGE state/update lifecycle
  → DF9740 (+0x108 == 2)
  → DF95A0 catalogue publication
```

The exact indirect caller that invokes the Play Spore vtable `+0x38 = DF9740` remains a separate
boundary. Setting fields in a static branch is not runtime execution or catalogue readiness.

## SDK cross-check

`refs/Spore-ModAPI/Spore/App/IStateManager.h` puts `SwitchToState` at vtable `+0x28`, matching the
GA call shape in the `0x036C4A10` branch. This is a method-order/contract corroboration; the SDK
address table is not used as a literal GA address map.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --message
```
