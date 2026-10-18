# AppMode registration table — code, names and execution boundaries

> **Current verdict, 2026-10-15:** ten GA binary registrations are represented in
> `source/engine/appmodes.cpp`; their enter/tick/render behavior is not implemented. Script mode
> names and binary tag objects are different namespaces. The older string-search negatives below
> are bounded observations, **not proof that a class does not exist**. Input/visible launcher
> navigation now works; next runtime work is described in [RESUME](../RESUME.md).

**GGE follow-up:** [GalaxyGameEntry driver/UI trace](galaxy-entry.md) now follows the registered
vtable into controller setup, activation and code-owned UI names. Default primary `GlobalUIGGE-9`
is presented by our host; full original driver lifecycle, galaxy scene and events remain missing.

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

## Answer 2 — seven of the script's 17 mode names are not in the GA mode table

`AnimViewer`, `EffectsViewer`, `Launcher`, `ModelViewer`, `MultiverseJournal` appear **nowhere in the
24,895,528 bytes of the oracle — not even as substrings** (ASCII or UTF-16). The script asks for them;
the shipped GA binary has no such class. `TerrainEditor` and `Editors` DO occur in the oracle, but not
as defined string data, so an xref-by-text cannot reach them and their registration is not located yet.
`Editors` also occurs inside unrelated English prose ("...switch to the creature editor or an out..."),
which is a substring false positive and **not** a registration.

**The Beta190208 build settles what those names were** (next section): `Launcher`, `AnimViewer`,
`ModelViewer`, `TerrainEditor` and `Editors` are all present there, so they are REAL modes that were
CUT between February and release — the GA script simply was not pruned. `EffectsViewer` and
`MultiverseJournal` are in neither build. So the honest split is 10 in the GA table, 5 cut after the
beta, 2 in neither.

**Consequence for us:** the mode table cannot be "loaded" from data, and implementing it means
reversing each `AppMode*` class — which is the GAMEPLAY work, not the menu work. The five the beta
adds are all DEV tools (viewers/editors), none on the Main->playable path, so they do not unblock the
next step; the five playable stages are already in the GA table. Logging `mode X not executed` remains
the honest state, and we now know exactly why.

## The Beta190208 build: useful, and exactly how much (checked 2026-10-14)

`refs/bins/Beta190208_Also/SporeApp.exe`, 32,616,448 B, dated 2008-02-19 - the build the Beta190208
PDB belongs to. Two questions, answered separately because they are different questions.

**1. Is its code readable?** Yes, and this is measured rather than assumed - Shannon entropy per PE
section (`tools/beta_check.py`):

| section | raw size | entropy | reading |
|---|---|---|---|
| `.text` | 23,535,616 | **6.27** | **ordinary x86 - NOT encrypted** |
| `.rdata` | 2,199,552 | 5.35 | ordinary data |
| `.data` | 1,097,728 | 3.57 | sparse |
| `ars` | 3,248,128 | **7.9999** | encrypted/compressed |
| `artem` | 1,044,480 | **7.9952** | encrypted/compressed |
| `.securom` | 1,363,968 | 5.58 | the protection's own section |

Compare the retail GA `SporeApp.exe`, whose `.text` is **8.0000** - i.e. SecuROM-encrypted, which is why
the project's oracle is the *unpacked* GA dump and never the retail file. So: the beta IS a SecuROM
build (it has `ars`/`artem`/`.securom`), but its 23.5 MB of code is in the clear.

**2. Does it carry the mode names the GA oracle lacks?** Partly, and the answer is more interesting
than a yes:

| name | in GA oracle | in beta | where in the beta |
|---|---|---|---|
| `Launcher` | absent | present | .rdata, only inside longer strings |
| `AnimViewer` | absent | present | .rdata, 1 standalone C string |
| `ModelViewer` | absent | present | .rdata, 1 standalone C string |
| `TerrainEditor` | present (no registration located) | present | .rdata, 1 standalone C string |
| `Editors` | present (no registration located) | present | .rdata, 1 standalone C string |
| `EffectsViewer` | absent | **absent** | - |
| `MultiverseJournal` | absent | **absent** | - |

And a dating signal: **`ScenarioMode` occurs ZERO times in the beta** while being a registered GA mode,
so it was added between February and release. The betas-to-GA story is therefore "editors cut, one
game stage added", which explains the GA script asking for mode names its own binary does not contain:
the script is data and was not pruned.

**A NEGATIVE RESULT WORTH KEEPING, because it nearly became a claim.** I tried to establish whether the
beta *registers* those five names, without importing it into Ghidra (a ~20-minute job for a small
question), by converting each string's file offset to a VA and scanning `.text` for the little-endian
dword - the encoding a `push imm32` of a string address uses. Every missing name came back with **0
references**. That looked like an answer. It is not: `tools/beta_control.py` ran the SAME method on the
ten names the GA oracle provably registers - `Game_Creature`, `Game_Space`, `Game_Tribe`, `Game_Civ`,
`LoadGame`, `GalaxyGameEntry`, `Game_EditMode`, `GameCinematics` - and they came back **0 as well**. A
method that reports "no reference" for things that certainly have references is broken, and it can
conclude nothing. **The correct tool for the beta is Ghidra, the same as for the oracle.** Both
byte-scan scripts are deleted rather than left in the tree to be trusted later.

**Verdict on usefulness.** Real but bounded, and off the current path:

* **What it is good for:** a cut-content reference (five names GA no longer ships, with the strings
  still present), and a second, independently dated build to cross-check anything we are unsure about -
  the same role `upstream/SporeMaster3` plays, and with the same rule attached: **its ADDRESSES are
  never the target's.** Project rule: the reversal oracle is the GA `SporeApp.exe.unpacked.exe` alone.
* **What it is NOT good for right now:** the five names it adds are `AnimViewer`, `ModelViewer`,
  `TerrainEditor`, `Editors`, `Launcher` - all DEV tools (viewers and editors), none of them on the
  Main->playable path. The five playable stages (Cell, Creature, Tribe, Civ, Space) plus
  GalaxyGameEntry, ScenarioMode, LoadGame and GameCinematics are ALL already in the GA table, so the
  beta does not unblock the next step. It becomes valuable when the editors are on the path, or if a
  GA question turns out to be answerable by comparison.

## The complete mode table, read out of the binary (2026-10-14, later)


The registration functions are tiny, and there are exactly ten. Each one is a single call:

```c
(**(code **)(*<registry> + 0x20))(object, <MODE_TAG>, "<MODE NAME>");
```

so the table below is not inferred from names matching — it is the argument list of a registration
call, with the object the factory just allocated beside it.

| mode name | MODE TAG (2nd arg) | registers from | object size | constructor |
|---|---|---|---|---|
| `Game_Cell` | `0x01654C00` | `0x00e616c0` | `0x0c` | vtable `PTR_LAB_01485490` |
| `Game_Creature` | `0x01654C01` | `0x00d3bd20` | `0xe8` | `0x00d3b960` |
| `Game_Tribe` | `0x01654C02` | `0x00cdb4e0` | `0x3a8` | `0x00cd9b90` |
| `Game_Civ` | `0x01654C04` | `0x00cfa480` | `0x150` | `0x00cf9a10` |
| `Game_Space` | `0x01654C05` | `0x00fddb30` | `0x198` | `0x00fdc9f0` |
| `Game_EditMode` | `0x01654C06` | `0x00d1c2a0` | `0x54` | `0x00d1c150` |
| `LoadGame` | `0x01654C08` | `0x00b5d500` | `0x04` | vtable `PTR_LAB_014625b0` |
| `ScenarioMode` | `0x01654C10` | `0x00ef1870` | `0xe0` | `0x00ef0210` |
| `GalaxyGameEntry` | `0x02CCD1D2` | `0x00ddde30` | `0x10` | vtable `PTR_FUN_0147d2b0` |
| `GameCinematics` | `0x8916F92D` | `0x00eb2e40` | `0x48` | `0x00eb28b0` |

**The MODE TAG is the address of a static marker object, not a sequence number.** The values
`0x01654C00/01/02/04/05/06/08/10` sit one byte apart in `.data` — these are empty, zero-initialised
statics whose ADDRESS is their identity, so the linker packed them adjacently. That is why the numbers
have holes (no `03`, `07`, `09`…): a hole is simply a marker the linker placed elsewhere or that is not
used as a mode. `GalaxyGameEntry` and `GameCinematics` use markers in other data regions, which is why
their values look unrelated.

**CROSS-VALIDATION against the state script.** Every one of the ten names is exactly a name the shipped
`SporeAppStates` script's `mode` verbs ask for, and the pairing is one-to-one:

| mode | the state whose script says `mode <this>` |
|---|---|
| `Game_Cell` / `Game_Creature` / `Game_Tribe` / `Game_Civ` / `Game_Space` | `CellGame` / `CreatureGame` / `TribeGame` / `CivilisationGame` / `SpaceGame` |
| `Game_EditMode` | `LevelEditor` |
| `LoadGame` | `LoadGame` |
| `ScenarioMode` | `ScenarioMode` |
| `GalaxyGameEntry` | `GalaxyGameEntry` |
| `GameCinematics` | `GameCinematicsMode` |

Two independent artifacts — the binary's registration calls and the shipped script — agree on all ten.
And `FUN_00b5dcd0` (the mode→state mapper) switches on **the same tags** to select the transition:
`0x01654C00→"CellGame"`, `01→"CreatureGame"`, `02→"TribeGame"`, `04→"CivilisationGame"`,
`05→"SpaceGame"`, `10→"ScenarioMode"`, `0x02CCD1D2→"GalaxyGameEntry"`, plus an `"Editor"` suffix path.
Three functions, one tag space.

**What a mode IS, now that the table is complete.** A mode is a lightweight driver object (4 bytes for
`LoadGame`, 12 for `Game_Cell` — just a vtable, up to `0x3a8` for `Game_Tribe`) registered under a tag
and selected by tag. The behaviour lives in the class's vtable methods, not in the registry. So
"implementing the mode table" is not a data-loading job: it is implementing ten driver classes, which
is the gameplay work. What we CAN now do — and do — is hold the table, so the engine knows the tag,
the class address and the state each mode drives instead of printing "not implemented".

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

Static transition evidence now adds a concrete native edge inside this tag space:

```text
Game_Space / SpaceGame 0x01654C05
  → ScenarioMode 0x01654C10
  → pre-enter callback B33A80 → B335F0 service preparation
  → post-enter callback B336E0 → B33650 service preparation
```

The pair is guarded in two distinct `0x00B33xxx` callbacks, not one continuous guard/call body.
The producer `7D8670` publishes tags from its registered table around the new object's OnEnter;
`B63650` consumes the messages and reaches these callbacks. This proves static transition handling,
not the unresolved upstream selection of the ScenarioMode target by GGE `Play Scenario 0x070D7509`.
See [the retained evidence and phase correction](audit/scenario-mode/message-route.md).

## What this changed in the plan (historical; superseded by current header)

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
