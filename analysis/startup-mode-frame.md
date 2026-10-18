# Startup: mode selection, driver lifecycle and the GGE frame call

Authority: read-only short `objdump -d -M intel` ranges and file-backed PE bytes from the
selected GA-unpacked binary, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Selected slots/instructions are pinned by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
No native runtime, controller callback or save-writing path was executed.
Continuation of [startup-message-delivery.md](startup-message-delivery.md).

## 1. Same mode manager in startup, selection and frame lookup

AppMode initialization `7E8C40` calls `7D8E80` at `7E90F2`, retains the result at AppMode
`+0xC8` (`7E910F`), and publishes it with `67DEC0` at `7E9129`.
`67DCD0` returns that global, `015FD8B0`.

`7D8E80` allocates `0x34` bytes, installs primary vtable `01412608` and listener vtable
`014125F4` at manager `+4`, initializes active index `+0x28 = -1` and auxiliary owner
`+0x2C = 0`. Relevant primary slots are:

| slot | body | inspected operation |
|---|---|---|
| `+0x0C` | `7D8120` | create auxiliary owner and register command listeners |
| `+0x14` | `7D82B0` | loop through drivers, invoke driver `+0x0C` |
| `+0x18` | `7D82F0` | loop through drivers, invoke driver `+0x10(manager)` |
| `+0x20` | `7D9320` | register `(driver, tag, name)` |
| `+0x2C` | `7D8380` | return selected driver or null |
| `+0x40` | `7D8D40` | validate requested index and invoke selection |
| `+0x4C` | `7D8420` | select by registered name |
| `+0x50` | `967D90` | return auxiliary owner `[manager+0x2C]` |

**Important distinction:** manager `+0x50` does not return the selected driver. `+0x2C` does.
These are also separate objects from the state manager at AppMode `+0x34` and from the
app's own `+0x34` field.

## 2. Setup-all differs from activating-one

`7D9320` retains the registered driver, copies tag/name into a `0x18`-byte table entry and
appends it to manager `+0x14`. If manager byte `+0x10` is set, registration first calls the
driver's `+0x0C`; it does not invoke its `+0x18` activation callback.

`DDDE30` installs GGE vtable `0147D2B0` and registers it with mode tag `0x02CCD1D2` and name
`GalaxyGameEntry`. The **state** ID `0x02CCDA9C` is a different namespace.

The startup ordering has concrete call sites:

```text
AppMode +0x20 = 7EAAE0
  7EACB6: mode manager +0x0C = 7D8120
  7EACC7: manager +0x54 attaches AppMode+0x58 viewer to auxiliary owner

run F484E6: separate object virtual +0x18
  [receiver ownership/registrar body is not re-audited here]
run F484F3: mode manager +0x40(0)
run F4850A: state manager loads SporeAppStates
```

More importantly, the setup-all loops themselves are resolved: `7D82B0` walks table entries
at stride `0x18` and calls each driver `+0x0C`; `7D82F0` walks all entries and calls
`+0x10(manager)`. At `F484B8`, run calls the latter manager slot. This is **before** the
`SporeAppStates` load and does not mean that every registered mode is active.
The full producer/caller of the former loop is outside this focused pass.

For GGE, these slots resolve to `DDDBE0` (patch-manager preparation) and `DDDF80` (controller
setup). `DDDF80` allocates `0x274` bytes, calls `DE6B90`, stores the controller in `016A1344`
at `DDDFB0`, and links driver `+0x0C` into controller `+0xF0`.
This joins the previously recovered [GGE driver setup](galaxy-entry.md) to the manager's
setup-all loop; it is not evidence of completed GGE activation or resource readiness.

## 3. Delivered mode command reaches the selected driver's lifecycle

`7D8120` creates the listener at manager `+4`, vtable `014125F4`, callback `+4 = 7D8240`,
and subscribes it to `00D3C602`, `00E11332` and `00E11333` through message service `+0x20`.
As established in [startup-state-dispatch.md](startup-state-dispatch.md), `mode <name>`
produces `00E11333` with the name stored in the command payload.

`7D8240` recognizes `00E11333`, extracts the name and restores the primary manager by
subtracting four from `this`, then calls primary `+0x4C = 7D8420` at `7D8274`.
The latter searches names at table-entry `+0x08` (stride `0x18`); on match it calls
primary `+0x40 = 7D8D40`. A missing name returns false; there is no default-name fallback.
`7D8D40` rejects negative/out-of-range indexes, otherwise calls `7D8670`.

`7D8670` performs, in order:

1. Return immediately if the requested index equals active `+0x28`.
2. If the old index is valid, invoke old driver `+0x1C` and retain its mode tag.
3. Store the requested index in manager `+0x28` at `7D86E3`.
4. Through the service from `883950`, synchronously dispatch `0x0212D3E7` with
   new tag at payload `+0x08` and old tag at `+0x10`.
5. Invoke selected driver `+0x18` at `7D8766`.
6. Synchronously dispatch `0x022D1ADC` with the tag pair.

The two service getters `883950` and `67DC80` are linked to the same constructed message
service by the publication sites traced in [startup-message-delivery.md](startup-message-delivery.md).
Thus these are same-call notifications, unlike the queued state `mode` action that led here.
The pre-enter event cannot be renamed successful entry. **There is no test of the driver
activation return between `7D8766` and construction of the post-enter notification.**
Neither the selected index nor the emitted post-event proves resources/world/UI became ready.

For GGE, selected driver `+0x18 = DDD960` calls controller activation `DEA1B0` at `DDD99F`;
driver `+0x1C = DDD9E0` delegates deactivation to `DE8160`. These mappings were previously
inferred from side effects and are now joined to the concrete selection caller.

## 4. Selected-driver update in the outer frame

The [outer message/frame audit](startup-message-delivery.md) already pins
`F477D0 → F478F1` queue processing, followed by `F47945 → AppMode +0x78 = 7E9E80`.
The continuation inside AppMode is:

```text
7EA258: state manager +0x18             script-reload check, not driver update
7EA265: mode manager +0x50             auxiliary owner, not selected driver
  conditional auxiliary-owner +0x2C update at 7EA283
7EA290: mode manager +0x2C = 7D8380     selected driver lookup
  null → skip driver update
7EA2AF: selected driver +0x3C(two float arguments)
```

`7D8380` checks the active index against zero and table size; an invalid index returns null.
An in-range index loads entry word zero (the driver pointer). Thus frame dispatch follows the
same `+0x28` index written by `7D8670`, rather than a separate guessed global.

AppMode converts its unsigned millisecond input to seconds with float at `013F9458`, then
multiplies by the time factor from `+0x164`, or zero when counter `+0x160 > 0`. The driver
receives `(scaledSeconds, rawSeconds)`: instructions `7EA29F..AC` place raw at stack `+4`
and scaled at stack `+0`. This describes native argument preparation, not measured timing.

When the selected table entry is GGE:

```text
GGE +0x3C = DDDB80
  → DE8710(controller=[016A1344], same two floats)
  → B3D330() returns [0167EAC0]
  → B5EAE0(auxiliary manager, cvtss2si(rawSeconds * 1000.0))
```

`DE8710` is not just a mode marker: it invokes the current internal handler `[controller+0x38]`
virtual `+0x14` at `DE8785` with the first float, performs other service/UI work and calls
`DE6600` at `DE88BF`. Later it resolves a candidate internal handler from table `+0x124` via
`DE75E0`; if different from `+0x38`, it invokes old handler `+0x10`, stores the replacement,
invokes new handler `+0x0C`, then calls `DE44A0` and UI synchronization `DE48C0`.

Those controller handler slots are a **third lifecycle**, distinct from outer app states
and mode drivers. Existing [Intro](gge-intro.md) and [Play Spore/DBCheck](gge-dbcheck-play.md)
audits describe concrete internal handlers. This pass does not reinterpret the update call
as a playable game tick, nor claim all branches of `DE8710` are recovered.

## Result and remaining boundary

The startup traversal now connects queued `mode GalaxyGameEntry` → manager name selection →
selected-driver activation → outer frame → selected-driver update → GGE internal handler update.
Setup-all, activation-one, auxiliary-owner update and internal-controller dispatch are separate.

Still Unknown: successful native delivery/activation, runtime resources and service availability,
GGE object-74 Scenario action, stage/world creation and a playable gameplay tick. The next useful
step is to trace the current-handler setup/update handoff into GGE Intro/scene gates and identify
which startup-provided prerequisites those paths actually consume. See
[startup-gge-internal.md](startup-gge-internal.md). No host runtime code or readiness policy
changes in this pass.
