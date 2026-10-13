# The app state machine — RECOVERED FROM DATA (session 13)

**This file corrects a wrong conclusion.** For three sessions we recorded that the game's state names
were "data we do not have", because `SporeAppStates` (`0xD6396913`) appeared nowhere as a resource
GROUP. It is an **INSTANCE** id, in group 0. The search was looking at the wrong field. See
"Lessons" at the bottom — this is the same class of mistake as the dead `Locale/` search path, found
the same day.

## Source

`trigger` resource (type `0x024A0E52`), instance `0xD6396913` = `SporeAppStates`, present in three
packages, all RefPack-compressed plain text:

| package | record | packed | decoded |
|---|---|---|---|
| `Spore_Game.package` | 9655 | 2,094 B | 8,202 B |
| `PatchData.package` | 9272 | 2,094 B | 8,202 B |
| `Spore_EP1_Data.package` | 7663 | 2,155 B | **8,499 B** (newest, kept) |

Extracted verbatim to `analysis/scripts/SporeAppStates.txt`; parsed to
`analysis/scripts/SporeAppStates.json` by the same grammar as `analysis/config-scripts.md` (this is
the game's ArgScript language, the same one `ConfigManager.txt` uses).

The file's own header, which is the clearest statement of intent we could ask for:

```
# State script for Spore game
#
# This basically defines the different parts of the game,
# and what has to be turned on or off as we move between
# those parts.
#
# Note: if you run with the -demo flag, you'll start up
# with the Demo state. Otherwise, you can run with
# -state:<stateName> to get a specific state.
```

So the user's `-state:SpaceAccessoriesEditor` detail is **confirmed by the shipped script itself**,
including the colon.

## Shape of the language

```
state <Name> [-id <0xHEX|dec>]
   description "<text>"
   mode <ModeName>
   gotoState <OtherState>
   entry
      mode <M>
      cheat "<command line>"
      effect <name>
      message -id $kVar [-data1 (hash(<name>))] [-data2 ...]
      prop <...>
   exit
      <same verbs>
   end
seti kSomeId 0x...          # module-level id constants, referenced as $kSomeId
```

`-id` is **explicitly assigned** for the states the UI addresses by number ("assign manual IDs just
so I can fill in the UI layout stuff more easily", the script's own comment), and auto-hashed
otherwise. This mirrors the Properties.txt lesson: **take the id from the file, never hash it.**

## The recovered machine: 60 states, 17 modes

**Entry chain** (this is the Main→playable path, in the game's own words):

```
Main  (-id 0, description "Spore Launch Screen")
  gotoState GalaxyGameEntry  (-id 0x02ccda9c, mode GalaxyGameEntry)
```

`-demo` instead enters `Demo` (description "Spore EAP3 Demo", `cheat "prop demo true"`) which also
goes to `GalaxyGameEntry`. `-preload` enters the empty `Preload`. There is a commented-out
`# gotoState DevLauncher` in `Main` — the dev launcher is the alternate boot path, and `LaunchScreen`
exists only as "backwards compat. for sniffs" forwarding to `DevLauncher`.

**The five game stages**, each a real `mode`, each with `From*` forwarding states:

| stage state | mode | entered from |
|---|---|---|
| `CellGame` | `Game_Cell` | LaunchScreen, LoadGame, LevelEditor |
| `CreatureGame` | `Game_Creature` | LaunchScreen, LoadGame, LevelEditor, **CellGame** |
| `TribeGame` | `Game_Tribe` | LaunchScreen, LoadGame, LevelEditor, **CreatureGame** |
| `CivilisationGame` | `Game_Civ` | LaunchScreen, LoadGame, LevelEditor, **TribeGame** |
| `SpaceGame` | `Game_Space` | LaunchScreen, LoadGame, LevelEditor, **CivilisationGame** |

So the progression Cell → Creature → Tribe → Civ → Space is explicit data, and every stage can also be
entered cold from the launch screen or a save (`*FromLoadGame`). The inter-stage transitions carry
messages: `kMessageCellToCreatureTransition 0x02A4f8f0`,
`kMessageCreatureToTribeTransition 0x234e60a`.

**The 13 editors**, all sharing `mode Editors` and switching by message
(`kMessageSwitchEditor 0x022d308b`, `-data1 (hash(<setup name>))`):

`PlannerThumbnailGen 0x734444b0`, `CellEditor 0x022d9e00`, `CellToCreatureEditor 0x0588b78b`,
`CreatureEditor 0x022d9e01`, `BuildingEditor 0x022d9e02`, `VehicleEditor 0x022d9e03`,
`VehicleLandEditor 0x052b3270`, `VehicleAirEditor 0x052b3271`, `VehicleWaterEditor 0x052b3272`,
`UFOEditor 0xf12178d3`, `FloraEditor 0x022d9e05`, `TribalAccessoriesEditor 0x022d9e06`,
`AdventureAccessoriesEditor 0x07429067`. Plus `TerrainEditor 0x022d9e10` (its own mode).

Note the editor id block `0x022d9e00..0x022d9e10` — manually assigned, contiguous, and again evidence
that these ids are allocated in runs, exactly like the `locale` label keys
(`0x045BEDD1 Save / D4 Cancel / D8 Exit / D9 Quit`).

**The user's example resolves**: `TribalAccessoriesEditor -id 0x022d9e06` carries
`message -id $kMessageSwitchEditor -data1 (hash(SpaceAccessoriesEditorSetup))`. `SpaceAccessories…`
is the *setup resource* name; the *state* name is `TribalAccessoriesEditor`. So
`-state:SpaceAccessoriesEditor` as remembered is close-but-not-exact — the state is addressed by the
state name, and the accessories editor is split Tribal / Adventure.

**Viewers and test harnesses**: `EffectsViewer`, `ModelViewer`, `AnimViewer 0x679d892f`,
`MultiverseJournal 0x036af998`, `ScenarioMode`, `LevelEditor (mode Game_EditMode)` with four
`LevelEditorFrom*Game` forwarders, `LoadGame`, `BasicLauncher`, `GameCinematicsMode`, and four
`SniffTest*` states that run `tester -run TestScript …` cheats.

## The lead this hands us for `spui`

`DevLauncher` contains the actual command that loads a UI layout:

```
state DevLauncher -id 1
   entry
      mode EffectsViewer            # just to get a black background.
      cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"
      effect  I_splashscreen
      effect UI_splashscreen_start
   exit
      cheat "uilayout -unload E3LaunchScreen"
end
```

So the SPUI entry point is the **`uilayout` console verb** with `-load <name> <id1> <id2>` /
`-unload <name>`. Two ids follow the layout name — a (group, instance) or (layout, atlas) pair, which
is precisely the ambiguity the `spui` reader has to settle. `E3LaunchScreen` is a real named layout
(the `spui` screen list has `LaunchScreen`, `GameLaunchScreen`), and the ids are concrete handles we
can look up in the package indexes. That is a much better door into the layout format than guessing
field order from bytes.

## Lessons (why this mattered more than the state list)

1. **"The data does not exist" is a hypothesis, not a result.** We had recorded the missing state
   names as a fact in `RESUME.md` and `MEMORY.md`. The real finding was that a DBPF record has
   *three* id fields (type, group, instance) and I had probed only one of them.
2. **Enumerate before you conclude.** The fix was not a cleverer guess; it was dumping *all* 303
   `trigger` resources with their group and instance names and reading the list. That took one script
   and immediately produced `SporeAppStates`, `CSAAppStates`, `Startup`, `UIMachineScenarioMode`,
   `TriggerConfigScenarioMode`, `OptionProperties`, `ConfigManager` — several of which we had also
   written off as absent.
3. The same day, the `Locale/` search path turned out to be dead for a similar reason (we looked in
   the wrong place and reported the data missing). Both were caught by re-asking "where did I look?"
   rather than "what did I fail to find?".
