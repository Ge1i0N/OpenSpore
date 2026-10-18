# Startup: line dispatch, gotoState actions and switching

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Evidence: short read-only `objdump -d -M intel` ranges and file-backed PE vtables, pinned by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
No native code, save path or game runtime was executed. This continues
[startup-state-registration.md](startup-state-registration.md).

## Parser input reaches the registered callbacks

The parser vtable `0141CAD0` resolves `+0x3C = 843000` and `+0x40 = 844180`.
The earlier `841360` wrapper therefore reaches `843000`, which:

- checks the input interface and its `+0x10` flag;
- loops over input using interface `+0x14/+0x2C`, reading a line through `93A820` with a
  `0x800`-byte buffer argument;
- writes the line number at parser `+0xF0`, calls parser `+0x40(line)` at `843135`, and repeats;
- checks that block-stack and auxiliary-stack depths match their initial depths at end of input.

`844180` preprocesses a line through `840F90`, then handles an auxiliary stack if present,
performs `843700`, tokenizes through `8384A0` into parser `+0x16C`, and obtains token zero.
It resolves that token through `8439D0` and invokes the returned handler's **slot zero** at
`8442A5`, passing the token/argument object. Missing handlers reach the diagnostic branch
at `844278`; this is not silently treated as a valid command.

`8439D0` searches the block-handler stack at parser `+0x108/+0x10C` from newest to oldest.
It calls each block handler's `+0x1C` name resolver, and falls back to the parser's top-level
registry at `+0x2C`. For `state`, vtable `01413260 +0x1C = 83CAD0` resolves the nested verbs
from that handler's registry at `+0x10`. Thus the outer `state` declaration and its nested
`gotoState` do not need to be registered in the same map.

This identifies the stream→line→token→registry→callback route, not a full lexer specification.
Quoting, substitutions, macros, auxiliary-stack rules and every error unwind remain partial.

## gotoState is a stored message action, not a pre-resolved record link

`[0153F600] → 0141316C` names `gotoState`. During `7E1C10` setup:

```text
7E1D19: create handler vtable 014132B4
7E1D23..36: register it with state handler +0x18 = 83CD00
014132B4 +0x00 = 7E4310        parse callback
014132B4 +0x08 = 6A38C0        retain owner context
```

`6A38C0` stores `context-0x10` at the nested handler's `+0x0C`, giving it the same manager
used by its parent. `7E4310` reads one argument with `838400`, allocates a command object,
sets command `+0x30 = 0x00E11330`, stores the argument string through `618D00`, then calls
`7E4180(manager, args, command)` at `7E4374`.

`7E4180` appends an eight-byte `{command pointer, flag=0}` action to manager's staged lists:
`+0x2C` by default, or `+0x40` when `onExit` is supplied or manager `+0x80` is set. The `entry`
callback `7E19E0` clears `+0x80`; `exit` callback `7E1A00` sets it. Consequently Main's bare
`gotoState GalaxyGameEntry` is staged as an entry action. The record's `+0x4C` is a different
mechanism, populated by the declaration-order link recovered in the preceding audit; do not
reinterpret it as the parsed gotoState target.

## 7E3BF0 performs a switch body; it is not only an enqueue operation

The switch body compares the requested ID to manager `+0x88` and returns true immediately
when equal. Otherwise, in native instruction order:

1. If `+0x81` is set, emit event `0x02319914` with old/requested IDs and manager context.
2. `7E3CDA → 7E28C0(oldID)`: resolve the old record and dispatch its exit-action list at
   record `+0x14` through the message service from `67DC80`.
3. Reset manager `+0x8C` to `0xFFFFFFFF`, then `7E3CF4 → 7E3150(oldID, requestedID)`:
   resolve an explicit transition record in map `+0xD8` and execute it if found.
4. Resolve requested ID in map `+0xB8`; on a hit call `7E2D90(recordIndex)` at `7E3D2D`.
5. If `+0x81` is set, emit event `0x02319915`.
6. Save the old ID in history at `+0x90` when not `0xFFFFFFFF`; **only then**, at `7E3E1B`,
   store the requested ID in manager `+0x88`.

`7E2D90` computes `record = [manager+0xA4] + index*0x50` and iterates its entry actions
at record `+0x00/+0x04` (eight-byte stride). Action event ID comes from command `+0x30`.
For manager policy `+0x14 == 1`, it uses message service virtual `+0x14`; for policy `0`,
it uses `+0x18` with additional arguments. Other policy values skip these dispatch branches.
This pass pins the call shapes, not synchronous/deferred delivery semantics of those services.
It also applies the record's `+0x28` entries through a separate service (`67DD90`, virtual
`+0x98`), and copies non-sentinel record `+0x4C` into manager `+0x8C`.

The routine returns true even if the requested ID was not found in the inspected map branch;
that return is not proof of successful state activation. Also, current-ID assignment occurs
after action dispatch: reentrancy/order must not be guessed without the message-service body.

## Native receiver for gotoState and the separate pending field

The manager owns a secondary listener interface at **manager+4**: its vtable `01413684`
has callback `+0x04 = 7E36C0`. Initialization `7E57F6..7E584B` subscribes that interface to
the event table at `014131F0`, which includes `0x00E11330`.

In `7E36C0`, `this` is that secondary interface, **not the manager base**. References such as
`[esi-4]` restore the primary interface. The jump-table entry for `0x00E11330` at `007E3BD0`
selects `7E37AB`, which extracts the command's string (`payload+0x08`, nested `+0x0C`), calls
manager `+0x38 = 7E30E0`, checks for `0xFFFFFFFF`, then calls manager `+0x28 = 7E3BF0`.

The event `0x00E1132D` takes a **different** jump-table arm (`7E3708`): it reads listener
`+0x88`, which is **manager+0x8C**, and if non-sentinel invokes primary manager `+0x28`.
This proves a consumer for the pending/declaration-link field; the producer and delivery
schedule of `0x00E1132D` are not recovered here. It is not yet a native per-frame queue-drain proof.

## Main → GGE: static route and remaining delivery boundary

The shipped [SporeAppStates script](scripts/SporeAppStates.txt) defines:

```text
Main -id 0: gotoState GalaxyGameEntry
GalaxyGameEntry -id 0x02CCDA9C: mode GalaxyGameEntry
```

Together with the endpoints above, the static route is:

```text
7E2370 name lookup Main → switch(0) → Main entry command 00E11330
  → message service [delivery timing/runtime success Unknown]
  → 7E36C0 / 7E37AB → name lookup GalaxyGameEntry → switch(02CCDA9C)
  → GGE entry mode command 00E11333
  → message service [delivery timing/runtime success Unknown]
  → existing mode-manager listener/setter chain
```

The `mode` parser callback `7E4690` and downstream `7D8240 → 7D8420 → 7D8D40 → 7D8670`
were previously recovered in [Scenario launch-state audit](audit/scenario-mode/launch-state.md).
This pass supplies the entry-action executor between them, not observed native activation.

**Correction to the old explanation in states.md:** `can't nest states or transitions` tests
the declaration/parser staging state. It does not by itself establish a runtime reentrancy
guard or prove the host's queued `request_goto()` / 32-hop cap reproduces native scheduling.
No host runtime code or gameplay-readiness status is changed in this pass.

Next: identify the message service's `+0x14/+0x18` implementations and the `00E1132D` producer,
then trace ordering into the already-known mode activation phases. World/stage readiness,
GGE object-74 launch delivery and actual successful parser execution remain Unknown.
