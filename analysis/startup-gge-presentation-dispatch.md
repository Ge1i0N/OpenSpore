# GGE Play Spore presentation dispatch after catalogue rebuild

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no runtime callback, resource mount or gameplay path was executed.
Continuation of [startup-gge-presentation-state.md](startup-gge-presentation-state.md).

## `DF6D80` input and namespace

`DF6D80(this, requested, ...)` first compares requested value with `this+0x2C`; equal values
return immediately. It computes `requested-1`, rejects values above 4 and dispatches through
table `0x00DF7670`:

```text
requested 1 → DF6FF9
requested 2 → DF6DAF
requested 3 → DF6E25
requested 4 → DF6EE3
requested 5 → DF6F3B
```

The local `+0x2C` field is separate from controller `+0x108 == 2` (Play Spore admission),
controller `+0x10C == 10` (internal request), outer AppState IDs and ScenarioMode tags.

## Caller and bounded UI operations

`DF7730` replaces a local presentation owner at `+0x8C`, then calls `DF6D80(1)`. If that
returns true, it applies UI operations to the owner using targets including:

```text
0x03753F40
0x04656452
0x05B31710
0x0638491D
```

For the current record, an additional guarded operation uses `record+0xA8 == 5`, global byte
`0x016A0F88 == 0` and target `0x03C4925E`, with value/flag `2`. These are observed UI/effect
operations; target lookup or flag call success does not establish a mounted screen.

## Separation from projection

The bounded order remains:

```text
DF9740: controller +0x108 == 2 gate
  → DF95A0: source merge and records
  → DF9200: per-record UI-data objects
  → DF8300: marker/effect pass
  → DF6C10: event/timing continuation
  → DF7690/DF7730: local presentation owner/state
  → DF6010: record position +0x80 → DE3C20 viewer projection
  → DF6110: per-record window placement/clamp
```

`DF6010` loops over `0xBC` records, calls `DE3C20` using `record+0x80`, and dispatches the
projected result through vtable slot `+0x64` (`DF6110`). `DE3C20` obtains a viewer from
GameModeManager `+0x58`; `DF6110` separately uses `record+0xB8` per-record UI data and the
layout root bounds. These are geometry/presentation consumers, not world construction.

## Result

The local presentation dispatcher and later projection are now kept as separate boundaries.
Unknown remains explicit for UI attachment/mount success, viewer production, camera parameters,
live catalogue contents, Scenario launch conversion, stage/world construction and gameplay.
