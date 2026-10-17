# GGE message-to-state dispatch

Reviewed **2026-10-17**. Static persistent Ghidra evidence from the selected GA-unpacked binary;
no original process was executed. The generated C/instruction pairs are retained in this directory.

## Handler contract

The GGE object vtable at `0x0147E030` contains `0x00DE99B0` at slot `+0x18` (`0x0147E048`).
The body has the shape of a GGE message listener.
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

The seven state/UI constants used by the bounded handler/helper bodies were also found as actual
SPUI `ControlID` properties in the shipped corpus; see [the data cross-check](control-id-correlation.md).
The bounded GA immediate scan found no separate MessageManager producer for them, so they remain
ControlID/handler identities rather than independently proven message producers.

Do not conflate `ControlID 0x070D7509` (`Play Scenario`, EP1 `GGEZoomPanelV2`) with handler/state
value `0x070D74E7`; the values differ and the bounded native scan finds the former only in UI setup.

Conversely, `0x036C4A10` is directly corroborated as the `Dev` ControlID in `GlobalUIGGE-9` and as
the `DE99B0` branch that invokes StateManager-compatible vtable `+0x28(1)`. This is the current
strongest shipped-UI-to-state link. The state script identifies target id 1 as `DevLauncher`, not
Play Spore state 2; it is still static evidence, not a runtime click observation.

The related state-to-UI helper `DE48C0` maps controller state 5/13 to UI IDs `0x061DD69B` and
`0x070D74E7`, respectively, via `00810650` and control vtable `+0x28`. Its retained evidence is
separate in [gge-state-ui](../gge-state-ui/README.md); matching IDs are not treated as a proven
message feedback loop without a caller/payload observation.

## Exact `0x070D74E7` feedback branch

The local GA instruction pass further bounds the separate `0x070D74E7` message/state route. After
the ID comparison, the body:

```text
guard 0x6854E0(2)
→ object/resource lookup
→ 0x00810650 lookup with UI identity 0x0806A310
→ resolved UI vtable +0x7C(1,0)
→ controller +0x104 = 13
→ controller +0x108 = 13
```

This is a concrete state-feedback branch for `0x070D74E7`, subject to its guards. It does not use
`0x070D7509`: the latter remains present only in the bounded `DE48C0`/`DE5900` UI setup tables,
with no click producer or ScenarioMode post established.

## Shared UI target `0x0806A310`

The same UI identity used by the feedback branch is also referenced from `DF6C10`, the common
continuation reached by the Play Spore state-2 enter/refresh path. It is close to `DF6D80` in the
address space, but it is **not** the `DF6D80` phase-1 body:

```text
DE9C88  → 0x00810650 → 0x0806A310 → UI vtable +0x7C
DF6C56  → 0x00810650 → 0x0806A310 → UI vtable +0x7C
```

Direct-call inventory for the continuation is:

```text
DF6C10 direct call: DF9620
DF9740 → unconditional tail jump → DF6C10
DF6D80: separate function with its own eight direct callers
```

This corroborates `0x0806A310` as a shared GGE control/state target across feedback and the
Play-Spore state-2 common continuation. It still does not identify the upstream `Play Scenario`
click producer, and it does not turn `0x070D7509` into the state-13 event value `0x070D74E7`.

The related UI target `0x0806AA00` has two bounded GA references:

```text
DF6CBA  → state-2 continuation branch
DF9112  → separate GGE per-entry/UI lifecycle cleanup path
```

This is another shared UI target rather than a unique Play Scenario control. Its semantic name and
the exact relationship between the two lifecycle uses remain Unknown.

## SDK cross-check

`refs/Spore-ModAPI/Spore/App/IStateManager.h` puts `SwitchToState` at vtable `+0x28`, matching the
GA call shape in the `0x036C4A10` branch. This is a method-order/contract corroboration; the SDK
address table is not used as a literal GA address map.

`IMessageListener` supplies the semantic `HandleMessage(messageID, payload)` contract, while
`IMessageManager::AddListener` is at SDK vtable `+0x20` and is the slot used by `DEA1B0` when it
registers the GGE listener object (`controller + 8`). The GGE object vtable placement `+0x18` is
kept as a GA-specific observation; it is not substituted for a standalone SDK interface layout.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --message
```
