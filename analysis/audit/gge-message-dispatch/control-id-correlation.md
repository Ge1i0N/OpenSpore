# GGE message IDs cross-checked against shipped SPUI ControlIDs

Reviewed **2026-10-17**. This is a static shipped-data correlation, not runtime message tracing.
The selected GA package corpus contains 748/748 parsed SPUI layouts. The probe does not merge
package priority or execute a native UI message.

## Exact matches

| Value | Shipped layout/control observation |
|---:|---|
| `0x036C4A10` | `GlobalUIGGE-9` object 133, inline caption `Dev`, parent object 1490; the same value is handled by `DE99B0` through StateManager vtable `+0x28(1)`, which the shipped state table identifies as explicit `DevLauncher` state id 1 |
| `0x062D88E9` | `GlobalUIGGE-9` root copy: PatchData/Spore_Graphics object 266; EP1 copy object 276 |
| `0x0615B966` | `GGEZoomPanel` object 57; EP1 `GGEZoomPanelV2` object 53 |
| `0x0615B616` | `GGEZoomPanel` object 35; EP1 `GGEZoomPanelV2` object 64 |
| `0x061DD69B` | `GGEZoomPanel` object 46; EP1 `GGEZoomPanelV2` object 42 |
| `0x070D74E7` | `GGEZoomPanelV2` object 87 |
| `0x070D7509` | `GGEZoomPanelV2` object 74 |
| `0x07BFB76E` | one EP1 layout record 13754, object 459 |

Every listed match is a `ControlID`, not a `CommandID` in the bounded corpus; all 18 exact matches
carry serialized `CommandID = 0`. Their parent is a GGE panel/container (for example object 81 in
the zoom panel and object 1319 in GlobalUIGGE-9). This strongly
corroborates that the constants consumed by `DE99B0`/`DE48C0` belong to the GGE UI/control route.
It does not prove which UI action posts each message, nor that a ControlID is globally unique across
package copies/layouts. The native handler may receive a higher-level UI event whose ID is derived
from the control; that producer remains Unknown.

One inline caption is already present in the bounded data: `0x070D7509` has caption text
`Play Scenario` in the EP1 `GGEZoomPanelV2` copy. Other targeted captions are locale-table
references whose table/key resolution is intentionally not substituted here.

## Play Scenario object details

The explicit EP1 record `Spore_EP1_Data.package`, ordinal `12415`, was inspected directly for the
matching object:

```text
object       74
class        0xEEEFE2C3
ControlID    0x070D7509
CommandID    0
parent       138
Drawable     object 73
serialized   WinProcs property absent
```

This proves the control is a real serialized window/object with the expected caption and parent,
but its event/WinProc attachment is not present in the serialized SPUI object. GA `DE5900` now
provides the native explanation: after the GGE layout request it resolves `0x070D7509` through
`00810650` and calls the resolved window's `IWindow::AddWinProc` slot `+0x104`, attaching the GGE
multi-vtable object's `+0x04` interface. The absence is therefore not treated as “no handler”.

The remaining producer question is narrower but still open: the attached `DE99B0` listener accepts
UTFWin `kMsgComponentActivated` and recognizes `0x070D74E7`, while the attached control's identity
is `0x070D7509`. Static attachment does not by itself establish the click-to-payload conversion or
runtime execution.

The generic WinButton payload rule is now bounded separately from this object correlation:

```text
CommandID != 0 ? CommandID : ControlID
```

Object 74 has serialized `CommandID = 0`, so its generic activation payload is statically
`0x070D7509`. Object 87 separately carries `0x070D74E7`. This makes the direct object-74-to-
`DE99B0(0x070D74E7)` interpretation unsupported by the selected static evidence; a forwarded or
separate control route remains possible but Unknown.

The EP1 object hierarchy further separates the two identities:

```text
object 138 (parent 139)
  ├─ object 74  WinButton, ControlID 0x070D7509, caption Play Scenario
  └─ object 114 panel/control 0x0806AA00
       └─ object 87 WinButton, ControlID 0x070D74E7
```

This is shipped-data structure only. It does not identify the native message or listener that may
connect the sibling `Play Scenario` control to the nested state-feedback control.

The `0x036C4A10` match is the strongest current UI→state corroboration: the shipped `Dev` control
and the native handler branch share the exact value, and the native branch calls the SDK-compatible
StateManager `+0x28(1)` slot. The shipped `SporeAppStates` table assigns explicit id 1 to
`DevLauncher`, not `Play Spore`; therefore this control is a developer/launcher route and must not
be presented as the gameplay state-2 entry. This still does not prove the complete UI dispatch
scheduler or live state transition execution.

The similarly named state/UI value `0x070D74E7` is a different value. `find_imm` finds
`0x070D7509` only in the `DE48C0`/`DE5900` UI-control tables in the bounded native scan; no direct
handler branch for `0x070D7509` was observed. Therefore the corpus proves the control identity and
caption, not that clicking it posts `DE99B0(0x070D74E7)` or enters Adventure/gameplay.

The bounded GA immediate scan found no separate `MessageManager` send/post call that emits these
values. Their current classification is therefore **GGE UI ControlID / handler identity**, not an
independently proven message producer. `0x07BFB76E` also has a separate consumer at `0x00DFE422`
that reads an event-object ID; this does not establish a producer or connect it to `DF9740`.

## Reproduction

```powershell
python tools/gge_control_id_probe.py --output C:\captures\gge-control-ids.json
```

The default root is the existing shipped corpus under `build/bin/Debug`; pass `--root` explicitly
for a different caller-selected corpus. No user-data directory is searched.
