# GGE message IDs cross-checked against shipped SPUI ControlIDs

Reviewed **2026-10-17**. This is a static shipped-data correlation, not runtime message tracing.
The selected GA package corpus contains 748/748 parsed SPUI layouts. The probe does not merge
package priority or execute a native UI message.

## Exact matches

| Value | Shipped layout/control observation |
|---:|---|
| `0x062D88E9` | `GlobalUIGGE-9` root copy: PatchData/Spore_Graphics object 266; EP1 copy object 276 |
| `0x0615B966` | `GGEZoomPanel` object 57; EP1 `GGEZoomPanelV2` object 53 |
| `0x0615B616` | `GGEZoomPanel` object 35; EP1 `GGEZoomPanelV2` object 64 |
| `0x061DD69B` | `GGEZoomPanel` object 46; EP1 `GGEZoomPanelV2` object 42 |
| `0x070D74E7` | `GGEZoomPanelV2` object 87 |
| `0x070D7509` | `GGEZoomPanelV2` object 74 |
| `0x07BFB76E` | one EP1 layout record 13754, object 459 |

Every listed match is a `ControlID`, not a `CommandID` in the bounded corpus; all 15 exact matches
carry serialized `CommandID = 0`. Their parent is a GGE panel/container (for example object 81 in
the zoom panel and object 1319 in GlobalUIGGE-9). This strongly
corroborates that the constants consumed by `DE99B0`/`DE48C0` belong to the GGE UI/control route.
It does not prove which UI action posts each message, nor that a ControlID is globally unique across
package copies/layouts. The native handler may receive a higher-level UI event whose ID is derived
from the control; that producer remains Unknown.

One inline caption is already present in the bounded data: `0x070D7509` has caption text
`Play Scenario` in the EP1 `GGEZoomPanelV2` copy. Other targeted captions are locale-table
references whose table/key resolution is intentionally not substituted here.

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
