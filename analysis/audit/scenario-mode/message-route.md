# ScenarioMode message route

Static GA evidence, reviewed 2026-10-18. `EF2250` is installed as the handler for message
`0x0212D3E7` by `EF2290`. Its relevant logic is:

```text
payload +0x08 == 0x01654C10 (ScenarioMode)
  and controller +0xCC == 0
    → EF1D20

payload +0x08 != 0x01654C10 AND != 0x00DBDBA1 (Editors)
  and controller +0xCC != 0
    → EF1380
```

This payload is the mode tag from the GameModeManager message route. The SDK `cScenarioMode` tag
and the GA registration tag agree. The route does not consume ControlID `0x070D7509`; the producer
that would turn the `Play Scenario` UI action into a state/mode request remains Unknown.

The handler returns handled=1 on the observed branches. Static instructions do not establish that
`EF1D20` ran in the original process or that its services/resources were available.

### Same event ID, separate global handler

The selected GA binary also registers the same event ID `0x0212D3E7` through a separate global
controller path:

```text
E81DFA
  → MessageManager/AddHandler(+0x28)
  → event 0x0212D3E7
  → handler E81C90

E82050
  → corresponding removal/teardown path
```

`E81C90` is not the ScenarioMode handler `EF2250`. Its bounded checks use adjacent mode-state
values `0x01654C00` at payload `+0x08` and `0x01654C08` at payload `+0x10`, then call its own
global-state helpers. It contains neither `0x01654C10` nor `0x070D7509`. Therefore the event ID is a
shared MessageManager channel with multiple consumers; its presence alone cannot join the GGE
`Play Scenario` control to ScenarioMode.

### Additional app-level consumer

The app-level handler at `0x007E7160` is another concrete consumer of the same event channel. Its
bounded dispatch is:

```text
event 0x0212D3E7
  → zero two app/controller float fields at +0xB4/+0xB8
  → handled = 1
```

The same handler has separate branches for `0x00F62ADD`, `0x0255ABF5` and `0xAE1CFE73`; on the
`0x00F62ADD` branch it performs a retry/update path and later forwards `0x00F62ADE`. It does not
compare `0x01654C10`, `0x070D7509` or `0x080D7DE1`. This is an app-lifecycle consumer, not a
ScenarioMode entry route.

### Correction: `54DF60` subscribes; it does not publish

The lifecycle method at `0x0054DF60` obtains its service through `883950`, then registers itself for
three event IDs through slot `+0x24`:

```text
service vtable +0x24(listener, 0x0212D3E7)
```

The neighboring IDs are `0x01EE100A` and `0x02319914`. `54E020` removes the registrations through
slot `+0x2C`, and `54E0B0` is the listener body. The earlier description as a shared-event producer
was incorrect: there is no message payload construction here. In particular, a listener pointer
is not the mode-change message payload. See the retained REA evidence below.

### Explicit `kMsgEnterMode` producer

The selected binary contains a concrete numeric-state notification producer at `ED57A0`. The
reference name `kMsgEnterMode` refers here to Scenario-internal modes, not selection of an
`IGameMode` tag:

```text
ED57A0
  → MessageManager service
  → event/tag 0x074BAF67 (kMsgEnterMode)
  → payload +0x08 = caller numeric state; +0x10 = previous object +0x1C
```

The same local UI/controller family invokes this producer at several bounded callsites. Its
ownership must not be assigned to GGE solely from these calls:

```text
ED628A → ED57A0(-1)
ED6429 → ED57A0(-1)
ED672A → ED57A0(2)
ED67F0 → ED57A0(1)
ED6836 → ED57A0(0)
```

Those callsites are lifecycle/state transitions and contain no literal `0x01654C10` or
`0x070D7509`. The producer therefore proves a numeric-state notification channel, not a GGE-side
`IGameMode` request or the semantic conversion `Play Scenario → ScenarioMode`.

### Dynamic-value source is bounded for this producer

The producer's dynamic value is the caller argument copied into object field `+0x1C`:

```text
ED57A0:
  argument → EDI
  if EDI != object +0x1C:
    publish kMsgEnterMode
    object +0x1C = EDI
```

Constructor `ED5130` initializes both `object +0x1C` and `object +0x20` to `-1`. A complete direct
caller inventory for `ED57A0` in the selected `.text` gives:

```text
ED628A → -1
ED6429 → -1
ED672A →  2
ED67F0 →  1
ED6836 →  0
```

No bounded direct caller supplies a resource/property-derived value or `0x01654C10`. Teardown
method `ED6910` consumes `+0x20`, falls back to `+0x1C` when it is `-1`, and passes the resulting
value to `ED6620`, whose local lifecycle dispatch is limited to branches `0`, `1` and `2`. This
closes the dynamic-source question for this producer: it is a small numeric lifecycle state
machine, not the missing Play Scenario-to-ScenarioMode tag conversion.

### Native SpaceGame → ScenarioMode callbacks: exact phases

A separate mode-transition coordinator has two helpers with the same mode-pair guard:

```text
current mode = 0x01654C05 (Game_Space / SpaceGame)
target mode  = 0x01654C10 (ScenarioMode)
  → pre-enter helper B33A80 → B335F0
  → post-enter helper B336E0 → B33650
```

These are distinct guards, not one continuous body: `B337E6..B337F4` belongs to `B336E0` and
selects `B33650` at `B338D2`; `B33BC6..B33BD8` belongs to `B33A80` and selects `B335F0` at
`B33C94`. `B335F0` performs ScenarioMode-specific simulator/service preparation:

```text
B335F0
  → B3D340 getter → B33E50(0x01654C10)
  → B3D410 getter → returned service vtable +0x14(0x01654C10)
  → additional simulator/service preparation
```

`B3D340` and `B3D410` are global getters, not mode lookups or message senders. The tag pushed
before the getter survives for the following call. `B33E50` switches a service and stores the tag
at service `+0x84`; it is not the mode registrar. The helper is a reaction to an already-selected
pair, not a producer selecting the target. The earlier unqualified "bootstrap" label is narrowed
to this simulator/service preparation.

### Recovered producer → pre-enter → OnEnter → post-enter

`7D8670` changes the selected table index at controller `+0x28`. Each table entry is `0x18` bytes:
object at `+0x00`, mode tag at `+0x04`. Its bounded order is:

```text
old table object vtable +0x1C()  [when the old index is valid]
select new index
883950() → message service
send 0x0212D3E7:
  payload +0x08 = new table entry tag
  payload +0x10 = previous table entry tag (zero if no valid old index)
new table object vtable +0x18()  [OnEnter]
send 0x022D1ADC with the same new/old tag pair
```

The producer does not need a literal ScenarioMode tag: both tags come from registered table
entries. `B63650` normalizes the two payload tags through `B5CAE0` (which preserves ScenarioMode),
checks its own guards, and routes unequal normalized tags as follows:

```text
0x0212D3E7 → B5F180(old, new) → B33A80(old, new)
0x022D1ADC → B5DCD0(old, new) → B336E0(old, new)
```

This joins the native tag-pair callback to a real mode-change producer. It does **not** identify
the upstream UI request that selects the new table index, nor observe a successful OnEnter.

### Scenario launch-record preparation candidate

`DFAD20` calls `EF1910`, which clears the shared record at `0x015ACCC8`, sets the default word at
`0x015ACCEC` to `1`, and returns the record pointer. `DFAD20` copies a three-word caller key to
record `+0x0C/+0x10/+0x14`, zeros the first three words and words `+0x18/+0x1C/+0x20`, then calls:

```text
B3D510() [global getter]
  → E3E350(1, 0x01654C10, 1)
```

`E3E350` stores the tag at receiver `+0x45C`, configures Scenario-specific loading UI/resources,
and sends `0x0490A868` on its show branch (`0x047D2B68` on hide). Thus this is launch-record and
loading-UI preparation, not by itself a GameModeManager switch. Instruction fallback identifies
`E00170`, key-type check `0x366A930D` and local controller state `5` as one caller of `DFAD20` at
`E001E9`. REA did not carve `E00170` in this ephemeral import; its empty direct-caller result for
`DFAD20` did not establish absence of that call. The upstream object-74 join remains Unknown.

Follow-up [launch completion audit](launch-state.md) recovers `DFC210 → DFBD40` after local
state-9/10 countdown, a concrete StateManager request for `ScenarioMode` state ID `0x21851EBE`,
and the matching launch-record consumer in `EF1D20`. Crucially, state 9 requests internal edit
mode 1; state 10 requests play mode 2. `DFAD20` alone is therefore not the playable launch path.
The generic name/tag table setters are recovered there, while object-74 action selection and
end-to-end queued command execution remain separate unknowns.

The current route boundary is:

```text
Play Scenario producer: Unknown
  → mode target 0x01654C10: still unresolved
  → registered-table selection: unresolved from this UI action
  → 7D8670 mode-change messages: recovered statically
  → B63650 → pair callbacks: recovered statically
  → B335F0 service preparation / native OnEnter slot: separate recovered edges
```

### Retained evidence and limits

[REA bundle](mode-transition-evidence.json) retains complete inline records, target SHA-256 and
provider limitations. Key observations:

- `ev_5f6077063e55491237367d3bee5b5f8d91ac5ff2056e393bedcc307d324d2246`:
  distinct `B336E0`, `B33A80`, `B335F0` bodies.
- `ev_244a28f402fff277586cb1b7469b7b0cce6b41ce4b9e5b7e14ecfe9fb4f8a419`:
  `B5F180`, `B33E50`, `7D8670` (one other selector failed).
- `ev_08a85ae838c922679c6b53861521be2fdc60189495b3b859ccd3b9f8a302fb45`:
  `B63650`; assembly confirms payload offsets and argument order where pseudocode has extra registers.
- `ev_cc3e6194c80af2c3812976ba5de51836413eb74a5d68a990c427823e0703fa7f` and
  `ev_1d92d45ff86f9b1391b0eaaaff3ad848ba35c2f15c5d8095274f0f06ca1fa677`:
  launch-record and loading-UI candidates; unresolved selectors remain errors, not recovered functions.
- `ev_9a02c9e93626d57e3a5085588858cc7d7c89dd26b09ea2f1df10785080b4a8b4`:
  subscription and numeric-state notification checks; `EF2250` was not carved in this import, so
  its retained earlier assembly/C extract remains the evidence for that handler.

All evidence is static. No original game was launched, no save was changed, and no host diagnostic
is promoted to native delivery or gameplay readiness.

### Reproduction

```powershell
python tools/verify_scenario_mode_route.py
```

Windows launcher: `tools/run_scenario_mode_route_verify.bat`. The verifier pins the full GA digest
and checks 53 authority/instruction/script assertions for table payload construction, call targets,
separate pair-guard phases, subscription slots, Editors exclusion, table setters and launch-state
record preparation/consumption.
It is a byte-regression test, not a runtime test or a general x86 decoder. A failed address guard
must be investigated rather than weakening the expected result. REA pseudocode can have incomplete
stack/register calling conventions; the instruction guards independently cover the key joins.
