# Scenario Simulator proxy allocation and update boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
This is a bounded `objdump` audit of the shipped bytes. No native execution, resource completion,
terrain/world creation or gameplay tick was observed.

Continuation of [startup-gge-scenario-event-phantom-boundary.md](startup-gge-scenario-event-phantom-boundary.md).
The subsequent registry/table update boundary is documented in
[startup-gge-scenario-simulator-proxy-registry.md](startup-gge-scenario-simulator-proxy-registry.md).

## `F47240`: generic manager allocation boundary

The `F47240` call used by the phantom path packages its caller arguments and forwards them to a
manager routine through global manager state at `0x016C8B44` / `0x016C8B40`. The helper returns a
native object pointer or null. Its bytes do not contain a terrain/stage-specific constructor or a
Scenario-mode acceptance transition.

Therefore the strongest supported meaning is **generic allocation request**. Registration is not
established by forwarding arguments to `928C80`; the returned object's ownership remains Unknown.

## `AEFB00`: proxy/object state initializer

The object initializer reached by the surrounding simulator family writes a table pointer
(`0x0145C100`) at object `+0x58` and initializes:

- bounds/storage at `+0x64..+0x6C`;
- list/state fields at `+0x58..+0x60` and `+0x70`;
- transform/float fields at `+0x04`, `+0x10..+0x1C`, `+0x40..+0x4C`;
- state bytes at `+0x20`, `+0x28`, `+0x34`, `+0x74`;
- scalar defaults at `+0x78`, `+0x128`, `+0x12C`, `+0x140`.

It also initializes an embedded substructure at `+0x80` and returns the object. These are concrete
native object fields, but the bytes do not identify this object as terrain, planet, stage, actor or
avatar.

## `B550F0`: geometry/proxy preparation

`B550F0` first requires an auxiliary source object, then obtains a component through source virtual
`+0xAC` and rejects the path when that component or its `+0x90` value is absent. It reads several
hashed tuning values, including the values passed through `4E1CA0/4E1CE0`, and conditionally loads
additional configuration keys when a caller flag is set.

It derives bounds/dimensions and selects one of several geometry/setup cases. The cases allocate or
fill typed setup records and return a proxy/geometry result with a caller status code. The function
does not call a terrain loader or publish a playable-stage state.

## `B55750`: update/attachment continuation

One branch of `B55750` requires object byte `+0xA7` and a status bit in `+0xA8`. It derives mode
flags from `+0xB0/+0xB8`, calls `B53380` and `B3E720` to prepare/update the proxy, then invokes
component virtual `+0xB8` and `B538C0` with the resulting object/context. A zero `+0xA7` instead
branches to `B559C4`, which has a separate table lookup/update path; these are not universal
entry preconditions for the function.

Its continuation has separate mode cases. Depending on flags and returned proxy state it:

- updates bounds/position-related values;
- calls `B52760`, `B54300` or `B540D0` lifecycle/update helpers;
- computes distance/scale-like values from the object transform;
- may emit additional typed setup records;
- handles object flag `0x2000` and byte `+0x71` as policy gates.

These are proxy update operations, not proof of completed attachment. The follow-on lookup and
manager-table boundary is
documented in [startup-gge-scenario-simulator-proxy-registry.md](startup-gge-scenario-simulator-proxy-registry.md).
No direct call to a world/terrain/stage factory is present in this bounded slice.

## Boundary result

```text
B56160 phantom policy
  → B550F0 geometry/proxy preparation
  → F47240 generic manager allocation
  → AEFB00 native proxy/object initialization
  → B55750 update/attachment cases
  → simulator/object lifecycle helpers
```

Established:

- a generic manager allocation request exists;
- a separate initialized native object family exists;
- geometry/bounds/proxy preparation and update paths exist;
- object flags and lifecycle helpers gate later updates.

Not established:

- manager ownership completion or successful runtime registration;
- terrain/planet/stage materialization;
- actor/avatar spawn or input activation;
- gameplay tick or playable-world readiness.
