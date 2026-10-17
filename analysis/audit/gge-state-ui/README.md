# GGE internal state → UI synchronization

Reviewed **2026-10-17**. Static GA-unpacked Ghidra evidence only; no original runtime execution.

`DE48C0` maps the controller's internal state field `+0x108` to UI resource/control handling. The
body initializes an event/control ID table containing `0x061DD69B`, `0x0615B966`, `0x0615B616`,
`0x070D74E7` and `0x070D7509`. For state `5`, it selects `0x061DD69B`; for state `13`, it selects
`0x070D74E7`. It then looks up controls through `00810650` and applies vtable `+0x28` flags and
related UI operations. Helpers `DE4A20`, `DE4AE0` and `DE4BD0` load/configure separate GGE UI
objects and update internal mode resources.

This is a state-to-UI path, not the Play Spore catalogue rebuild itself. `DF9740` remains the
GA-specific vtable slot whose state-2 guard calls `DF95A0`; no direct `DE48C0 → DF9740` edge is
claimed. The matching IDs in `DE99B0` therefore remain event/state feedback candidates until a
specific caller and payload route is recovered.

### Additional GA byte-level cross-check

The local GA `objdump` pass found the two direct immediate references to `0x070D7509`:

```text
0x00DE4918  inside DE48C0
0x00DE595F  inside DE5900
```

Both occur beside the distinct value `0x070D74E7`, matching the existing state/UI table rather
than a button event comparison. `DE48C0` uses the table while looking up UI objects and applying
state operations; `DE5900` initializes the GGE UI path, reuses the same IDs through the object
lookup/vtable `+0x104` route, then calls `DE48C0` before continuing initialization.

This strengthens the classification of `0x070D7509` as a shipped GA GGE UI/control identity. It
does **not** identify a mouse/button producer, a CommandID route, or a post to ScenarioMode tag
`0x01654C10`; those remain Unknown.

### Direct caller inventory

The local `.text` scan for direct `E8 rel32` calls found:

```text
DE48C0 callers:
  DE5A86
  DE8A71

DE5900 callers:
  DEA1B0
```

`DE5900` is therefore only reached directly from the already mapped
`GalaxyGameEntry` activation-shaped path `DEA1B0`; this supports its lifecycle/UI-initialization
classification rather than a button producer role. `DE48C0` is reached from that initialization
via `DE5A86` and from the separate `DE8A71` helper/update path. No direct caller from the bounded
UTFWin Button/CommandID route was found in this inventory.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --state-ui
```
