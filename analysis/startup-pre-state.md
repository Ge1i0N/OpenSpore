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

The nearby mixed ASCII/UTF-16 data contains the following concrete strings and option fragments:

```text
Spore(TM) Galactic Adventures
../Data/
../EP1Data/       ep1
../BP1Data/       bp1
sinclude "   (ASCII prefix; localCheats.txt is a separate UTF-16 string)
localCheats.txt
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
  → state-manager virtual +0x1C = 7E2370, instance 0xD6396913
      → trigger resource SporeAppStates
  → timing/config values and state dispatch
  → main loop
```

The `SporeAppStates` push at `F48505` supplies **instance** `0xD6396913`, not a group ID;
the actual indirect call is at `F4850A`. Group is zero. `F47C20` does not parse the trigger text.
The concrete consumer is now traced in [startup-state-registration.md](startup-state-registration.md).

## State-list load call and post-load handoff

At `F48505`, the object returned by startup getter `67DCE0` is called through virtual slot
`+0x1C = 7E2370` with instance `0xD6396913`. This is the concrete `SporeAppStates` load
path; it is a **different object/vtable from AppMode**. The surrounding sequence is:

```text
state manager +0x1C(0xD6396913, 0, launchArgs, 1)
  → 847780(parser, resourceKey)
  → on success: initial-state name lookup and switch request
return to run
  → 6A17E0(0x2A, 0)
  → 571E60()
  → 93A3E0() clock/timing conversions
  → continue AppMode startup dispatch
```

The resource-manager path at `847780` obtains the manager through `8DE4B0`, calls manager virtual
`+0x58` with the resource descriptor, and on success constructs/returns a loaded resource object.
This is the generic trigger/resource load boundary used by startup. `930C20` is a bounded UTF-16
copy/path helper used while building startup descriptors; it is not evidence of parsing
`SporeAppStates` into state records.

The manager constructor, parser registration, resource-load/parser invocation, state-record
commit and initial-state selection are now linked statically in
[startup-state-registration.md](startup-state-registration.md). The full lexer/line-dispatch
implementation and downstream execution of every state action are not claimed complete.

`571E60` is a `QueryPerformanceCounter` wrapper (IAT `013CC2B8`) returning the counter in
`EDX:EAX`. `93A3E0` returns the two words at `0154EAE8/0154EAEC`; it does not itself read the
clock. `6A17E0` stores an indexed byte-valued flag in a dword array (`+0x38/+0x3C`), extending it
on demand. The meaning of index `0x2A` remains Unknown; none of these is a state registrar.

The shipped ArgScript extraction and `Main → GalaxyGameEntry` data are already available, but
they must not be treated as runtime parser observation.

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
+0x24 = 0x7E6520   +0x28 = 0x7E9520
+0x2C..+0x3C = 0x7E5F20..0x7E5F70   (counters/timing fields)
+0x40 = 0x7E6680   +0x44 = 0x7E66E0
+0x48 = 0xE310C0   +0x5C = 0x7E9DD0
```

The `+0x24` slot performs AppMode teardown/cleanup and emits the hashed app events
`0x00F62ADD`, `0xAE1CFE73`, `0x0255ABF5` and `0x0212D3E7`. **Correction:** `+0x48 = E310C0`
is only `xor eax,eax; ret` (constant zero); the field test previously attributed to it starts at
the different function `E310D0`. The earlier dispose label for `+0x5C` is withdrawn pending a
focused body audit. The table entries alone do not establish those methods' semantics or exclude
all downstream Scenario effects.

`F47C20` is called at `F48054`; `F48054` continues with the AppMode vtable operations and later
the `F48505` trigger load. This closes the ordering question without claiming that the trigger
parser itself is inside `F47C20`.
