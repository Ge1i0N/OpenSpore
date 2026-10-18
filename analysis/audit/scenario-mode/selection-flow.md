# Scenario controller selection and pending-key flow

Static selected GA-unpacked authority only, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No native process, resource request or user-save writer was executed.

This continues the [GGE controller routing](launch-state.md), not an implementation of gameplay.
The source is the [retained REA bundle](selection-flow-evidence.json), corroborated by bounded
objdump instructions and `tools/verify_scenario_mode_route.py`.

## Four records, two navigation directions

`DFC6C0` builds a vector at controller `+0x0C/+0x10/+0x14` with stride `0x24`.
Each initialized record contains panel ID at `+0`, forward control at `+4`, back control at `+8`,
and local controller state at `+0x20`. The native diagnostic labels are preserved verbatim;
they are not automatically captions on the buttons.

| Record | Panel | Forward control | Back control | State / DFB580 name | Native record label |
|---:|---:|---:|---:|---|---|
| 0 | `0x07BFB2A0` | `0x05384D05` | `0x05933FE0` | 1 / SelectingScenarioForPlay | choose adventure |
| 1 | `0x07BFB2A1` | `0x05384D06` | `0x05933FE1` | 2 / SelectingAvatarForPlay | choose captain name |
| 2 | `0x07BFB2A2` | `0x05384D07` | `0x05933FE2` | 3 / NamingAvatar | choose captain name |
| 3 | `0x07BFB2A3` | `0x05384D08` | `0x05933FE3` | 4 / SelectingPosse | choose posse |

`DFE430` routes the forward range to `DF9EC0` and the back range to `DF9E30`.
Neither range means “write local state 9”. Both read the record selected by controller `+0x2C`:

- Forward: increment index while another record exists; copy record `+0x20` into controller
  `+0x20`; hide panels with `DF97B0`, then select the record's panel through `DF9C50`.
- Forward skips state 2 when `+0x7C == 0` and state 4 when `+0x7D == 0`, bounded by the vector end.
- Back: index zero goes to `DF9AA0`/Neutral; otherwise decrement, copy record state and select
  its panel. It recursively skips state 2 when `+0x7C == 0`. At state 1 it can also go back again
  when service `67DCC0 +0x54` is false and GGE root `+0x1F8 == 0`.

The forward bounds divide vector bytes by `0x24`; the record-state addressing is
`base + index * 0x24 + 0x20`. The four initialized records are not all twelve controller states.

## Starting/resetting the selection flow

The known `0x0733FE08 → DFE1F0` branch resets navigation index `+0x2C`, entry count `+0x30`,
entry selection `+0x34`, several keys and name storage, and reads the first record's state/panel.
It disables the navigation controls through `DF9B00(0)` and marks UI synchronization dirty (`+0x7E`).

A guarded alternative inside this reset helper calls `DFDA20` to obtain a candidate three-word key,
then primary `+0x40 = E00170` and `DF9EC0` when the key is nonzero. The query includes a
`0x366A930D` filter. Candidate availability and native resource-service success remain unobserved.
This does not prove reset is called unconditionally on the first state-13 entry.

## Selected adventure is not a launch request

In `E00170`, a nonempty supplied key with second word `0x366A930D` dispatches by local state:
state 1 calls `E00070`, state 5 calls `DFAD20`, and state 7 calls `DFAC70`.
The `0x2B978C46` key family follows `DF9CF0` instead. These are key/payload word comparisons,
not replacements for UI ControlIDs.

`E00070` first calls service `67DC90` vtable `+0x0C`. Only on success does it copy the supplied
three-word key into pending storage `+0x98/+0x9C/+0xA0` and call `EC5A10(key,0,0)`.

| Branch | Observed effect |
|---|---|
| Initial service call false | No promotion to selected key in this body |
| `EC5A10` false | Clear pending key |
| `EC5A10` true, `EC42D0` false | Copy pending to selected `+0x8C/+0x90/+0x94`, clear pending, call `DFFD70` |
| `EC5A10` true, `EC42D0` true | `DFAAF0(0)`, save local state into `+0x24`, write state 8 |

`DFB580` names state 8 **WaitingForAutoPollinate**. This names the waiting state, not proof of
what every resource-service result means. Do not invent a synchronous success fallback.

## Wait completion has a separate internal-message route

`DE8A80` runs on GGE root `+8`: its receiver `+0x30` is active root `+0x38`, and forwarding
primary `+0x34` resolves to Scenario `E00280` (primary table base `0x0147EDA0`). This differs
from UI forwarding `DE99B0 → primary +0x58 = DFE430`.

`E00280` recognizes `0xCC0BF723` only for the relevant wait handling when local state is 8 and
payload is non-null. For numeric status 2 it copies pending to selected, clears pending, and
restores `+0x24` into local state. Restored state 1 calls `DFFD70` and updates the UI; restored
state 9 has a separate loading-presentation/record path.

Statuses 1 and 3 take a different branch: for saved state 1 they restore state 1; for saved
state 9 they clear selected key and return to Neutral; pending is cleared for those handled cases.
Other status values do not follow the status-2 promotion. These numbers are observed branch
identities; semantic success/failure enum names and actual notification delivery remain Unknown.

## Gates feeding the launch/navigation controls

`DF9F90` computes a value sent to `DF9B00`:

- state 1: selected first word `+0x8C != 0`;
- state 2: first word at `+0xA4 != 0`;
- state 3 with `+0x7F != 0`: name span `+0x168 != +0x16C`;
- other cases: starts true;
- if `EC42D0` is true: force false.

`DF9B00` applies this value with window slot `+0x7C(2,value)` to forward controls
`0x05384D05..07` and launch controls `0x05951103/04`. The `..08` forward ID is not in this
five-element update list; do not silently extend it. Three auxiliary controls get `+0x7C(1,value)`.
Separately, `0x05951103` receives `+0x7C(1,+0x7D == 0)` in both navigation/UI-sync paths.
These are exact slot/argument observations, not a claim of complete UTFWin flag semantics.

After obtaining selected data, `DFFD70` calls `DFFA00` and reads a temporary structure:
one byte branches into writes to `+0x7C/+0x7F`, key `+0xA4..+0xAC` and name storage;
the count of its `0x38`-byte records becomes controller `+0x30`, with `+0x7D = (count != 0)`.
The records are copied to controller `+0xC0` onward. Exact types/meaning of all temporary fields
remain unclassified; their existence must not be synthesized from host preview metadata.

## Remaining boundary and evidence

The explicit launch control branch is still `0x05951103/04 → DFAE20 → state 10`, with its own
guards and later readiness/countdown work. No selection helper inspected here proves native
launch completion, asset mounts, ScenarioPlanet creation or the object-74 action `0x070D7509`.

REA records:

- `ev_a061524154d31f75df116196dad331524231fd1b4259fd15bac51c30089b95ca`:
  navigation, reset, pending/selected keys and selected-data reader.
- `ev_8cf4b876fa8305e8c8b3e04e88224af9e9db322dfddb17c5676d89930c924ec3`:
  panel selection, UI synchronization and candidate query.
- `ev_1fd8adceea57d4cf2acca8b54dd3a5024f6798e6e8fc3d83a2ed9ac16a6d9655`:
  entry-related notification producer and UI gate conditions.
- `ev_157aa0158327be77edd967383e79bafe9935918f8a0176aee3531dfefb9037e0`:
  reset and bounded decompiler errors for `DE8A80`/`E00170`; raw instructions cover those edges.
- `ev_404985bf6b51a036eb63d798a4f47d6da704e37b494feba4ae1c274d6b2bee32`:
  adjacent selection/query helpers, without inferring unavailable query results.
