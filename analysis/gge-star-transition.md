# GGE show/phase cases — explicit guard, not native dispatch or latch

Reviewed **2026-10-15**. GA-unpacked SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Local Ghidra/objdump static evidence and actual SPUI tests; current-phase/caller inputs are
**controlled observations**, not original runtime reads. Analyst labels are not native function names.

## Outcome

`engine/gge_star_transition.*` plans `DF6D80` cases **2..5** with the equality guard against UI-data
`+0x2C`, ordered external requirements and explicit duplicate object choices.
`StarUiLifecycle::apply_phase_transition(ticket, inputs, error)` applies supported independent host
flags/names/local rectangle-copy and records a separate `report().transition`.
`StarUiScene::apply_phase_transition(ticket, entry_index, inputs, error)` routes an explicit request
to one card and retires old draw/pick views before a fresh projection publication.

No mouse/pointer/CommandID is automatically converted into a case, native current phase is never
inferred from host action success, opaque services are not called and no original phase latch is
written. Initial open/apply behavior is unchanged: phase1 hides panels; normal GGE has no live caller.

## Recovered case bodies and callers

Dispatch `0xDF7670` routes cases1..5 to `DF6FF9/DF6DAF/DF6E25/DF6EE3/DF6F3B`.
All listed writes use **mask1 (visible)**, not enabled mask2 or “show all”.

| Case | Inspected order after unequal-phase guard |
|---:|---|
| 2 | Opaque event `D06D9182`; hide `046A9CA7`; show `04656452`; show `05B31710`; assign phase+2C |
| 3 | Event `D06D9182`; show `046A9CA7`; service67CA60 slot4C `(1,window)`; assign phase+2C; entry names `02E712E4/046EA550`; show `05B31710` |
| 4 | Event `D2FC3AD5`; hide `04656452`; hide `05B31710`; **return1 WITHOUT assigning phase+2C** |
| 5 | Event `6C6DA889`; hide `04656452`; hide `046A9CA7`; show `03753F40`; if `05B31710` exists, copy `05B34D93` slot34→target60 then show; assign phase via inspected paths |

**Correction:** case4's successful return is not a phase4 latch. Case3 also writes its latch before
the name/show tail. All normal returns1 remain distinct from native readiness/dependency validation.

`DF7BF0` is the controller **F8 route**: only when +FC is null and +F8 changes, it restores old case1,
then chooses **saved→3 / candidate→2** for a new entry with UI data, projects it and stores +F8.
`DF7AC0` is the **FC route**: on a changed entry it performs other controller/services work, optionally
restores old case1, stores the new +FC pointer and chooses **saved→5 / candidate→4** only when its
explicit byte argument is true. Full hover/selection/input-pump/notification scheduling is not
implemented, and no host hit is promoted into those original pointer fields or predicates.

`436350/436390` use service getter `A20670` (global `0166D9F4`): the first samples slot20; the
second invokes slots38/40/58 when the service exists. Payload and purpose are not guessed as audio,
telemetry or successful no-op. Host outcome is **Unsupported**, not an observed absent manager.
`67CA60` returns global `015FCC4C`; its slot4C request is likewise Unsupported, not assumed focus.

## Rectangle-copy correction

WindowBase table `0x1415328` maps slot34→`95F950`, slot60→`95FDA0`, slot64→`95FE20`.
The getter returns interface-local+94; the setter compares/copies **four float rectangle values**
and notifies via slot94; the two-coordinate mover preserves extents then calls slot60. Case5's
source34→target60 operation is therefore not by itself a layered-object relation.

Host `copy_local_rect(target,source)` literally copies decoded local coordinates **and size**, then
refreshes inherited placement/visibility, without converting between source/target parent spaces.
It does not execute native notifications or rewrite serialized Objects. Missing objects, collapsed/
nonfinite geometry and writing the positioning root fail atomically as host safety guards, not native
crash/skip behavior. The phase1 `80DE40` compound operation remains separately Unsupported; its
layering semantics are not completed by this four-float finding.

## Explicit guard/report contract

```cpp
StarTransitionInputs request;
request.requested_case = 2;
request.observed_current_case = 1; // only when explicitly observed; nullopt means Unknown
request.choices = {{0x046A9CA7, 90}};
card.apply_phase_transition(card.report().ticket, request, error);
// Inspect transition->plan.guard, blocked_on, ordered requests/results; true is NOT native-ready.
```

- Unknown current produces a blocked `UnknownCurrent` plan, no body actions. Observed0 is a value;
  raw word equality also handles unusual observed current values without inventing enum semantics.
- Equal current/requested produces `AlreadyCurrent`, no actions even if host windows look different.
  It is not a corrective ensure-visible fallback.
- Unequal produces `CaseBody`. Only requested2..5 are supported; case1 has the separate planner.
  Invalid cases/choices fail transactionally as **host API scope**, not an emulated native false return.
- All candidates stay in the plan. `046A9CA7` object87 is nested under object90; selecting87 does
  not reveal ancestor90. No first-match, class or hidden/visible heuristic is used.
- Supported flag/name/rect actions apply independently in plan order. Missing/Ambiguous bindings
  and Unsupported services remain explicit. Later visual actions may run despite unsupported earlier
  services: **host policy**, not original dependency/exception/optional-control skip scheduling.
- The supplied current-phase observation never advances. Repeated calls with an unequal observation
  may repeat host writes. Neither phase1 pass success nor visible controls infer a native latch.
- Lifecycle/scene generation, catalogue and busy guards apply. Accepted scene requests retire old
  views, even blocked/equal plans; invalid target/ticket/request leaves the prior valid frame intact.
  Another explicit camera update is required before drawing/picking changed visibility.

Native services/mount/scene readiness functions remain false; there is no original callback/listener,
selection side effect, MoviePlayer/DBCheck completion or launch dispatch.

## Verification

`gge_star_transition_verify`: **195 checks PASS**, both original 152-object/58-window layouts.

- Unknown/equal/unequal/raw0/rawFFFFFFFF guards, exact case2..5 order/event/flag/name requirements.
- Case2 panel41 becomes hittable from the **untouched original template** after explicit phase1
  host actions plus the requested transition. Root177 IgnoreMouse stays **unchanged (flags19)**;
  other panels remain hidden. No earlier controlled root-bit picking workaround is needed here.
- Case3 saved panel/names and Unsupported service; case4 narrow hides; case5 panel176 and
  literal source95→target28 rectangle before its show.
- Duplicate/hidden-ancestor selection, case/choice/target bounds, failed-plan atomicity, stale/closed
  lifecycles, scene/current-frame invalidation and unavailable catalogue protection.
- Clearly controlled missing-control/duplicate-rectangle-source variants exercise Missing/Ambiguous
  outcomes and explicit source choices; these variants are not claimed as additional original layouts.
- Scene republish yields a real panel hit/object identity, not automatic native dispatch. Raw template
  isolation and rectangle geometry failures are asserted. Existing lifecycle232/scene204 reentry tests
  also cover new transition methods without inflated check counts.

Camera/catalogue/current-phase observations are controlled, not a live save/catalogue/native camera.
No new GPU/full-card/galaxy screenshot is claimed; these assertions cover actual layout state/picking.

```powershell
cmake --build build --config Debug --target OpenSpore gge_star_transition_verify
.\build\Debug\gge_star_transition_verify.exe <SPUI-fixtures>
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --transition
python tools/gge_evidence.py --static --transition
```

Evidence: [facts/corpus](audit/gge-star-transition/facts.json), ten functions (C/instructions),
[case bodies](audit/gge-star-transition/cases2-to5.asm.txt), [F8 caller](audit/gge-star-transition/hover-caller.asm.txt),
[FC caller](audit/gge-star-transition/selected-caller.asm.txt), [rectangle setter](audit/gge-star-transition/rectangle-setters.asm.txt),
[aligned seeds](audit/gge-transition-seeds.tsv). REA MCP was absent; existing local Ghidra/objdump
was reused without config changes or target execution. These are not REA Evidence IDs.

Next: real input/callback routes and opaque services, native current-phase/active-camera producer,
explicit genuine catalogue integration and GPU card validation. Automatic GGE, native factories/
effects, data discovery and gameplay remain unavailable rather than synthesized.
The [input follow-up](gge-star-input.md) now ports only explicit world-ray distance selection; UI
window hits, world hits and raw callbacks remain separate and neither dispatches native routes.

Final Debug x86 warning-free. Transition195 and all prior scene204/lifecycle232/streaming95/
AnimatedIcon159/image80/phase1/instance/projection/parser/catalogue/DBCheck/Intro/SPUI/package
suites PASS; existing controlled image/frame/scissor GPU readbacks PASS. Six Python tests,
58 maintained Markdown link checks and generated dashboard --check PASS. Launcher/Main → GGE/
visible launcher → CreatureEditor smoke runs exit0, aborts/asserts0; MoviePlayer remains Unknown.
New launcher capture is RGB pixel-identical to `analysis/shots/spui_e3launch_uv.png` (diff bbox=None).
Those smoke runs do not automatically invoke the new transition API or claim a rendered native card.
