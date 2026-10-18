# GGE: local Play Spore presentation-state dispatcher

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no native runtime or UI callback was executed. This continues
[startup-gge-catalogue-rebuild.md](startup-gge-catalogue-rebuild.md).

## `DF6D80` is a third state namespace

The dispatcher receives a requested presentation value and compares it with `this+0x2C`:

```text
if current +0x2C == requested: return true
index = requested - 1
if index > 4: return false
jump table at 0x00DF7670[index]
```

The five bounded jump targets are:

| requested value | target |
|---:|---:|
| `1` | `DF6FF9` |
| `2` | `DF6DAF` |
| `3` | `DF6E25` |
| `4` | `DF6EE3` |
| `5` | `DF6F3B` |

On the observed successful branches, UI flag operations occur first and the local field
`this+0x2C` is written only afterward. This field is not controller `+0x108`, not controller
`+0x10C`, not internal handler key `2`, and not an outer AppState ID.

## The observed caller uses presentation value 1

`DF7730` calls `DF6D80(1)` after replacing the local per-record presentation owner at
`this+0x8C`. If the dispatcher returns true, it applies several UI/control operations to the
current presentation owner:

```text
0x03753F40
0x04656452
0x0A79B310
0x0638491D
```

It then checks the current record at `this+0x30`: stage `+0xA8 == 5` and global byte
`0x016A0F88 == 0`. Only under those guards does it apply another operation to control
`0x03C4925E`, with flag/value `2`. The operation is a local UI/effect presentation action;
resource attachment and callback success remain Unknown.

## Bounded branch observations

The branches use distinct UI/effect sets. For example:

- `DF6FF9` looks up `0x02E5A4A8` and applies flag operations to targets including
  `0x04656452` and `0x05B31710`.
- `DF6DAF` applies operations to `0x046A9CA7`, `0x04656452` and `0x05B31710`, then writes
  `this+0x2C = requested`.
- `DF6E25` begins with the same effect-owner lookup and has its own UI branch before writing
  the local state.

These are bounded instruction observations, not semantic names for the resource IDs. The full
five-branch UI matrix and all computed callback receivers are not promoted beyond the inspected
prefixes.

## Separation from catalogue and gameplay

The surrounding order is:

```text
DF9740 state-2 admission
  → DF95A0 source merge / records
  → DF9200 per-record UI data
  → DF8300 marker/effect pass
  → DF6C10 event/timing continuation
  → DF7690 owner/layout preparation
  → DF7730 local presentation value 1
  → DF6D80 UI/effect flags
  → later DF6010 viewer projection
```

This dispatcher does not create terrain, actors, a stage world or ScenarioMode. A successful
local state write or UI flag call is not a readiness observation. Object-74 `Play Scenario`,
native mounts, camera/viewer availability and playable gameplay remain Unknown.
