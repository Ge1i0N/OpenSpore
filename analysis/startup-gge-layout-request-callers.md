# GGE layout request callers: ZoomPanel and GlobalUI owner paths

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native resource load, runtime mount, parent attachment or
rendered UI was observed. Continuation of [startup-gge-layout-synchronization.md](startup-gge-layout-synchronization.md).

## `DE5900`: ZoomPanel/V2 request and control setup

The GGE controller UI initializer begins by selecting a panel variant through `6854E0(2)`. It
then calls `812250` with a bounded request containing:

```text
name source:
  selected between global layout-name pointers 16A0D34 / 16A0D44
mode:       1
descriptor: 0x40464100
tag:        0x05B598FA
```

`812250` parses the selected wide name, frames the three-word layout key and forwards to
`8121C0`. The native operands establish request construction; they do not establish that the
package resource was found or that the owner was mounted.

After the request, `DE5900` keeps a table of control IDs and iterates the first group through the
active receiver at `controller+0x144`:

```text
0x0615B966
0x061DD69B
0x0615B616
```

Each ID is passed to `810650` with recursive flag `1`, then the returned window receives a vtable
slot `+0x104` operation with the active receiver. If `6854E0(2)` selects the alternate branch,
the second group adds:

```text
0x070D74E7
0x070D7509
```

The latter is the serialized EP1 `Play Scenario` control identity. This call site proves control
lookup/configuration operands, not the object-74 action execution or ScenarioMode conversion. The
subsequent WinProc and parent-registration boundary is documented in
[startup-gge-winproc-parent-registration.md](startup-gge-winproc-parent-registration.md).

## `E035F0`: GlobalUI layout owner

The separate `E035F0` path initializes the `UI/GGE/GlobalUI` owner:

```text
E035F0
  → initialize controller/service fields
  → obtain layout object through 8100A0
  → replace owner at object +0x08 when different
  → when owner exists:
      8121C0(parameter, mode=1, mount=0x2EDD95CA)
      → optional continuation E02AC0 when 8121C0 succeeds
```

`E035F0` therefore supplies a second caller for the layout-owner replacement path. Its
`0x2EDD95CA` operand is a parent/owner lookup identity in this call chain, not the per-card root
ControlID `0x02E5A4A8`, not the camera ID `0x780786FD`, and not a ScenarioMode tag.

## Relationship to attachment and synchronization

The bounded caller chain is:

```text
DE5900
  → 812250(name, mode, descriptor, tag)
  → 8121C0
      → 812100 owner replacement
      → 811C20 component AddWindow-like calls
      → 8113D0 bounds synchronization

E035F0
  → 8121C0(GlobalUI, mode=1, 0x2EDD95CA)
  → same owner/attachment lifecycle when owner exists
```

This separates request construction, owner replacement, component attachment, synchronization and
later control lookup. Static success of one stage must not be promoted to success of the next.

## Status

Established statically:

- ZoomPanel/V2 selection branch;
- layout request tag/descriptor/mount operands;
- control-ID groups used by the ZoomPanel path;
- separate GlobalUI caller and `0x2EDD95CA` owner operand;
- connection to `812250/8121C0` lifecycle.

Still Unknown:

- package lookup/load result and runtime owner creation;
- component registration, parent attachment and callback success;
- `Play Scenario 0x070D7509` event delivery/execution;
- ScenarioMode conversion, world/stage creation and gameplay readiness.
