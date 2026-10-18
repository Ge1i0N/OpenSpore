# Play Scenario: source/parent dispatch, manager callback and serialized flags

Static selected GA-unpacked authority, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No original process, input action or save-writing path was executed.

## Manager callback is not the parent window handler

The `+0xC8` field read by `959240`, `9595A0` and `95A010` is relative to the **window manager**
receiver, not to a window or the GGE controller. Raw constructor `958C50` installs manager vtable
`0x01440868` and clears that field. Slot `+0x20 = 957EC0` replaces the callback with add-ref /
release ownership; no direct E8 caller of that setter was found in the bounded scan.

The callback's virtual `+0x0C` receives phase codes: `95A010` sends `(3,0,message)` before
dispatch, and `959240` sends code `4` for a handled case or `5` after exhaustion. Its return
value is not used as the handled result in these bodies. This is consistent with a routing
observer/hook, not a recovered fallback action handler. It could still have side effects or
modify the supplied record; its concrete runtime instance is Unknown. Do not declare it inert.

Earlier descriptions of a "window +0xC8/owner fallback" conflated manager and window coordinates.
The actual parent propagation is a separate path below.

## Source-only and parent/default phases differ

`95A010` snapshots seven dwords of the supplied message, sets source to outer window `+4`,
and destination to source-parent outer `+0x38` plus `4` (or null). It calls `959240(message,1)`.

1. `959240` calls `958950(sourceOuter,message,1)`. This walks the source's WinProc list.
2. `958880` calls each matching WinProc virtual `+0x18`. Mask matching is retained, not bypassed.
   A false result continues the list; a true result stops that list.
3. The final argument `1` in this source phase **suppresses outer virtual `+0x20`**, even if the
   source's WinProcs did not handle the record.
4. If source dispatch does not stop routing, `959240` starts at destination parent and iterates
   outer `+0x38` links, calling `958950(parentOuter,message,0)`.
5. In this parent phase, after unmatched/unhandled WinProcs, `958880` can call the parent's
   outer virtual `+0x20`. This is the real window-level default handler.

The next WinProc identity/order is dynamic. `AddWinProc 961F10` uses priority and event masks;
knowing one attached GGE receiver is not a full list snapshot. No native source/parent list was
captured for object 74.

## Plain WinButton defaults do not launch ScenarioMode

Constructor `966EB0` installs outer WinButton vtable `0x01441390`. Its virtual `+0x20 = 950B10`
is a different dispatcher from `InteractiveWinProc::HandleUIMessage 950E50`:

| Message type | Plain window `950B10` | Interactive WinProc `950E50` |
|---|---|---|
| `0x17` | outer `+0x70` | proc `+0x68` |
| `0x18` | outer `+0x74` | proc `+0x6C` |
| `0x287259F6` | default `960370` | default false in the inspected generic body |

For the plain WinButton table, `+0x70 = 950E00` and `+0x74 = 951190`, both false-return stubs.
`960370` handles only the inspected types `0x0D/0x14/0x16`; component activation reaches its
false default. None of these defaults converts `0x070D7509` to the GA tab ID or Scenario launch.
This applies to that concrete table, not to all derived windows or all parent WinProcs.

## Shipped object 74 is locally hidden, object 87 is locally visible

The explicit EP1 SPUI record `12415`, instance `0x28F6D535`, has decoded payload length `28910`,
SHA-256 `49e24564e56fca7f3104082dc5b38814a2ff3857bc9084193bdfa47cebe408a1`.
The [retained probe report](zoom-button-flags.json) preserves both separate objects:

| Field | Object 74 | Object 87 |
|---|---|---|
| Class | `0xEEEFE2C3` WinButton | `0xEEEFE2C3` WinButton |
| Caption | inline Play Scenario | locale ref resolving Galactic Adventures |
| ControlID | `0x070D7509` | `0x070D74E7` |
| CommandID | `0` | `0` |
| WindowFlags | `0x0000000A` | `0x0000000B` |
| Own visible bit `0x01` | false | true |
| ButtonType | `3` | `3` |
| ButtonGroupID | `2` | `2` |
| Parent | 138 | 114 (whose parent is 138) |
| Serialized WinProcs property | absent | absent |

The shipped designer/SDK semantics name `0x01` Visible and ButtonType `3` Radio. Native getters
and setters corroborate the receiver fields, but no runtime visibility was observed. In particular:

- These are **serialized local flags**, not effective visibility through ancestors and not proof
  the native game never shows object 74. A runtime visibility writer remains possible/Unknown.
- Revealing all hidden controls in a host diagnostic is not proof that Play Scenario is the normal
  playable entry. The distinct GA control's state-13 route is the recovered native candidate.
- `9640B0` selects ordinary type `0x18` when loaded button type at outer `+0x214` is `2` or `3`.
  Its `0x287259F6` activation message is built separately, before that notification. Do not
  assume every WinButton sends `0x17` or that a `0x18` notification alone launches gameplay.
- Radio grouping does not establish ID conversion between the two controls.

`DE48C0` updates button-state flags via IButton query `0x8ED27E7A` and `+0x28` masks
`4/2/8/0x10`; this is not an observed `IWindow::SetFlag(Visible,true)` for object 74. Do not
reinterpret button-state updates as evidence of runtime visibility.

## Reproduction and limits

```powershell
python tools/gge_control_id_probe.py --root build/bin/Debug --id 0x070d7509 --id 0x070d74e7
python -m unittest tools.tests.test_gge_control_id_probe -v
tools/run_gge_control_id_probe_verify.bat
tools/run_scenario_mode_route_verify.bat
```

The probe parses shipped SPUI with copies separate, retaining class, payload digest, local flags,
button type/group and absent versus empty WinProcs. Its report covers 748 parsed layouts with no
layout parse failures; five unsupported packages remain explicitly skipped, not silently absent.

[REA evidence](fallback-evidence.json) retains successful and failed requests. Key records:

- `ev_678333e2dbda9f8933dbfd264d2308aa089fd0f98a69ba152a58d55aed81b45d`:
  source/parent routing, WinProc walk and generic producer.
- `ev_bf224a6c6661b5359de10c1fd8dc8dc5d64268645e60da4facbb38b9ac7a6d34`:
  plain window dispatcher versus WinProc walk.
- `ev_c262f1f1ccd4662df2df19a519f101ca80ebed450f677ace7ac917ed32af5202`:
  false-return defaults and explicit uncarved setter request.
- `ev_4324251ef1f0ac594e20981028dc855de56a7495a3a72fb7fe1a72676aa8fd7b`:
  GGE button-state sync, attachment and InteractiveWinProc dispatch.
- `ev_3c0723f4b26bbda24b70175ee872a53023f5dcabd335b762d345e6f1103f7624`:
  plain WinButton constructor and generic notification split.

Raw instructions cover the uncarved manager setter/constructor. Unknowns remain: native list
instances/order, callback identity/side effects, effective runtime visibility, object-74 semantic
action and successful game launch. No native payload conversion is inferred from these negatives.
