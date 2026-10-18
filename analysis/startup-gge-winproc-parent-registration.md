# GGE WinProc attachment and parent-registration boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime callback, mounted window or rendered event was
observed. Continuation of [startup-gge-layout-request-callers.md](startup-gge-layout-request-callers.md).

## `DE5900`: WinProc/interface configuration

After receiver-local `810650` lookup, `DE5900` obtains an interface from the returned window using
interface key `0x8ED27E7A`. The returned interface is then configured through vtable slot `+0x28`:

```text
resolved window
  → interface getter +0x0C(0x8ED27E7A)
  → interface +0x28(4, conditional root flag)
  → interface +0x28(2, 0)
  → interface +0x28(8, 0)
  → interface +0x28(16, 0)
```

The conditional flag is set when the current ControlID equals the selected primary ID. The same
sequence is used for the alternate group containing `0x070D7509` when `6854E0(2)` is true.

This is a static WinProc/control configuration edge. The interface key and vtable operations do
not establish that a callback was invoked by a user event.

## `962780`: parent registration

The owner-side `+0xD8` AddWindow-like path resolves the child and then performs outer lifecycle
registration:

```text
962780(owner, child)
  → retain/normalize child pointer
  → link child into owner list
  → optional owner +0x30 notification
  → child outer vtable +0x18
```

The selected GGE vtable has outer slot `+0x18 = DE99B0`; the generic button vtable has a different
outer implementation. The call is therefore an object lifecycle callback, not the GGE message
receiver by identity alone.

If the outer `+0x18` callback returns false, `962780` calls owner vtable `+0xE0` with the child
link and returns false. This is an explicit rollback edge. When the callback succeeds, it calls
the list/state helpers `960AA0`, `960A40` and `9613B0`, then returns true.

The symmetric `962830` path removes the child from the owner list, invokes the owner notification
when present, clears child links and releases the child. This confirms that `+0xD8` attachment has
a paired removal lifecycle.

## Object-74 boundary

The static chain for the serialized EP1 `Play Scenario` control is now:

```text
DE5900
  → 810650(0x070D7509, recursive=1)
  → IWindow interface 0x8ED27E7A
  → interface +0x28 configuration
  → parent AddWindow-like +0xD8
  → 962780
      → outer child lifecycle +0x18
      → rollback on false / list synchronization on true
```

This establishes attachment/configuration infrastructure only. It does not establish:

- a user click or WinProc callback delivery;
- the object-74 action producer;
- `0x00E1132F`/ScenarioMode delivery;
- world/stage construction or gameplay readiness.

## Status

Established statically:

- WinProc/control interface key and configuration sequence;
- conditional alternate-group configuration for `0x070D7509`;
- parent registration and outer callback slot `+0x18`;
- explicit rollback through owner `+0xE0`;
- symmetric child removal path.

Still Unknown:

- runtime WinProc invocation and event delivery;
- callback semantic payload and object-74 action execution;
- ScenarioMode conversion, world/stage creation and playable gameplay.
