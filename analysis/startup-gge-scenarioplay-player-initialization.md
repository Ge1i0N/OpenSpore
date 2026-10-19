# cPlayer initialization: collectables, resource query and listener

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump`/PE inspection; no Ghidra, native execution or save writer in this pass.
Continuation of [player factory dispatch](startup-gge-scenarioplay-player-factory-dispatch.md).

## Callback identity and base reference

`B20F40` calls the returned player's virtual `+0x28(0)`. For the previously recovered cPlayer
vtable `0x01472CC8`, this slot is `C7C870` (body `C7C870..C7C959`). It starts by forwarding the
argument to `B18490`, which replaces the reference at player `+0x2C` with retain/release calls.
For this zero argument it clears/releases that reference; it does not copy an avatar or world
pointer into the player. `B18490` copies source `+0x30` to player `+0x30` only for non-null input.

## Local state and collectables allocation

The remaining initial setup performs:

1. `9201E0(&player[+0x10F8])`: obtains time-derived state through `_mktime64` and `_time64`,
   masks the difference with `0x7FFFFFFF` and stores one DWORD. This is not a readiness timer
   completion or world reference.
2. `C7C640(player +0x113C, 10)`: grows/shrinks a vector of `0x14`-byte entries to ten elements.
   Entry payload semantics remain Unknown; ten entries do not prove ten assets/actors loaded.
3. `F47240(0x6DAC, "Simulator/cCollectableItems", ...)`, followed by constructor `597E00` for
   a non-null allocation.
4. Retain/release replacement of player `+0x10E8` with that result.
5. `599440(0xAD56080C, 0x4A5C4493, u32[0x0150D6DC])` on the collectables receiver, and reset
   player `+0x10EC = 0`.

The allocation's null branch sets the candidate pointer to zero; the subsequent configuration
call is not protected by a non-null check. No successful allocation or graceful failure is inferred.

`597E00` installs table pointers `0x013F62A0/0x013F6290`, initializes embedded containers and
defaults, and returns the object. `599440` stores its third argument at collectables `+0x6D74`,
calls `599040` with the first two arguments, then returns `AL = 1` unconditionally. The inspected
prefix of `599040` checks the keys, queries service `67DDF0` virtual `+0x2C`, and branches on its
Boolean result before reading property data. **`599440` returning true is not proof that this
resource query succeeded or populated the container.** Full property/resource decoding is outside
this slice.

## Listener registration, not event publication

`C7C870` calls `C75380`. That body registers the adjusted listener receiver `player +0x34` through
message-service virtual `+0x20` for these IDs:

```text
04BEF1E3  044F1189  05E902D3  06524498  06526395
06667038  06666683  06527231  06527EAF  01A0219E
```

The concrete message service was recovered in
[startup-message-delivery.md](startup-message-delivery.md): its vtable `0x01427AC0` has
`+0x20 = 883AD0`. `883AD0` builds a listener descriptor (listener pointer, flag and zero handler
fields), optionally calls listener `+8` under service byte `+0xC4`, and forwards the descriptor
and event ID to service `+0x44`. It is a registration operation, not a send.

The player constructor installs listener vtable `0x01472CA4` at player `+0x34`; its callback slot
`+4` is `C77ED0`. Thus listener subscription and callback body are statically joined, conditional
on the expected player and service objects. Delivery remains unobserved.

### Concrete consumer of `0x01A0219E`

At `C7816F..C781A5`, the listener handles event `0x01A0219E`:

```text
payload +8 == 5
  and listener-relative +0x10B4 != null
    → 599440(the same two IDs and global argument)
return AL = 1
```

`listener +0x10B4` is `player +0x10E8`, the collectables pointer above. A different payload code
or null collectables pointer skips reconfiguration but still returns handled. The event is not
renamed as a world-ready notification.

Previously inspected `B228F0`/`B20CC0` send payload codes `3`/`4` respectively; **those codes
do not pass this consumer's code-5 gate**. No code-5 producer or original-runtime delivery is
established in this pass. Other branches of `C77ED0` call separate services or emit separate
events; their entire behavior is not promoted to ScenarioMode gameplay.

## Final resource/context query

After registering listeners, `C7C870` releases a prior interface at player `+0x11F4` (if present),
clears it, and calls global service `67DDF0` virtual `+0x2C` with numeric keys `0x02CA2581`,
`0x02AE0C7E`, and output address `player +0x11F4`. The query result is not tested before return.
The key names, resource payload and whether a non-null interface was produced remain Unknown.
An output address alone is not evidence of mounting, avatar population or world attachment.

## Result and next boundary

- **Established statically:** cPlayer init callback, base reference handling, local vector/time
  state, collectables allocation/configuration, player listener and final service query.
- **Corrected:** event value references include subscription/removal as well as sends. `AC1430`
  subscribes through `+0x20`; `B452F0/FDE170` remove subscriptions through `+0x2C`.
- **Unknown:** resource/query success, code-5 producer/delivery, player/avatar attachment, world
  or terrain/stage construction, input activation and completed gameplay tick.
- Object-74 `Play Scenario 0x070D7509` producer/delivery join is unchanged.

Next useful candidates are player activation/update slot `+0x1C = C79F70` and the ordinary
ScenarioPlayMode phase-3 continuation `EF72C0`, rather than expanding collectables/UI details.

## Static evidence scope

Instruction anchors: `C7C870..C7C959`, `B18490..B184C9`, `C7C640..C7C6BE`,
`9201E0..920253`, `597E00..597ED2`, `599440..59945D`, `C75380..C754BC`,
`C77ED0..C781AE`, `883AD0..883B1D`, plus the inspected resource-query prefix
`599040..59914F`. Callback slots, allocation label, calls and branch/data anchors are pinned by
`tools/verify_startup_state_route.py`. Unresolved indirect effects remain Unknown.
