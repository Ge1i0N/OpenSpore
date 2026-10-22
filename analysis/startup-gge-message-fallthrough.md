# GGE message fall-through: generic ButtonClick producer versus DE99B0

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native click, WinProc pump, callback or Scenario launch was
observed. Continuation of [startup-gge-winproc-parent-registration.md](startup-gge-winproc-parent-registration.md).

## Generic activable-window producer

The generic activable-window body at `0x009640B0` constructs a component message after its local
state/flag checks:

```text
object +0x84, or fallback object +0x80
  → message.type = 0x287259F6
  → message.payload = selected command/control value
  → embedded IWindow message sender slot +0x114
```

The same body has later state branches that construct message types `0x17` and `0x18` for generic
button click/select paths. These are generic UTFWin contracts; the bounded body does not contain
the GGE `0x070D7509` or `0x070D74E7` identities.

## `00958880`: attached WinProc walk

The generic window-message path walks the attached WinProc list:

```text
window message
  → enumerate attached procedures
  → procedure outer slot +0x18(message)
  → if handled: stop
  → if unhandled: continue to next procedure
  → default/owner fallback path may run afterward
```

The return value from the attached procedure controls whether the list continues. This is the
important boundary for `DE99B0`: being attached proves it can be reached by the common walk, but
does not prove it consumes every message.

## `DE99B0`: explicit branches and negative controls

`DE99B0` first rejects/defers when its own lifecycle field or an external guard is active. It
forwards through object `+0x34` when that optional target exists, then compares the incoming
component identity and dispatches a bounded set of explicit branches.

The inspected switch contains identities including `0x0615B966`, `0x061DD69B`, `0x0615B616`,
`0x070D74E7` and related controls, but no direct `0x070D7509` branch. Its bounded body also has
no direct `0x17`/`0x18` message-type branch. Therefore the static result is:

```text
generic ButtonClick/Select or Play Scenario candidate
  → attached DE99B0
  → DE99B0 may return unhandled for this bounded case
  → next WinProc / parent-owner fallback remains possible
```

The known `0x070D74E7` branch is not promoted to a conversion from ControlID `0x070D7509`.

## Parent/owner fallback remains separate

The window walk and parent-registration rollback paths are distinct from the GGE listener body:

```text
962780 outer child lifecycle
  → false: owner +0xE0 rollback

00958880 message walk
  → attached procedures
  → unhandled fall-through
  → separate window/owner default route
```

The existence of an unhandled return does not identify the later owner callback payload or prove a
command dispatch. The generic owner fallback modes are bounded in
[startup-gge-owner-fallback.md](startup-gge-owner-fallback.md). No static join in this bounded pass connects `0x070D7509` to `0x070D74E7`,
`0x00E1132F`, `0x01654C10`, or ScenarioMode.

## Status

Established statically:

- generic activable-window message construction;
- generic `0x17`/`0x18` message branches outside the GGE identity set;
- attached WinProc walk and handled/unhandled continuation;
- DE99B0 negative controls for direct `0x070D7509` and `0x17`/`0x18` branches;
- separation of listener fall-through from parent-registration rollback.

Still Unknown:

- runtime click/message delivery;
- producer field population for the Play Scenario button;
- parent/owner fallback semantic payload;
- CommandID `0x00E1132F` dispatch and ScenarioMode conversion;
- world/stage construction and playable gameplay.
