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

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --state-ui
```
