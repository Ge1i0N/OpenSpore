# Pre-state-list startup data-root setup

Static analysis of the selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native runtime was launched and no save/resource writer was executed.

This is the small startup boundary between [AppRun](startup.md) and the later
`SporeAppStates` load. It is intentionally separate from ScenarioMode and from host preview
policy.

## `F47C20`: data/config-root setup

`F47C20` is called from `F48054` before the main state-list handoff. Its first guard checks the
startup option `shipDirs` (`UTF-16` text at `0x013F8DB4`); when present it skips the dev/data-root
setup branch and reaches the later cleanup/return path.

When the branch is active, the function:

1. checks the option keys represented by the UTF-16 table near `0x0148D7B0`;
2. builds temporary three-word descriptors and range objects through `41E010`, `688CA0`,
   `930C20`, `579A90` and `688490`;
3. uses the configured data roots and releases temporary ranges with the ordinary allocator
   release path;
4. returns to `F48054`, which continues the AppMode setup and state-list loading.

The exact semantic names of each temporary descriptor and the service identities are not promoted
beyond their native writes. This is startup preparation, not proof that any Scenario resource is
mounted successfully.

## Shipped option/root strings

The nearby UTF-16 data contains the following concrete strings and option fragments:

```text
Spore(TM) Galactic Adventures
../Data/
../EP1Data/       ep1
../BP1Data/       bp1
include "localCheats.txt"
isHost
clientIP
hostIP
UI/MainWin
mouse
automation
Spore_DDFList.txt
```

The short option strings `ep1`, `bp1`, `shipDirs`, `devDirs`, `hostIP`, `clientIP`, `isHost`,
`mouse` and `automation` are not equivalent to Scenario/GGE state IDs. They select startup/data
configuration or runtime connection options.

## Ordering relative to `SporeAppStates`

The relevant ordering is:

```text
AppRun / AppMode setup
  → F48054
      → F47C20 data/config-root setup
  → AppMode virtual +0x1C load with instance 0xD6396913
      → trigger resource SporeAppStates
  → timing/config values and state dispatch
  → main loop
```

The `SporeAppStates` call at `F48505` supplies instance/group identity `0xD6396913`; this audit
does not claim that `F47C20` parses the trigger text. The trigger-resource query/parser boundary
remains the separate item documented in [content-loading.md](content-loading.md).

## Current conclusion

This startup pass adds a concrete data-root/config handoff before `Main → GalaxyGameEntry`. It
does not alter any ScenarioMode readiness conclusion and does not connect object 74's
`Play Scenario` control to a native state request. The unresolved producer and runtime delivery
boundaries remain unchanged.

## AppMode state-host slots around the handoff

The AppMode vtable at `0x01413B50` also makes the startup state-host boundary explicit. The
already documented initialization slots are `+0x14 = 0x7E6850`, `+0x18 = 0x7E93D0`,
`+0x1C = 0x7E7430` and `+0x20 = 0x7EAAE0`. Nearby slots include:

```text
+0x24 = 0x7E6520   ++0x28 = 0x7E9520
+0x2C..+0x3C = 0x7E5F20..0x7E5F70   (counters/timing fields)
+0x40 = 0x7E6680   ++0x44 = 0x7E66E0
+0x48 = 0xE310C0   ++0x5C = 0x7E9DD0
```

The `+0x24` slot performs AppMode teardown/cleanup and emits the hashed app events
`0x00F62ADD`, `0xAE1CFE73`, `0x0255ABF5` and `0x0212D3E7`. The `+0x48` slot is a small boolean
test over AppMode `+0x68`, and `+0x5C` is the AppMode dispose/cleanup entry. These slots are
startup lifecycle infrastructure, not alternate Scenario launch producers.

`F47C20` is called at `F48054`; `F48054` continues with the AppMode vtable operations and later
the `F48505` trigger load. This closes the ordering question without claiming that the trigger
parser itself is inside `F47C20`.
