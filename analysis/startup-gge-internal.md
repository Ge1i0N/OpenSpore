# Startup: GGE internal handler table, Intro gate and Play Spore boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no original runtime, save writer or controller callback was
executed. Selected facts are pinned by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
This continues [startup-mode-frame.md](startup-mode-frame.md) and the existing
[Intro](gge-intro.md) / [DBCheck–Play Spore](gge-dbcheck-play.md) audits.

## GGE activation creates an internal handler table

GGE driver activation `DDD960` calls controller activation `DEA1B0`. The controller is created
by `DDDF80` through `DE6B90` and stored at global `016A1344`.

`DE7660` is the controller's internal-handler setup. It allocates/constructs handlers and stores
them in a table rooted at controller `+0x124`, using these **internal numeric keys**:

| key | constructor | vtable / role |
|---:|---|---|
| `0/1/8/9` | `DEA4B0` | `Intro`, vtable `0147E1D8` |
| `2` | `E008B0` | `Play Spore`, vtable `0147EF50` |
| `3` | `DE2CB0` | separate internal handler |
| `4/10/11/12/13` | `DE3320` | separate internal handlers |
| `5/6/7` | `E00600` | separate internal handlers |

The exact semantic names for handlers other than Intro and Play Spore remain bounded or Unknown.
These numeric keys are not outer `AppState` IDs and not script mode tags.

## First active handler can be Intro while controller state is still 0

`DE6B90` initializes controller `+0x108 = 0` and `+0x10C = 9`. `DE7660` then builds the
handler table. The transition logic at `DE8680` maps requested/current values `1`, `0`, `8`
and `9` to internal handler key `2` in the return-state stack, but this is not an immediate
claim that the outer numeric state became 2.

`DE8A20` selects a handler from the table according to controller `+0x1EC`; `DE8A33` stores
the selected object at controller `+0x38`, calls the old handler `+0x10` and the new handler
`+0x0C`, then calls `DE44A0` and `DE48C0`.

The startup path sets controller `+0x1EC = 1` at `DDE1BB` after `DE7660` returns. `DEB450`
then calls selected handler `+0x0C` and, when that phase is 1, starts the title path through
`DEB060`. This is why the initial GGE internal handler is Intro even though the controller's
other state fields still contain startup values. It is not evidence that the galaxy catalogue
or world has loaded.

## Intro enter/update gate

`DEA4B0` constructs Intro and clears its local one-shot bytes. Its vtable confirms:

```text
+0x0C = DEB450    enter
+0x10 = DEA3D0    exit
+0x14 = DEAF70    update
+0x34 = DEA820    message handler
```

`DEB450` removes old registrations, subscribes the Intro listener to its message IDs, clears
Intro `+0x0D`, and if controller `+0x1EC == 1` invokes `DEB060`. The title path `DEABD0` can
release title UI, set controller `+0x1EC = 6` and send event `0x066F8F25`; this is a title-phase
transition, not a state-manager `gotoState`.

`DEAF70` first calls title animation/update `DEAC80`. It then samples controller phase and
calls MoviePlayer query `67CAD0 → vtable +0x14 = FD7550`:

```text
FD7550: return ([MoviePlayer+0x34] != 0)
```

Only when phase is exactly `6` and that query is false does it enter DBCheck:

1. If Intro `+0x11` is clear, create/setup DBCheck `60B6B0/60B510`, call `60B5D0`, then set
   Intro `+0x11`.
2. On a later update, if Intro `+0x12` is set and `+0x0D` is clear, call `DE9550`, then set
   `+0x0D`.

`DEA820` sets Intro `+0x12` on message `0x08085C4C`. `60B5D0` posts this message on its
missing-manager or manager-true branch and also has a UI-load-failure path that posts it.
The event is a continuation signal, not proof that every database operation succeeded.

The startup MoviePlayer construction is visible in `F4838C → FD8120`; `FD8120` allocates a
`0x330`-byte object and jumps to `FD7B10`. Its existence/idle result remains a runtime
dependency: static construction does not establish that the service was successfully mounted
or idle during a real Intro update.

## Post-Intro request and Play Spore handler

`DE9550` performs additional event/UI work and, unless requested state `+0x10C == 10`, calls
`DE8680(10,1)`. This is an **internal GGE state transition**, not `gotoState CreatureGame`
and not proof of outer ScenarioMode activation.

The handler for internal key `2` is constructed by `E008B0`; its name method `+0x54 = E00930`
returns `Play Spore`. Its enter/update slots are:

```text
+0x0C = DF9620    enter
+0x10 = DF85B0    exit
+0x14 = DF8700    update
+0x38 = DF9740    state-2 UI/rebuild path
+0x68 = DF6010    per-record projection/update
```

`DF9740` gates on internal state `2`, shows the relevant control/GlobalUI, rebuilds the game
record array through `DF95A0`, then continues to `DF6C10`. `DF6010` projects each live game
record through `DE3C20` and forwards it to per-record UI data. These are the existing static
boundaries documented in `gge-dbcheck-play.md`; no live game records, viewer, camera or native
layout attachment are observed here.

## Current conclusion

The startup-to-GGE internal route is now bounded as:

```text
mode driver activation
  → GGE controller creation
  → DE7660 internal handler table
  → controller +1EC = 1
  → Intro enter/title
  → phase 6 + idle MoviePlayer gate
  → DBCheck continuation signal
  → DE9550 → internal requested state 10
  → later internal handler selection / Play Spore state 2
  → live-record rebuild and projected per-star UI path
```

The arrows after the Intro gate are conditional static branches. The state-2 catalogue boundary
is expanded in [startup-gge-play-spore.md](startup-gge-play-spore.md). Unknown remains explicit for
MoviePlayer runtime state, DBCheck manager availability, message delivery, record availability,
camera/viewer production, native UI attachment, Scenario launch and playable world ticks.
