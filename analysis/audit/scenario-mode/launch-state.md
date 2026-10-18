# Scenario launch completion → named state → mode selection

Reviewed 2026-10-18. Static selected GA-unpacked binary only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No original process, state script, or save-writing code was executed.

## Recovered completion request

```text
DFAD20 sets local controller state 9 and countdown +0x28 = 10
  → E3E350(1, ScenarioMode tag, 1): loading presentation
  ... controller Update must run ...
DFC210: after its guards/countdown, local state 9 or 10
  → DFBD40: rebuild shared launch record
  → StateManager +0x28(0x21851EBE)
```

`0x21851EBE` is the lowercase FNV-1 state-name identity of `ScenarioMode`, not its mode tag
`0x01654C10`. The retained shipped [state script](../../scripts/SporeAppStates.txt) contains:

```text
state ScenarioMode
   entry
      mode ScenarioMode
   exit
end
```

This state has no explicit `-id`. The earlier [state identity recovery](../../states.md) distinguishes
name hashing from explicit IDs and records package provenance. This pass uses that retained extract;
it does not claim a fresh extraction or runtime script execution.

This identifies a native **state request**, not merely loading UI carrying a tag. It still does not
join `Play Scenario 0x070D7509` to the local controller's action/state selection, observe controller
Update, state-request acceptance, queued command delivery or successful mount.

## Launch record separates editing from playing

`DFBD40` calls `EF1910` again: the earlier preparation record is reset and rebuilt, not simply
carried unchanged into ScenarioMode. The completion branches write:

| Local controller state | Record `+0x24` / `0x015ACCEC` | Record `+0x28` / `0x015ACCF0` |
|---|---:|---:|
| 9 | 1 | 0 |
| 10 (other branch under the recovered Update guard) | 2 | 1 |

State 9 additionally copies a three-word key from controller `+0xB0/+0xB4/+0xB8` to record
`+0x0C/+0x10/+0x14`. Both branches copy the primary three-word key from `+0x8C/+0x90/+0x94`.
The other branch prepares an asset-key collection and a key at record `+0x18..+0x20`. The semantic
types of every key are not established by these copies alone.

`EF1D20` supplies the matching consumer:

```text
0x015ACCF0 (record +0x28) → ScenarioMode +0xD0
record base 0x015ACCC8 → F37DC0 and later Scenario services
0x015ACCEC (record +0x24) → EF11F0(requested internal mode)
```

`EF11F0` writes ScenarioMode `+0xCC`, with explicit 1→2 and 2→1 transition work. Existing Scenario
lifecycle evidence identifies `+0xCC == 1` as edit and `== 2` as play. `EF1D20` also has intermediate
mode writes, so the final `EF11F0` request must not be replaced by its first temporary assignment.

Consequently **DFAD20's state-9 route is editing preparation, not evidence of the playable route**.
Both routes select the same outer ScenarioMode state/tag; the record chooses the internal strategy.
The producer of local state 10 for the requested Play Scenario action remains Unknown. The exact
controller writer is now recovered through `DFE430 → DFAE20`, and the corresponding IDs are present
as serialized controller `WinButton` ControlIDs; what remains unresolved is whether object 74's
activation selects either controller button. The absence of a direct `0x070D7509` comparison or
conversion is not promoted to `Absent` because the native payload may be forwarded or dynamically
constructed.

## Adjacent controller event map

The nearby component-activation handler `DFE430` compares payload type
`0x287259F6` and dispatches selected component/event IDs to local controller helpers. The exact
bounded state writes in that dispatch are:

```text
0x05933FE0..0x05933FE3 → DF9E30 (not a direct state-9 write)
0x0733B57F             → state +0x20 = 9
0x05951103/0x05951104 → DFAE20 → state +0x20 = 10, +0x28 = 10, +0x80 = 1
0x0733FE08             → DFE1F0
0x0733FE09             → DFA430, then state +0x20 = 6
0x073BBA58             → state +0x20 = 5
0x074D0932             → state +0x20 = 7
```

This is positive evidence that local states 5–10 are event-driven controller phases. `DFAE20` is
the exact state-10 writer, but the inspected family contains no `0x070D7509` comparison. The
remaining question is whether event `0x05951103/04` is emitted by the selected-star Play Scenario
action or by another Scenario UI action; that upstream semantic join remains Unknown.

The IDs occur in a packed adjacent controller/state data region around `.data 0x015A4318`, together
with neighboring event identities such as `0x05933FE0..E3`, `0x073BBA58` and `0x074D0932`. The
table is read with indexed addressing by `DFAE60`/`DFA7E0`, which find these controls through
`810650` and add/remove the GGE `root +4` WinProc. An empty exact-address xref for an interior
table word does not exclude these indexed readers. Separately, the controller's UI-sync
helpers contain explicit uses of the same IDs: `DF9B00` resolves each through `810650` on the
controller layout and calls the resolved component's vtable `+0x7C(2, param)`, while `DF9F90`
resolves `0x05951103` and updates it through `+0x7C(1, condition)`. These are UI synchronization
paths, not proof of the click producer.

The shipped SPUI corpus independently contains exact `ControlID` matches in EP1 layout record
`13754`, instance `0x611E869D`: object `1375` is `0x05951103` and object `1580` is `0x05951104`,
both class `0xEEEFE2C3` (`WinButton`). The same `0x05951103` button also occurs in the PatchData
and Spore_Graphics copies of instance `0x43DD7F6B`. This establishes concrete serialized Scenario-
controller buttons, while their caption and the native click-to-event conversion remain Unknown.
It does not identify either button with GGE object 74 (`Play Scenario`, `0x070D7509`).

DFE430 still recognizes the pair through the bounded arithmetic branch
(`uStack_c + 0xFA6AEEFD < 2`), giving exact values `0x05951103/04`. The adjacent `0x0733FE08`
branch calls `DFE1F0`, not `DFAE20`; it is a separate UI action. The handler payload remains a
dynamic/component context, and the semantic join from object 74 to these buttons is not recovered.

## Controller state names and the narrowed UI boundary

The preceding adventure/captain selection and state-8 pending-key handling are mapped separately
in [selection flow](selection-flow.md). A selected key or an enabled host button is not a substitute
for the native wait/launch gates below.

`DFB580` formats the controller's local state using the shipped wide-string table. The relevant
entries are exact:

```text
state 9  → ReadyToLaunchScenarioForEdit
state 10 → ReadyToLaunchScenarioForPlay
```

Therefore `DFAE20` is not merely a numeric “playable” phase: it enters the controller state named
`ReadyToLaunchScenarioForPlay`, then the existing `DFBD40 → ScenarioMode` record path selects
internal play mode `2`. This is static native/UI evidence only; it does not observe a click, queued
message delivery, state acceptance, or successful launch.

## Scenario controller owner and initializer

Constructor `DFB990` establishes the primary vtable at `.rdata 0x0147EDA0`, not `0x0147EDCC`.
The latter is an interior address (primary `+0x2C`). The corrected primary slots are:

```text
+0x00 → 1082A20  +0x04 → B267D0   +0x08 → DFBD20
+0x0C → DFAE60   +0x10 → DFA7E0   +0x14 → DFC210
+0x18 → DFB010   +0x1C → E00630   +0x20 → A6C9A0
+0x24 → A6C990   +0x28 → A6C990   +0x2C → DFC6C0
+0x30 → DF97A0   +0x34 → E00280   +0x38 → E00610
+0x3C → DFB420   +0x40 → E00170   +0x44 → DE08B0
+0x48 → DFB440   +0x4C → DF9810   +0x50 → DFB400
+0x54 → DFB580   +0x58 → DFE430
```

The table is not being promoted to a C++ class name from an external SDK; this is the observed
shipped vtable/function relationship. `DFC6C0` is the initializer reached through primary `+0x2C`.
It requests layout name `GGENewScenariov3`, whose lowercase FNV-1 identity is `0x611E869D`, then
requests the layout through `812250` with group `0x40464100` and mount `0x05B598FA`.

`DFC6C0` creates four initial controller records with exact semantic labels and IDs:

```text
"choose adventure"     → 0x05933FE0
"choose captain name"  → 0x05933FE1
"choose captain name"  → 0x05933FE2
"choose posse"         → 0x05933FE3
```

The records are stored in a `0x24`-byte dynamic array through `DFB100`/`DFBFD0`; later UI-sync
helpers find/update the same layout's `0x05951103/04` buttons (not create them). This is a recovered owner
identity for the state-10 path: `DFE430` is part of the native `GGENewScenariov3` Scenario
controller, not an unrelated generic event consumer.

The serialized hierarchy provides a second, bounded owner cross-check in the same EP1 layout
record `13754`:

```text
initializer ID       object  parent   native label
0x05933FE0           914     958      choose adventure
0x05933FE1           1328    1344     choose captain name
0x05933FE2           1536    1552     choose captain name
0x05933FE3           1961    1975     choose posse

state-10 UI ID       object  parent
0x05951103           1375    1552
0x05951104           1580    1975
```

Thus `0x05951103` shares its serialized controller panel with the `choose captain name` record
`0x05933FE2`, and `0x05951104` shares its panel with `choose posse` record `0x05933FE3`. All six
objects are serialized `WinButton` instances with `CommandID = 0` in the bounded probe. This is
layout ownership/correlation evidence only; it does not establish that GGE object 74 selects one
of these panels or that a native click was delivered.

The initializer does not prove the missing upstream click conversion. Its virtual caller is now
resolved below in `DE7660`; this is static dispatch resolution, not observed execution.

### Constructor and owner registration

The controller constructor is now recovered at `DFB990`. Its observed initialization writes are:

```text
object +0x00 = 0x0147EDA0  (primary controller vtable)
object +0x08 = 0x0147ED9C  (secondary controller interface)
```

It then initializes the controller arrays, UI/resource fields, state flags, event/context pointers,
and three-element per-entry storage. The constructor's direct caller is `DE7660`.

`DE7660` is a simulator/controller registration routine. Under the observed feature guard
`6854E0(2)`, it allocates `0x178` bytes with heap tag `Simulator`, invokes `DFB990`, invokes the
new object's vtable `+0x2C` initialization slot, and publishes the resulting object through
registry slot/index `13`. This closes the native construction/registration edge:

```text
DE7660
  → allocate 0x178 (Simulator)
  → DFB990
  → controller vtables 0x0147EDA0 / 0x0147ED9C
  → primary +0x2C → DFC6C0 (layout initialization, not DFE430)
  → ordered controller map at GGE root +0x124, key 13
```

`DDE1B0` is a direct call instruction to `DE7660`, with ECX loaded from GGE global `0x016A1344`.
The provider does not carve that call site into a containing function; raw call decoding still
resolves the edge. The enclosing procedure's identity remains Unknown. Registration does not prove
that GGE object 74 activates the controller or that the native registry reached runtime readiness.

### Secondary controller interface

The secondary interface at object `+8` has a single observed slot at `.rdata 0x0147ED9C`:

```text
+0x00 → E00520   (this-adjustor thunk to E00170)
```

`E00520` subtracts `8` from ECX and jumps to `E00170`, restoring the complete-object receiver.
The following word at `0x0147EDA0` begins the primary table, not a second secondary slot. Earlier
reports incorrectly counted those adjacent primary methods as secondary-interface entries.

`E00170` is bounded as an internal controller message handler. It checks payload word `+0x04`
against `0x366A930D` and `0x2B978C46`, rebuilds local per-entry state, and dispatches based on
controller state `7`, `5` or `1` to `DFAC70`, `DFAD20` or `E00070`. The inspected body contains
neither `0x070D7509` nor ScenarioMode tag `0x01654C10`; it therefore narrows, but does not close,
the object-74-to-controller producer boundary.

### GGE state selection, update and UI forwarding

`DE75E0` is an ordered integer-key map lookup/insertion helper, not a fixed array-slot accessor.
`DE7660` supplies `GGE root +0x124` and key `13`. `DE8710` uses the same map with the current
GGE state at `root +0x108`. A separate guard overrides the selected key with `1` while
`root +0x1EC` is in `1..5`; this guard must not be bypassed.

On a selected-object change, `DE8710` calls old primary `+0x10`, stores the new pointer at
`root +0x38`, then calls new primary `+0x0C`. For key `13`, these are `DFA7E0` (exit/remove
WinProcs) and `DFAE60` (enter/add WinProcs). Earlier in the same Update body, an existing active
controller receives primary `+0x14`, resolving to `DFC210` for the Scenario controller.

`DE99B0` is invoked on the GGE WinProc interface, **root +4**. Therefore its receiver-relative
`+0x34` reads **root +0x38**, the active controller selected above. Its forwarding slot is
`+0x58`, resolving to `DFE430`. It is not the per-star marker/effect `DF6820` field merely
because both bodies mention a relative `+0x34`.
The state-13 writes at `DE9D11/17` likewise use interface-relative `+0x108/+0x104`, hence
root `+0x10C/+0x108` (requested/current GGE state), matching the `DE8710` selector.

```text
Galactic Adventures control 0x070D74E7 (not object 74)
  → DE99B0 guarded state-13 write
  → DE8710 map[13], unless the explicit key-1 override applies
  → active controller at GGE root +0x38
  → DFAE60 attaches GGE root+4 WinProc to Scenario layout controls
  → later DE99B0 forwards to active primary +0x58 = DFE430
  → component 0x05951103/04 → DFAE20
```

These are resolved static edges with normal entry/update/message guards retained. They do not
observe a native click, successful layout load, readiness, or game launch. The distinct object-74
`0x070D7509` action remains unjoined; no ID conversion to `0x070D74E7` or `0x05951103/04` is claimed.

## Direct table-index selection is generic, not the missing UI join

The mode table manager has two selection endpoints:

```text
7D8CF0  find entry by tag at entry +0x04 → index
7D8D40  validate index → 7D8670(index)
7D8420  find entry by name at entry +0x08 → index
```

`7D8670` publishes the mode-change pair around the selected table entry. These endpoints establish
how a mode index is selected once a command reaches the manager. They do not show that object 74's
ControlID is converted into a tag, a name, or local state 10.

## State script's `mode` verb and table-index setters

Focused instruction inspection resolves the generic selection machinery:

1. `0x0153F614` points to the literal `mode` at `0x01412504`. `7E1C10`'s registration sequence
   constructs a parser with vtable `0x0141332C`, first slot `7E4690`, and registers it under that
   string at `7E1EAE..7E1EC1`.
2. `7E4690` reads one argument, constructs command ID `0x00E11333`, stores the argument through
   `618D00`, and appends the command to one of the state command lists. It does not call OnEnter.
3. Manager listener `7D8240` recognizes that ID by successive subtraction. The by-name branch
   extracts the stored string and calls manager vtable `+0x4C`.
4. Manager vtable `0x01412608 +0x4C = 7D8420` searches entry names (stride `0x18`, name at
   `+0x08`) and calls manager `+0x40` with the matching index. Missing names return false.
5. `+0x40 = 7D8D40` validates the index before calling `7D8670`. Independently,
   `+0x28 = 7D8CF0` searches tags at entry `+0x04` and calls `7D8670` on a match; a miss returns
   false, not a fallback index. The earlier [mode-message route](message-route.md) then applies.

This supplies name-based and tag-based selection endpoints. The state command-list executor and
actual delivery are not re-established end-to-end here: script/registration, command producer,
listener branch and setters remain separate static observations, not a runtime trace. SDK names
`kMsgSetGameModeByName`/`SetActiveMode` are semantic corroboration only; addresses and calls above
come from the selected binary.

## Loading UI is not the state request

`E3E350` stores the target tag at receiver `+0x45C`; adjacent inspected readers select presentation
and animation behavior. No mode-table setter was recovered from those readers. The concrete switch
candidate is the independent `DFC210 → DFBD40 → StateManager` request. This is bounded to inspected
readers, not an assertion that the field can never influence selection elsewhere.

## Save-safety and evidence limits

`DFBD40` also calls `DE4800`. The latter names `GGEUserData.dat.tmp` and `GGEUserData.dat` and
includes writing/rename operations. **Do not execute this path as a harmless probe**: it can mutate
user data. This investigation was read-only static analysis.

[REA bundle](launch-state-evidence.json) retains five complete batch records, including errors:

- `ev_dd74616dac9899956838be34ee9cfda430d4818339f65ad69a019046de7f1107`:
  `DFBD40` record branches and StateManager call.
- `ev_fb57015ede476c060d0266dc7da8934cd7d3c0b973fe506772fb5ecbaf41a673`:
  `EF1D20` record consumer and `DE4800` save-writing boundary.
- `ev_016c3a2f0f33add0cc3c6d95316b318d5b46ccaa30bd280cad0bef6b847b7e38`:
  `EF11F0` internal mode setter.

This REA import did not carve the generic setters/ArgScript parser; those pseudocode requests remain
errors. Focused objdump instructions and the verifier cover the joins instead. Pseudocode may omit
implicit `this`/stack arguments; key offsets are checked against instructions.

Reproduction: `python tools/verify_scenario_mode_route.py` or
`tools/run_scenario_mode_route_verify.bat`. Guards include constructor-derived vtable slot arithmetic
and accepted/rejected state-10 IDs, not only byte equality. They do not prove execution, resource
availability, ScenarioPlanet creation or gameplay. See also [retained controller route evidence](controller-route-evidence.json).
