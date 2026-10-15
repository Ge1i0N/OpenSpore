# GGE Intro handler — post-title guards, not galaxy readiness

Reviewed **2026-10-15**. Target: GA-unpacked, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
[Retained evidence](audit/gge-intro/tables.json): 23 Ghidra function/instruction extracts plus bounded
objdump for MoviePlayer, DBCheck and internal state 10. No original game was executed.

## Correction / identity

The shared controller handler constructed at `0xDEA4B0`, installed for states 0/1/8/9 by `0xDE7660`,
is named **`Intro`** (wide name at `0x147E234`). Its vtable is `0x147E1D8`:

| Slot | Target | Observed role |
|---:|---|---|
| +0x08 | `0xDEA7E0` | destructor |
| +0x0C | `0xDEB450` | enter: subscribe five events, start title when controller +0x1EC==1 |
| +0x10 | `0xDEA3D0` | exit: unsubscribe and show zoom layout |
| +0x14 | `0xDEAF70` | update: title step, MoviePlayer/phase guard, DBCheck latch, post-intro call |
| +0x18 | `0xDEA5B0` | key handling, includes Escape branches |
| +0x34 | `0xDEA820` | message handler, including DB notification |

These are the actual controller-call slots (+0x0C enter, +0x10 exit, +0x14 update), not the outer
AppMode or outer AppState callbacks. Never equate controller phase/state integers with script IDs.

## Title path

`0xDEB060` loads `SporeTitleSeq` through `0x812250` with mount operand `0x05B598FA`. It builds six
element timing/control records, checks a `zh-cn` locale branch and chooses element index 3 or 5.
The index-5 branch immediately calls `0xDEABD0`, which releases title UI (`0xDEA510`), sets controller
**+0x1EC = 6**, and sends `0x066F8F25`. Therefore "initial handler" does not mean "galaxy loaded".
Other branches animate title elements in `0xDEAC80`; they are not implemented here.

Handler entry also calls `GlobalUI::show(false)` (`0xE01320(0)`) and hides zoom UI. The previously
added primary GlobalUI frame is a **diagnostic view**, not the original visibility lifecycle.

## Exact post-title predicate and latches

`0xDEAF70` first runs title update, then samples controller phase and calls the object obtained by
`0x67CAD0` at vtable **+0x14**. Its identity is traced through host setup:

```text
0xF4838C → 0xFD8120 → 0xFD7B10 (0x330-byte MoviePlayer, vtable 0x1493A50)
         → 0x67CBD0 stores global 0x15FCC68
0x67CAD0 returns 0x15FCC68
vtable +0x14 = 0xFD7550: return [this+0x34] != 0
```

The body at `0xFD7550` is `xor eax,eax / cmp [ecx+34],eax / setne al / ret`.
It is **not** a package-inventory readiness check.
Movie-service identification is also supported by `0xFD90D0` (same vtable +0x04): it registers
the `movie` console verb and constructs graphics/audio/movie-stream helpers. The MoviePlayer name
here is a semantic role, not a recovered GA class symbol.

Only when **phase == 6 and MoviePlayer is idle**:

1. If Intro +0x11 is clear, obtain/create `UI/DBCheck` (`0x60B6B0`, ctor `0x60B510`, setup at
   DBCheck vtable +0x10), call DBCheck +0x18 (`0x60B5D0`), then set +0x11.
2. This is an `else if`: even a synchronously delivered event cannot run step 3 in the same update.
3. On a later update, if Intro +0x12 is set and +0x0D is clear, call `0xDE9550`, then set +0x0D.

`0xDEA820` latches Intro +0x12 on **message `0x08085C4C`**, independently of +0x11. It does not
examine a success payload for this message. Calling it a guaranteed "DB successfully loaded" event
would overstate the evidence.

## DBCheck is not our package scanner

`0x60B5D0` checks the manager from `0x67CB00` (global `0x15FCC74`) and its +0x1C return. If it
exists and that return is false, DBCheck constructs a UI object, loads a layout descriptor with
instance `0x4B0AC091`, and locates control `0x0431E538`; dialog/event behavior remains to reverse.

The **missing-manager or true-return branch posts `0x08085C4C` immediately**. The layout-load-failure
path at `0x60B340` also posts that same notification. Therefore the event is a continuation signal,
not evidence that every database operation succeeded. We do not synthesize it from `748/748` SPUI
or `57/57` DBPF counts, and no real content-manager consumer is implemented in this pass.

## What follows the gate

`0xDE9550` has additional message/UI/effects work and, unless requested state is already 10, calls
`0xDE8680(10,1)`. That function maps current internal states 0/1/8/9 to 2, pushes the return-state
stack and sets requested state to 10. This is **not** `gotoState CreatureGame` or another outer state.
State-10 enter (`0xDEC320`) hides a zoom control, starts effect `0x4C4BA0A9` through `0xDE5380`, and
hides GlobalUI; its update (`0xDEC3B0`) waits for additional flags before `0xDE5280` pops a return
state (fallback 2). These dependencies are not executed by the current model.

## Implemented subset and explicit host policies

`engine/gge_intro.{h,cpp}` models **only the post-title decision guard and one-shot request latches**.
It returns `StartDbCheck` / `RunPostIntro` requests, not successful side-effect completion. It does not
implement MoviePlayer, DBCheck, messages, title animation or internal state-10 entry.

- Movie observation is three-valued: Unknown / Idle / Playing. **Unknown is a host safety guard**;
  GA expects an existing service and does not have that enum.
- Runtime on_tick probes this bounded model with phase 6 and **Unknown** observation. This is an
  explicitly chosen post-title model input, **not** an observation that the real title ran or phase
  6 was reached by a reconstructed controller. It never auto-advances.
- No synthetic DB completion event is sent by the runtime. Unit tests alone inject controlled
  observations/notifications to exercise every predicate.
- Enter/leave reset a fresh host model. This is not a claim that the original shared handler resets
  every latch on every reentry (its entry body only explicitly clears +0x0D).
- UI remains the labelled diagnostic primary-layout view; the sparse hidden branches are not
  forced visible. The new wait reason is printed on screen and logged only when it changes.

This is **partial behavior**, not a playable GGE or a completed mode implementation.

## Verification

`gge_intro_verify` covers inactive/unknown/playing states, phase==6 specificity, 1,000 idle updates
without a completion event, early synchronous notification, one-shot ordering, duplicate events and
host enter/leave reset. Controlled signals are not original-runtime observations. Other corpus,
drawable, scissor and visible launcher transition regressions remain in the standard suite.

```powershell
cmake --build build --config Debug --target OpenSpore gge_intro_verify
.\build\Debug\gge_intro_verify.exe
refs/ghidra/venv/Scripts/python.exe tools/gge_evidence.py --intro
python tools/gge_evidence.py --static --intro
```

**Next useful implementation boundary:** define the actual MoviePlayer/no-movie host service contract,
then the DBCheck missing-manager/UI branches with explicit dependency handling. Follow internal
state 2's controller (constructed at `0xE008B0`) and state-10 return/effect lifecycle for the first
real galaxy-scene step; do not replace these dependencies with an unconditional readiness boolean.

**Follow-up:** [DBCheck executor and Play Spore state](gge-dbcheck-play.md) now retain those branch
bodies, real dialog corpus/root and a tested explicit backend subset. State 2 is named “Play Spore”
and instantiates per-game UI/effects, not just a hidden SPUI branch. The live adapter is still absent;
default GGE remains blocked on Unknown movie observation, not auto-advanced by controlled tests.
