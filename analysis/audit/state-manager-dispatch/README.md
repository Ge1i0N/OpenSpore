# GA StateManager command dispatch

Reviewed **2026-10-17**. Static GA-unpacked evidence, persistent Ghidra project, no target launch.

## Exact SDK↔GA contract

The vtable-only command handler at `0x007E4FF0` is the shipped state-console command path. Its
nearby strings are:

```text
Switch to given state.
List all states
-next
-prev
-reload
no such state
```

The bounded instructions at `0x007E5050` use an object at `[this+0x10]`:

```text
object->vtable[+0x38](state_name)
  → returns state ID or -1

object->vtable[+0x28](state_id)
  → switches to the requested state
```

This is a direct GA corroboration of SDK `App::IStateManager`:

```text
+0x28 SwitchToState(uint32_t)
+0x38 StateIDFromName(const char*)
```

The two slots are therefore confirmed semantically without accepting an SDK address table as the
GA address map. This `+0x38` call is **not** the GGE Play Spore `DF9740` caller: it is a state
console command handler, and its object/strings distinguish it from the `0x147EF50` GGE vtable.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_seed.py analysis/audit/seeds.tsv
python tools/ghidra_run.py --timeout 180 tools/ghidra_list.py 0x007E4E80 220
```

The function seed is retained in `analysis/audit/seeds.tsv` so the persistent project keeps the
indirect command boundary available for later state-transition work.
