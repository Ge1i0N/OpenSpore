# ScenarioMode pre-play phases and the ScenarioPlanet lighting correction

Static selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native process, resource request or save writer was executed.

The [controller launch gates](launch-gates.md) end at a state request. This audit starts inside
ScenarioMode; selecting the outer mode is not evidence that a playable world exists.
Evidence: [preplay-evidence.json](preplay-evidence.json), plus authority-pinned instruction guards.
External SDK contracts are semantic corroboration only, not imported runtime implementation.

## A separate loading context, not mMode

`EF24C0` (OnEnter) sets ScenarioMode `+0xDC = 1`, then explicitly loads
`ECX = 0x015ACC30` before calling `EF1BC0`. The latter resets context `+4 = 0`,
`+8/+0x0C = -1`, and `+0x10 = 0`, and collects dependencies from Scenario services.
These offsets belong to that global loading context, not the ScenarioMode instance.

In `EF2EF0`, after service `B3D420 +0x28` permits Update:

```text
loading-context phase at 0x015ACC34 != 4:
    increment 0x015ACC40 if already nonzero
    ECX = 0x015ACC30
    EF2930()
    return
phase == 4:
    proceed to the separate enter latch and later mode-specific Update
```

The phase is distinct from ScenarioMode `+0xCC` (edit/play mode), `+0xD0` (launch context),
the GGE controller's local states and its `+0x78` launch subphases. Identical small integers do
not join those state machines.

## Four pre-play phase actions, each returning after advancement

`EF2930` dispatches phases `0..3` via jump table `0x00EF2A28`. Its receiver is the loading
context at `0x015ACC30`:

| Phase | Observed guard/action |
|---:|---|
| 0 | On first visit initializes a permission byte through service/root state, calls `8014A0`, sets counter `+0x10 = 1`. Advances when `(permission && counter >= u32[0x015ACD1C]) || counter > 30`; calls `EF2860`, `EF0140`, then increments phase. |
| 1 | Waits for service `401010` virtual `+0x28` result **equal to zero**, then increments phase. This differs from launch-controller's signed `<= 0` guard. |
| 2 | Waits for `EF01C0` to return true; calls `EF1B60`, increments phase. |
| 3 | Calls `EF25E0`, then root/service-dependent finishing work (`EF0AA0`, `ED5310`, `8014C0`, or the alternate branch), increments phase to 4. |

The artifact initializes `0x015ACD1C = 4`; this is an initial value, not proof it cannot be changed.
The `counter > 30` fallback advances only phase 0. It does **not** bypass phase 1's count or
phase 2's model checks. Each advancement returns from `EF2930`; phase 0 does not fall through
all other phases in the same Update.

`EF0140` iterates a separate three-word-key vector at context `+0x4C/+0x50`, stride `0x0C`.
For keys not accepted by service `401010 +0x18`, it submits a descriptor with ID `0x2EA8FB98`
through service `+0x4C`. The result and semantic type of every submitted key are not established.

### Exact model predicate at phase 2

`EF01C0` iterates context `+0x60/+0x64`, a vector of object pointers. For every object it calls
virtual `+0xAC`, then checks the returned pointer and its flags word at `+4`:

```text
pointer != null
flags bit 14 is set
flags bit 18 is clear
```

If any element fails, it returns false immediately. The empty vector returns true. This is exact
machine behavior, **not** permission to substitute an Unknown/missing dependency vector with an
observed empty one. Names of those model-flag bits remain unclassified here.

`EF25E0` includes mode-dependent object/spawn work and `F40210/F45E70` calls. This audit does not
label every branch successful world creation; their inputs/results still need separate recovery.

## First post-loading Update is initialization, not the ordinary play tick

Once the global phase equals 4, the enter latch `ScenarioMode +0xDC` is examined. If nonzero,
`EF2EF0` clears it, performs loading/UI-related work, then chooses:

```text
+0xCC == 2 → EF0E00 (play-side setup)
otherwise  → EF0990(0) (other/edit-side setup)
```

It then performs the lighting operation below and returns. Ordinary mode-2 Update runs on a
later call with phase 4 and latch clear. Its static calls include `F1F960`, `EF0D00`, optional
`F117C0`, `EF2EB0`, and additional simulator/goal/display helpers. Invocation of those helpers in
the artifact is not observed stage readiness, successful actor spawn or playable input.

## ScenarioPlanet string is a lighting configuration selection

Earlier notes described this as a `ScenarioPlanet` simulator resource/planet request. That was
unsupported. The actual call at `EF2FB7..EF2FD9` is:

```text
receiver = global[0x0167EA54]
configID = fnv1_lower("ScenarioPlanet") = 0x24720859
receiver virtual +0x30(configID)
return from Update
```

The selected native object chain is recovered, not merely inferred from the string:

1. `67DD50` returns manager global `0x015FD8D0`; setter `67DF60` publishes that manager.
   Graphics initialization `7A7E00` obtains it through `70A0B0 → 709CB0`.
2. `709CB0` installs manager vtable `0x0140C75C`; slot `+0x18 = 70DD00` is the world factory.
3. `B3AD20` calls this manager slot with world ID `0x04A4DEA0` and stores the result at
   global `0x0167EA54` (with retain/release ownership).
4. `70DD00` allocates an aligned `0x670`-byte `Graphics` object, constructs through `70D330`,
   and registers it. Constructor `70D330` installs vtable `0x0140C7A0`.
5. World slot `+0x30 = 70A5D0`: for a changed nonzero ID it calls `705B60`, stores selected
   config pointer at world `+0x10` and ID at `+0x14`, then invokes `709530`. Zero clears the
   selection; an unchanged ID skips re-selection.
6. `705B60` searches manager configuration entries with stride `0x144` and compares ID at the
   referenced record's `+8`. Miss returns null. No fallback planet construction occurs here.

The SDK independently names `67DD50` `ILightingManager::Get`, `0x0167EA54` `LightingWorld`,
manager `+0x18` `CreateLightingWorld` and world `+0x30` `SetLightingState`. Its configuration
contract references lighting properties group `0x40200100`. Those semantic names fit the GA
factory/config-selection chain. This pass does not prove that config payload was mounted/read
successfully, nor does the hash `0x24720859` alone establish a simulator planet.

## Remaining dependencies

- Actual results of the loading key submissions, count service and model predicates.
- Meaning/population of every dependency vector and both model-flag bits.
- World/terrain/actor creation and the mode-dependent `EF25E0` setup outcomes.
- Native first play tick, input, stage/goal initialization and gameplay.

The lighting correction removes a false planet-creation claim; it does not remove those blockers.
Host preview, readiness policies and user saves are unchanged.

Reproduction: `tools/run_scenario_mode_route_verify.bat` verifies the selected binary, dispatch
table, loading-context receiver, model predicate bits, one-shot return and graphics factory/slot
chain. `tools/run_scenario_launch_gate_verify.bat` also runs the separate countdown regressions.
Both are read-only checks, not original-runtime test modes.

Key REA records:

- `ev_171a978c663f8bb32b47d460d398fae3d7ab5cfc5099db9ce4fe546f0142a9fa`:
  loading-context phases and enter-side helpers.
- `ev_f8374179f201f9eaafe7d3f2ed99c72d2f68b063f1eeed84aa178e5152cf1fb4`:
  `EF0140/EF01C0`, phase finishing and Update.
- `ev_a49d36225e7566a2be24ecc2acb47d2275f825201e7d66f7a4975951e11d4cf9`:
  graphics-world global xrefs.
- `ev_27c93e820e07b217d27bdbd1af08badf5dcf8486cd58cfe678b9b4e09a57bb81`:
  manager getter and native world-global publication.
- `ev_28063e7512b075b83dc5ab46d38d2e848e109e7e95ca56c4591687382fdfc6ea`:
  graphics manager creation/publication.
- `ev_16a0369a18ff3f3cafb642d780c80caa2bde1313bf7d0cec84befded5a106b66`,
  `ev_188544dc42c2869f1775bbb5f845d0ba75d63a9dd96dbe7cd03f5363ef458a95`,
  `ev_e19d5cc1270bc55823b8f0cc9a5284ac45df5434408e39947deeb14db04fff51`:
  concrete manager/world constructors and factory.
- `ev_bf4764391b8d4f93afafe74b3cd958b27318a8d378387650b74f31daf234e734`:
  configuration lookup; `70A5D0` remains explicitly uncarved in decompiler output and is byte-guarded.
