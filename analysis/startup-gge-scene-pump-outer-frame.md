# Scene async pump in the outer AppMode frame

## Authority and result

Selected x86 GA PE `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Preferred image VAs; short read-only `objdump` slices and PE constructor/table bytes.
No original frame, mode Update, async job, callback, resource service or save write executed.
Continuation of [scene queue/completion](startup-gge-scene-queue-completion-children.md).

**Observed (static):** constructed graphics host `0x0140F838 +0x18 = 7A4E50` calls its retained
scene manager at host `+0x28`, virtual `+0x38 = 7535A0`. The host is retained at AppMode
`+0x5C`; `7E9E80` invokes its `+0x18` at `7EA2DD`, **after** selected mode-driver Update
`7EA2AF`. This closes the outer-frame scheduling join into the recovered async pump.

**Not established:** a direct call from the body of ScenarioMode Update to the pump, native
frame execution, completed jobs/children/rendering, captain input or playable readiness.
The positive route is an AppMode-owned common service pass, not Scenario-specific activation.

```text
outer message/frame → F47945 → AppMode +0x78 = 7E9E80
  → selected driver +0x3C at 7EA2AF
      ScenarioMode if selected: EF2EF0 (independent loading/phase gates)
  → optional AppMode +0x3C service +0x14
  → AppMode +0x5C graphics host, virtual +0x18 at 7EA2DD
      constructed target 7A4E50
      → host +0x28 retained scene manager, virtual +0x38 at 7A4EF7
          constructed target 7535A0
          → each container → 752680 → 751E50(container +0x314)
          → active async slot, numeric status 7/8 → 7515D0
```

The existing [mode/frame audit](startup-mode-frame.md) supplies the outer caller and selected
driver contract. [Scene completion](startup-gge-scene-queue-completion-children.md) supplies
the async status, force/wait and saved-request replay boundaries after the new join.

## Host ownership, construction and initialization

`7E6FB4 → 7A68A0` allocates `0x194` and constructs via `7A60D0`; constructor instruction
`7A60FA` installs primary table `0x0140F838`. `7E6FCE` retains/stores the host at AppMode
`+0x5C`, releasing the prior value. This is separate from AppMode device/services at `+0x60`
and state manager `+0x34`. Nearby offsets must not be conflated.

Two distinct host initialization slots exist:

- host `+0x08 = 7A4D70`, called at `7E6FE7`: obtains/prepares another subsystem, checks its
  init result and publishes it only on true. This is not the later scene-manager construction;
- host `+0x0C = 7A7E00`, called at `7EAC24`: graphics-subsystem construction includes
  `74FEE0 → 74FE00`, retaining scene manager at host `+0x28` and publishing it through
  `67DF50` at `7A8003`.

The manager's constructed table is `0x0140D768`; its `+0x38` is `7535A0`. `7A4E50` uses
the **retained host field**, not a fresh `67DD40` getter. This explains why a narrow
"getter followed by +0x38" search did not identify the actual caller. Publication, retained
receiver and frame dispatch are now joined independently. Startup acceptance, allocation
success, alternate tables and live pointer replacement remain conditional.

## Ordering and arguments

The established AppMode frame computes raw seconds from its unsigned millisecond input,
and scaled seconds using its local time-factor/pause fields. In `7E9E80`:

```text
7EA290: selected-driver lookup through mode manager +0x2C
7EA292..7EA294: null driver → skip driver Update only
7EA2AF: selected driver +0x3C(scaledSeconds, rawSeconds)
7EA2B1..7EA2BD: optional AppMode +0x3C service +0x14
7EA2BF: load graphics host from AppMode +0x5C
7EA2C2..7EA2C4: null host → skip this service pass
7EA2DD: host +0x18(scaledSeconds, rawSeconds)
... later AppMode +0x60 work ...
```

The graphics pass neither tests driver-return success nor demands a non-null selected driver.
A skipped/early-returning Scenario Update is not equivalent to skipping this common pump in
the inspected flow. A hung/nonreturning call is outside this static model. Under selected
Scenario table `0x0148AFD8`, driver `+0x3C = EF2EF0`; its loading/phase gates remain separate.

`7A4E50` updates retained service fields `+0x24`, `+0x14`, `+0x20` first, each independently
null-gated. It then loads host `+0x28`, pops a saved register, null-checks that manager, and
repushes its second/first floats at `7A4EE0..7A4EF7` in the same order for manager `+0x38`.
Do not mistake the intervening `pop` for argument cleanup or swap the two scalar values.

`7535A0` walks registered containers and forwards the same values through `7535C5 → 752680`.
That method calls `751E50` **before** checking container byte `+0x1D`; the later byte gate
does not disable the pump prefix. Active-slot/request/status conditions still gate completion.
This is not unconditional `74A990` execution or a completed renderer/simulator frame.

## Search limitations, evidence and continuation

Expanded raw getter/slot proximity searches produced unrelated candidates. Decoded examples:
`EBC4AE` uses a manager lookup's result for a four-argument `+0x38` query; `F1C717` uses a
different mode/auxiliary service. Slot-byte equality alone does not establish receiver identity.
No exhaustive caller inventory or absence of another scheduling path is claimed.

Focused decoded ranges: `7E6F8E..7E7019`, `7A68A0..7A6905`, `7A60D0..7A611C`,
`7A4D70..7A4DD9`, `7EAC0C..7EAC25`, `7A7FCA..7A800A`, `7EA247..7EA2DF`,
`7A4E50..7A4EFC`, `7535A0..7535DC`, `752680..7526A1`, `EF2EF0..EF2F2D`, and
rejected candidate slices above. Calls, slots, retained fields and argument prefixes are pinned
in `tools/verify_startup_state_route.py`.

Next useful boundary: a concrete current-actor/input consumer, or a host service contract using
this scheduling join without faking async success. Revisit child/resource setup only for a
specific missing contract. Native launch delivery, rendered captain, input, playable gameplay
ticks and object-74 `Play Scenario 0x070D7509` delivery remain Unknown.
