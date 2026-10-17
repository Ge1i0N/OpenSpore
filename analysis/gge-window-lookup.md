# GGE placement window — per-card lookup, not a global service

Reviewed **2026-10-17**. Authority: GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`, bounded static
instructions and complete sequential parses of explicitly selected shipped SPUI resources.
ModAPI supplies corroborating interface names, not execution evidence.

## Resolved question

The `0x02E5A4A8` lookup used for card placement is scoped to **that record's UILayout**. It is not a
request to an unidentified global service registry. The exact default SPUI target is object **177**
in both inspected `GGEGameStarUIv4` resources. Existing host instances already use this root; this
pass strengthens their regression assertions rather than adding another UI provider dependency.

### Instruction chain

1. `DF6119` loads `ECX = [record +0xB8]`, after `DF6115` loads the record argument into EDI.
2. `DF6127..DF612E` pushes recursive=false and ControlID `0x02E5A4A8`, then calls `810650`.
3. `810650` loads `[ECX +0x14]`; if non-null it tail-jumps to `8102A0`, otherwise returns null.
   No global registry getter or global owner load occurs in this wrapper.
4. `8102A0` enumerates that receiver's `+0x64/+0x68` component span. It requests interface
   `0xEEEE8218`, compares ControlID through slot `+0x1C`, and returns a matching window. Only when
   recursive=true does it try each window's child lookup at `+0xF0`.
5. `DF6110` uses returned-window slot `+0x38` for size, slot `+0x34` for current local position,
   and slot `+0x64` to move it when an axis changes by more than four pixels.

The pinned [ModAPI reference](community-references.md) names these as `UILayout::FindWindowByID`,
`IWindow::TYPE`, `GetControlID`, `GetRealArea`, `GetArea`, and `SetLocation`. Its `UILayout` member
`mpLayoutObjects` at `+0x14` and address pair `810C10/810650` agree with the actual selected GA.
The caller's ECX is decisive: decompiled C omits this implicit argument, which contributed to the
earlier overly general registry interpretation.

The retained `DF9200` assembly independently supplies the same per-card receiver when configuring
stage controls (`DF932E`, `DF936B`, `DF93B4`) and status flags. `DF7690` loads the UI by name on its
own receiver before looking up controls. Thus repeated ControlIDs across different mounted cards
do not imply one shared positioning window.

## Shipped-resource observations

| Package | Record | SPUI instance | Matching object | Parent / serialized root |
|---|---:|---|---:|---|
| PatchData | 19201 | `21DF01F2` | 177 | none / true |
| Spore_Graphics | 18791 | `21DF01F2` | 177 | none / true |

Each payload is 24,812 bytes with 152 objects and 58 windows; the two hashes differ and remain
separate observations. Root 177 is class `0x4EC1B8D8`, has local Area `(76,167,124,215)` (48×48),
raw flags `0x13`, and ordered Children `[28,41,90,176]`. It is the **only** window with this ControlID
in each layout, the sole serialized flagged root, and the sole parentless window.

The selected `GlobalUIGGE-9` resources (PatchData 10642 / Graphics 10399), `GGEZoomPanel`
(PatchData 3113 / Graphics 2987), and `GGEZoomPanelV2` (EP1_Data 12415) contain zero matching
ControlIDs. These are complete parses of seven selected layouts, **not** a negative search over
every resource or user override. No package priority is inferred.

## What this does not establish

- The SPUI root match identifies the default asset. It does not observe native creation, component
  registration, mount success, parent notifications or runtime configuration overrides.
- Serialized Area is not a captured runtime `GetRealArea` result. WinProc/layout transforms remain
  separate from the host's explicit geometry observations.
- Other controls have duplicates **within** a card. Host object choices remain explicit; SDK
  first-match behavior is not a reason to silently choose object 45 over 55, for example.
- This window is not the camera. The `67DCD0 +0x58` projection and controller `+0x50 -> +0x38`
  acquisition routes remain distinct, and their ownership/equivalence remain unresolved.
- No MoviePlayer, DBCheck, terrain lookup, runtime-refill or production catalogue gate is completed.

REA session inspection reported no open target. This pass used existing Ghidra instruction extracts
and three tiny objdump ranges, each with a 15-second subprocess timeout; it did **not** launch a
Ghidra worker, reimport the PE or claim new retained REA Evidence IDs.

## Reproduction and verification

```powershell
python tools/gge_evidence.py --static --window-lookup
cmake --build build --config Debug --target gge_star_present_verify gge_star_instance_verify gge_star_lifecycle_verify gge_star_scene_verify
```

[Audit facts](audit/gge-window-lookup/facts.json) retain complete matching-object properties,
decoded-resource hashes, parse consumption and the seven-layout comparison.
[Wrapper instructions](audit/gge-window-lookup/layout-window-wrapper.asm.txt),
[root search](audit/gge-window-lookup/layout-root-window-search.asm.txt), and
[caller receiver](audit/gge-window-lookup/card-placement-receiver.asm.txt) retain byte-level ranges.

On separately extracted fixtures from the two explicit packages: presentation **50**, ownership
**167**, lifecycle **232**, scene **208** checks PASS. Ownership tests continue to move only the
selected card, leaving the source and sibling instance unchanged. Metadata and camera inputs in
these tests remain controlled; image pixels and layout bytes are original resources.
