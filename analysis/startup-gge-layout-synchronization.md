# GGE layout synchronization and owner replacement

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime layout mount, registration, callback success or
rendered UI was observed. Continuation of [startup-gge-ui-lookup-attachment.md](startup-gge-ui-lookup-attachment.md).

## `8113D0`: synchronization after attachment

`8113D0` is not the per-card `DF6110` placement routine. Its bounded lifecycle is:

```text
8113D0(layout-owner)
  → require owner +0x88
  → 8102A0(-1, recursive=true)
  → if a matching window is found, stop
  → enumerate layout components
  → query interface 0xEEEE8218
  → collect component windows into a temporary vector
  → read each window's +0x38 rectangle
  → aggregate bounds
  → for each component, read its +0x38 rectangle
  → call component vtable +0x64 with offsets from aggregate bounds
  → release temporary component references
```

The function uses the window geometry interface and a temporary collection. Its coordinate
operations are layout synchronization/normalization, not camera projection and not proof that
the window container was attached to a live renderer.

The no-owner branch returns before enumeration. The early `8102A0(-1, true)` result also changes
the path: when a matching window is found, the function jumps over the aggregate/reposition pass.

## `811C20 → 8113D0`

The attachment helper enumerates windows and calls container vtable `+0xD8` for each component
exposing `0xEEEE8218`. After enumeration it calls `8113D0`. Thus the static sequence is:

```text
811C20
  → AddWindow-like +0xD8 for each component
  → 8113D0 layout synchronization
```

The `+0xD8` call is an attachment request. The subsequent `8113D0` call is a separate geometry
pass. Neither return value is a runtime observation of successful mount or display.

## `811C70`: callback and resource continuation

`811C70` first calls `811C20`, then checks owner callback `+0x8C`. When present, it calls that
callback with owner fields `+0x88` and `+0x90`, passing flag `1`. It then runs a global service
step and checks a lifecycle bit at the layout object derived from `owner-0x8`.

If that bit is set, it requests resource key `0x2FBF2058`, stores the returned object and executes
two additional property/resource operations. If the key lookup returns null, the continuation
stops at that branch. These operations are display/layout lifecycle, not GGE camera selection or
world construction.

## `812100` and `8121C0`: replacing the layout owner

`812100` accepts a candidate owner object. If the candidate is null and the caller's flag allows
lookup, it resolves an object through the global layout registry helper `8106C0`. It then:

1. compares the candidate with owner `+0x88`;
2. releases the old owner when different;
3. retains and stores the new owner at `+0x88`;
4. calls `811C20` when the new owner exists.

`8121C0` obtains a new `UI/LayoutObjects` object through `811070`, replaces the stored object at
`+0x14`, copies the caller-supplied three-word layout key into `+0x08/+0x10`, and calls `812100`
when the new object's `+0x88` is not initialized. The key is an indirect caller-supplied layout
identity, not the per-card root ControlID `0x02E5A4A8`.

`812250` is a caller-side name/key framing helper and forwards the resulting values to `8121C0`.
The two bounded GGE callers are detailed in
[startup-gge-layout-request-callers.md](startup-gge-layout-request-callers.md).
This narrows the static bridge from GGE UI initialization to layout-owner lifecycle, but does not
observe resource loading or runtime mount completion.

## Boundary separation

```text
DF6110
  → receiver-local lookup
  → per-card geometry
  → placement threshold / SetLocation-like call

811C20 / 8113D0
  → component attachment request
  → aggregate layout synchronization

811C70 / 812100 / 8121C0 / 812250
  → callback/resource continuation
  → owner replacement and layout identity
```

All three are UI/layout lifecycle boundaries. None proves renderer consumption, visible UI,
camera/world/stage readiness, Scenario conversion or playable gameplay.

## Status

Established statically:

- post-attachment synchronization and aggregate bounds pass;
- component offset updates through vtable `+0x64`;
- owner callback/resource continuation conditions;
- layout-owner replacement and `+0x88` storage;
- caller-supplied layout key path through `8121C0`/`812250`.

Still Unknown:

- runtime resource load, registration, parent attachment and callback success;
- displayed UI and renderer state;
- world/stage construction, Scenario action execution and gameplay readiness.
