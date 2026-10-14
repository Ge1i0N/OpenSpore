# The `mode` table: it is CODE, not data — and 5 of the 17 modes are not in this build at all

Measured 2026-10-14 with `tools/mode_where.py`, `tools/ghidra_xref.py` and `tools/mode_absent.py`.
This settles a question that was about to be attacked from the wrong side.

## The question

The recovered `SporeAppStates` script fires **17 distinct modes** across 60 states, and our engine logs
every one as *"not executed (the engine's mode table is not implemented)"*. The obvious assumption was
that the mode table is another shipped data table, findable the way `SporeAppStates` was. It is not.

## Answer 1 — a mode is a named C++ class, registered by a factory in code

11 of the 17 mode names exist as **standalone ASCII strings in the oracle** and appear in **no package
at all** (all three id fields of all 150,945 records, every depth):

| mode name | string VA | referenced from |
|---|---|---|
| `Game_Tribe` | `0x01477764` | `FUN_00cdb4e0` (+0x2f), 1 ref |
| `Game_Civ` | `0x01478a00` | `FUN_00cfa480` (+0x2f), 1 ref |
| `Game_EditMode` | `0x01479ee0` | `FUN_00d1c2a0` (+0x2c), 1 ref |
| `Game_Creature` | `0x0147ac30` | `FUN_00d3bd20` (+0x2f), 1 ref |
| `Game_Cell` | `0x01485610` | `FUN_00e616c0` (+0x39), 1 ref |
| `GameCinematics` | `0x01488560` | `FUN_00eb2e40` (+0x2c), 1 ref |
| `Game_Space` | `0x01494028` | `FUN_00fddb30` (+0x2f), 1 ref |
| `LoadGame` | `0x0145da44` | `FUN_00b1e000`, `FUN_00b5d500` |
| `GalaxyGameEntry` | `0x014627ec` | `FUN_00b5dcd0`, `FUN_00ddde30` |
| `ScenarioMode` | `0x014627fc` | `FUN_00b5dcd0`, `FUN_00ef1870` |
| `TerrainEditor` | (not defined as string data) | — |

The pattern is the giveaway: **one name, one reference, from a function that starts ~0x2f bytes
earlier.** That is a per-class factory registering itself by name. Decompiled (`0xd3bd20`, 64 bytes):

```c
void FUN_00d3bd20(int *param_1) {
  iVar1 = FUN_00f47240(0xe8, &DAT_013ebc48, 0, 0, 0, 0);   // allocate a 0xe8-byte mode object
  uVar2 = (iVar1 == 0) ? 0 : FUN_00d3b960();               // construct it
  (**(code **)(*param_1 + 0x20))(uVar2, &DAT_01654c01, "Game_Creature");   // REGISTER via vtable slot 0x20
}
```

So `mode Game_Creature` does not index a table — it **selects one of a family of C++ classes**. The
family is real: the binary contains `AppMode` literals including the qualified name
`AppModeEditorBase::Save` (at file offset `0x00FF4E9D`).

## Answer 2 — five modes do not exist in this build, under any spelling

`AnimViewer`, `EffectsViewer`, `Launcher`, `ModelViewer`, `MultiverseJournal` appear **nowhere in the
24,895,528 bytes of the oracle — not even as substrings** (ASCII or UTF-16). The script asks for them;
the shipped GA binary has no such class. `Editors` occurs once, but inside an unrelated English
sentence ("...switch to the creature editor or an out..."), which is a substring false positive and
**not** a registration. So 13 states sharing mode `Editors` are asking for a name that resolves some
other way (or is built from a longer literal we have not located).

**Consequence for us:** the mode table cannot be "loaded" from data, and implementing it means
reversing each `AppMode*` class — which is the GAMEPLAY work, not the menu work. Logging
`mode X not executed` remains the honest state, and we now know exactly why.

## Answer 3 — the mode/state link runs the way we hoped

`FUN_00b5dcd0` (body `00b5dcd0..00b5e026`) takes a **(mode, sub-mode)** pair of object pointers and
maps them to **state names**, then calls `FUN_00455180(name, "")` — a state transition by name:

```
mode 0x01654c00 -> "CellGame"        mode 0x01654c01 -> "CreatureGame"
    0x01654c02 -> "TribeGame"            0x01654c04 -> "CivilisationGame"
    0x01654c05 -> "SpaceGame"            0x01654c10 -> "ScenarioMode"
    0x02ccd1d2 -> "GalaxyGameEntry"      (+ an "Editor" suffix path)
```

Those are the very state names our script defines, so the original's flow is
**mode class → named state → UI layout**, which is the chain we are building forward. Note
`0x02ccd1d2` is close to but NOT equal to `GalaxyGameEntry`'s script id `0x02ccda9c`: the script's
manual `-id` values and the mode enum's object addresses are different namespaces, and conflating
them would be the classic "confident garbage" error.

## What this changes in the plan

The next step on Main→playable is **not** modes. It is **input driving the UI**: we already read each
widget's `Area` rect and its `command_id`, so a click can be hit-tested and dispatched into the state
machine — which is what turns the menu from a picture into a menu. Modes gate the 3D game stages, and
they need `AppMode*` classes, which is the step after.

## Reproduce

```
python tools/mode_where.py        # names vs the corpus (all id fields, all depths) + the binary
python tools/mode_absent.py       # substring search for the 6 that missed; AppMode prefixes
refs/ghidra/venv/Scripts/python.exe tools/ghidra_xref.py --string Game_Creature ...
refs/ghidra/venv/Scripts/python.exe tools/ghidra_decompile.py 0xd3bd20 0xb5dcd0
```

`tools/ghidra_xref.py` is new here and general: it answers "who references this address / this string
by text", the direction the bridge did not cover. Two Ghidra API walls surfaced writing it —
`Reference.isCall()` and `Reference.getReferenceTypeName()` are **not** exposed on the `MemReferenceDB`
objects this bridge sees, despite being in the documented interface; the tool now walks a defensive
ladder of accessors instead of assuming one.
