# GGE UI lookup versus layout attachment

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime layout mount, window registration or rendered UI
was observed. Continuation of [startup-gge-projection-placement.md](startup-gge-projection-placement.md).

## Placement lookup: `810650 → 8102A0`

The lookup used by `DF6110` is receiver-scoped:

```text
DF6110
  → ECX = record +0xB8
  → 810650(ControlID 0x02E5A4A8, recursive=false)
  → ECX = UILayout +0x14
  → 8102A0(component span, ControlID, recursive flag)
```

`810650` only unwraps the layout member at `+0x14`. If that member is null it returns null;
otherwise it tail-jumps to `8102A0`. No global registry getter is present in this wrapper.

`8102A0` enumerates the receiver's component span `[+0x64,+0x68]`. For each component it:

1. requests interface `0xEEEE8218` through the component's interface getter;
2. skips null interface results;
3. calls interface slot `+0x1C` to obtain/compare the ControlID;
4. returns the matching window;
5. only when the recursive flag is nonzero, calls window slot `+0xF0` for child lookup.

The placement caller passes recursive=false, so this edge is a local lookup of the root/window
object, not a mount or attachment operation.

## Placement operation after lookup

`DF6110` uses the returned window's geometry slots for the placement calculation and later calls
the placement object's vtable slot `+0x64` only after the strict movement threshold. The observed
operation changes window coordinates; it does not call the container's `AddWindow` slot.

The serialized `GGEGameStarUIv4` root match therefore supports the identity of the default asset,
but does not establish that native creation, component registration, parent notification or
runtime `GetRealArea` succeeded.

## Separate attachment path: `811C20`

The layout attachment helper has a different receiver and lifecycle:

```text
811C20(layout-owner)
  → if owner +0x88 == null: return
  → enumerate owner components [+0x64,+0x68]
  → request interface 0xEEEE8218
  → if non-null, call container +0x88 vtable slot +0xD8 with the window
  → continue through all components
  → continue layout synchronization through 8113D0
```

This is the static `AddWindow`-like attachment path. `811C70` calls `811C20`, optionally invokes
the owner callback at `+0x8C` with owner fields `+0x88/+0x90`, and then performs a separate resource
initialization branch keyed by `0x2FBF2058`. These lifecycle calls are not called by the bounded
`DF6110` placement tail. The post-attachment synchronization and owner replacement paths are
bounded in [startup-gge-layout-synchronization.md](startup-gge-layout-synchronization.md).

## Boundary separation

```text
projection
  → DF6110
      → receiver-local lookup via 810650/8102A0
      → geometry read
      → optional SetLocation-like placement

separate layout lifecycle
  → 811C20
      → component enumeration
      → container +0xD8 AddWindow-like calls
      → 8113D0 synchronization
  → 811C70 callback/resource continuation
```

Finding a serialized root or observing a static `+0xD8` call does not prove that the runtime
layout was mounted, that the card is visible, or that the renderer consumed it. World/stage
construction, Scenario launch and playable gameplay remain separate Unknown boundaries.

## Status

Established statically:

- placement lookup is scoped to `record+0xB8` and `UILayout+0x14`;
- interface identity `0xEEEE8218` and ControlID comparison slot `+0x1C`;
- recursive child lookup is conditional;
- placement and attachment use separate call paths;
- `811C20` enumerates windows and calls container `+0xD8` when owner `+0x88` exists.

Still Unknown:

- runtime resource creation/registration and attachment success;
- parent notifications, runtime geometry and rendered UI visibility;
- camera/world/stage readiness, Scenario conversion and gameplay tick.
