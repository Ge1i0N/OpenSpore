# Scenario Simulator proxy registry/service boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Bounded `objdump` audit only; no native execution, resource completion, terrain/world creation or
gameplay tick was observed.

Continuation of [startup-gge-scenario-simulator-proxy-ownership.md](startup-gge-scenario-simulator-proxy-ownership.md).

## `B540D0` / interior loop `B54180`: query-result iteration

`B540D0` uses the supplied second argument or looks it up in table `0x016852E8`. If no entry is
found, it returns. Otherwise it passes `entry +0x70` to `108E040` with value `8`, submits the
result through `1083380`, calls `109AEF0`, and iterates result entries at `+0x90` using count
`+0x94`. `B54180` is an interior loop address, not a separate function entry. The exact query and
submission semantics remain Unknown. Each result is admitted only when keyed value `0` read by
`496640` equals `3`, keyed value `1` is non-null, its component `0x01186577` exists, and component
byte `+0x77` is nonzero.

For an admitted entry the path:

1. reads the owner transform/bounds;
2. computes distance/scale-like values using component vectors;
3. writes the resulting vector through component virtual `+0x38`;
4. calls `B53AB0` for lookup/update;
5. calls `B55750` for the proxy continuation.

This is a query-result traversal and proxy update loop, not evidence of a persistent registry
insertion. Value `3` is retained as a native keyed value, not named as a gameplay/world-ready state.
The result is finally passed to `1083460` when non-null; its successful runtime processing is
unobserved.

## `B53A10` / `B53AB0`: manager-table lookup

The helpers resolve object/service entries through distinct lookup paths:

- `B53A10` checks component `0x0116DD1B` for non-null source. If present, it uses table
  `0x0167ED58` and returns `AEF260(entry)` for a non-null mapped entry; a miss in this branch
  returns null. The alternate branch (null source or missing component) uses table `0x016801A8`
  and returns the mapped value or null. It is not a fallback after every primary-table miss.
- `B53AB0` first looks in table `0x01682A48` and conditionally calls `B4E2D0`. It then checks
  component `0x0116DD1B` and table `0x0167ED58`, calling `B538C0(component, mappedEntry, 1, 1)`
  for a non-null mapped entry. A miss in this branch returns without that update.
- `B53AB0` calls `B524B0(source, 0)` in the alternate null-source/missing-component branch.
  This slice does not classify that helper as cleanup.

These are registry/service lookup and update paths. They do not expose a ScenarioMode acceptance,
terrain construction or stage activation call.

## `B538C0`: transform/component synchronization

For a non-null proxy, `B538C0` independently gates `AEF350(D20610(source))` on its first flag and
`AEF760(source virtual +0x2C)` on its second flag. It always copies the source's four-word value
from virtual `+0x30` into `AEF4B0`, also passing the second flag. It then checks source flag
`0x2000`. When present it obtains component `0x01186577`, looks it up via `B522C0` in table
`0x016852E8`, and conditionally forwards a derived vector through `108DE40`.

These are observed transform/data transfers. Indirect callees remain unresolved; no direct
world/terrain/stage factory join is established by the examined bodies.

## `B54300` / `B54410`: existing-object coordinate/vector updates

`B54300` uses the supplied proxy or calls `B53A10`. For a non-null result it computes a vector via
`B4E1C0`, calls `1087880(proxy, vector)`, then returns `AL = 1`. `1087880` forwards the vector to
`[proxy +0x58]` virtual `+0x48` and calls `10877B0`. If the first lookup fails, `B54300` looks in
`0x0167ED58` and calls `AEF760` for a non-null mapped entry, also returning true. With neither
result it returns false. **No allocation or table insertion is shown in `B54300`.**

`B4E1C0` checks keyed value `6` on the proxy: if nonzero it copies source virtual `+0x2C`; otherwise
it computes the center of source bounds from virtual `+0x68`, transforms it using virtual `+0x30`
through `59AED0`, adds source virtual `+0x2C`, and writes a four-word output with last word zero.
This is coordinate preparation, not registration.

`B54410`:

- checks component byte `+0x71`;
- resolves the registry entry;
- applies a transform through `AEF4F0`/related helper;
- routes missing entries through `B53A10`; for a result it calls `1088B40` and forwards the vector
  to `[proxy +0x58]` virtual `+0x60`. The optional flag scales the vector using `10882D0`.

The pair updates existing proxy objects. Removal, ownership acquisition, registration completion
and a playable-stage transition are not established by these calls.

## Boundary result

```text
B540D0 query/result setup
  → interior B54180 result traversal
  → keyed value 0 == 3 + component 0x01186577/+0x77 gate
  → distance/scale and transform calculation
  → B53AB0 manager-table update
  → B538C0 component synchronization
  → B55750 proxy continuation
```

```text
B54300 existing-proxy coordinate update
B54410 existing-proxy vector update
  → B53A10/B53AB0 table lookup
  → proxy lifecycle callbacks
```

Established:

- multiple keyed tables map sources/components to proxy-related objects;
- a query/result traversal uses count, keyed-value and component gates;
- existing-object coordinate/vector update paths are present.

Still Unknown:

- successful manager registration/runtime attachment;
- meaning of keyed value `3`, query type `8` and registry table identities;
- consumer join to terrain, planet, stage, actor or avatar creation;
- input activation, gameplay tick and playable-world readiness.

## Static evidence scope

The address anchors above refer to `B538C0..B53A02`, `B53A10..B53AA9`,
`B53AB0..B53B66`, `B540D0..B542C9`, `B54300..B543C8`, `B54410..B54589`,
`B4E1C0..B4E2CC`, `B522C0..B522FF`, and `1087880..108789C` in the pinned binary.
These bodies were inspected with `objdump -d -M intel`; selected call/branch/keyed-table bytes are
pinned in `tools/verify_startup_state_route.py`. No Ghidra execution was needed for this slice.
The lack of a demonstrated factory join here is bounded to these bodies, not a transitive claim
about every indirect callee or the entire game.
