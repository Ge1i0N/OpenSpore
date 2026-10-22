# Startup: synchronous messages, queued actions and frame delivery

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only short `objdump -d -M intel` slices and PE-byte/vtable checks only; no game process,
save-writing path or native message callback was executed. Selected facts are guarded by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
This continues [startup-state-dispatch.md](startup-state-dispatch.md).

## Concrete service identity

`67DC80` returns global `015FD890`; `67DE70` sets it. In AppMode initialization `7E8C40`:

```text
7E8ECB: allocate 0xC8
7E8EED: ctor 884B10                  vtable 01427AC0
7E8EF6: retain at AppMode+0x14
7E8F15: service +0x10(4, 0)          option setter 883A00
7E8F1B: 883960(service)              publish separate global 016514CC
7E8F24: 67DE70(service)              publish 015FD890
```

Relevant slots on **this** vtable:

| slot | body | bounded meaning |
|---|---|---|
| `+0x14` | `8854A0` | invoke target/listeners in the same call |
| `+0x18` | `885980` | construct and insert queued record |
| `+0x34` | `8841F0` | gated/budgeted queue-processing wrapper |
| `+0x38` | `8847F0` | extract records and deliver callbacks |

These addresses come from the selected constructor/vtable, not the different SDK address table.

## +0x14: synchronous callback delivery

`8854A0(eventID, payload, optionalTarget)` has two routes:

- Non-null explicit target: call target virtual `+0x04(eventID, payload)` at `8854C9`, then return.
- Null target: look up listeners for wildcard `0xFFFFFFFF` and the requested event ID in the
  registry at service `+0x70`. Object listeners receive virtual `+0x04`; function handlers are
  called with event, payload and stored context.

On the registered-listener route, `+0xC0/+0xC1` determine whether callbacks occur while walking
the list or after copying listener descriptors into temporary storage. The snapshot route's
callbacks at `88568A/88569E` still happen **before this function returns**. This temporary list
is not the persistent message queue. Locking/lifetime flags are preserved as byte conditions;
this audit does not claim exhaustive concurrency semantics or listener ordering under mutation.

Thus manager policy `+0x14 == 1`, which selects this service slot, can deliver actions before
the enclosing state switch writes its current ID. There is no basis for imposing the host's
32-hop queue policy on this synchronous branch.

## +0x18: queued record, no event callback here

`885980(eventID, payload, orderValue, optionalTarget)` retains applicable payload/target
interfaces under service flag `+0xC4`, then builds a six-dword (`0x18`-byte) record:

```text
+0x00 event ID
+0x04 payload pointer
+0x08 explicit listener pointer, or zero for broadcast
+0x0C signed ordering value
+0x10 zero function-handler pointer (this entry point)
+0x14 zero function-handler context (this entry point)
```

It passes the record to `885800` on service queue `+0x08`, protected by the `+0x38` lock when
`+0xC0` is enabled. `885800` compares signed record `+0x0C` values and chooses front, back or
interior insertion; the equal-tail case appends. State actions pass ordering value **zero**.
The payload/target lifetime calls in `885980` are not event delivery callbacks.

The state-manager constructor explicitly writes policy `+0x14 = 0` at `7E5317`. Combined with
the previously inspected `7E2D90` branch, this establishes **constructor-default queued action
delivery**, not proof that no later writer can change that policy. This pass does not claim
an exhaustive alias-aware search for all policy writes.

## +0x38: pop before callback; optional drain-until-empty

`8847F0` gets a starting queue-size/processed-counter bound, then repeatedly:

1. Calls `884640` on service queue `+0x08`; that helper copies a `0x18`-byte record out and
   advances/removes the queue front **before returning success**.
2. Increments the processed counter at service `+0x68/+0x6C`.
3. Releases the queue lock before callback delivery.
4. Dispatches the extracted record:
   - non-null target → target `+0x04(event, payload)` at `8848EA`;
   - non-null function → call it with event/payload/context at `88490F`;
   - neither → service `+0x14(event, payload, 0)` at `884938` (broadcast).
5. Performs applicable lifetime cleanup and decides whether to continue.

If byte `+0xC3` is nonzero, `884958` loops back to another pop regardless of the starting bound.
If zero, the processed counter is compared against that bound. Empty pop terminates either
route. Therefore new messages posted by callbacks **can be processed in the same pump call**
when `+0xC3` is enabled; this is not a one-event-per-frame contract and not a bounded 32-hop chain.

Constructor `884B10` sets bytes `+0xC0..+0xC5` to one. The observed startup setter
`+0x10(4,0)` dispatches to `883A44` and clears **+0xC3**, so the constructor's drain-until-empty
default is **not** the setting established by this startup call. Under that observed setting,
the starting count/counter bound limits a pump invocation; further messages may remain queued.
Later changes and actual runtime flag values remain unobserved.

`8841F0` is a separate processing wrapper: it checks `+0xC2`, queue occupancy, count and timing
budgets and invokes service `+0x38`. It must not be confused with `+0x38` itself.

## Concrete frame call site and ordering

The app frame function `F477D0` first checks app byte `+0x0C`, performs elapsed-time/frame-limit
work, then at `F478E5` obtains `67DC80` and invokes service `+0x38` at `F478F1`.
After this call it performs other app updates and calls AppMode virtual `+0x78 = 7E9E80`
at `F47945`. Inside that AppMode update, `7EA258` invokes state manager `+0x18 = 7E5CF0`
(the separate script-reload check), not a gotoState pending-queue drain.

This proves a static **frame → queued-message delivery → later AppMode update** order.
It does not prove that a particular frame ran or that any queued startup message was delivered.
The separate pump use at `F4763C` is in shutdown work and is not the main-frame call site.

## 00E1132D is the console next command, not the missing frame event

A four-byte immediate scan over `.text` found one literal `00E1132D` operand at `7E512D`;
the instruction starts at `7E512C`. The containing handler is console `state` callback
`7E4FF0`, selected by vtable `014137BC` (the nearby `7E5050` is an internal instruction,
not a separate function entry).

The command constructor `7E4F40` names option index 1 `-next^` and index 2 `-prev^`.
At `7E5119..5131`, option-mask bit 1 calls state manager's secondary listener **directly**:

```text
listener = manager+4
listener virtual +0x04(00E1132D, 0) = 7E36C0
  → 7E3708: if manager+0x8C != FFFFFFFF, switch to that ID
```

The adjacent bit-2 branch supplies `00E1132E` for previous-state/history handling.
This found producer bypasses the global message queue. The scan is bounded to literal operands;
computed IDs or indirect generic producers are not ruled out. It supplies no evidence that
`00E1132D` is emitted automatically each frame.

## Startup conclusion and next boundary

The static startup model is now more specific:

```text
state manager constructor policy=0
Main switch → entry gotoState action → service +0x18 queue insert
Main switch writes current ID and returns
later app frame → service +0x38 pop → targeted state-manager listener
  → resolve GalaxyGameEntry name → switch → mode action queued
later pump capacity → broadcast mode action → existing mode-manager endpoints
```

The separation above is conditional on the observed defaults/configuration remaining in force.
The startup `+0xC3=0` setting means do **not** assert that GGE's newly queued mode action is
delivered in the same pump invocation as Main's gotoState. Same-frame versus later-frame
activation requires following actual queue contents and other pump sites or observing runtime.

Continuation: [startup-mode-frame.md](startup-mode-frame.md) connects the mode-manager listener
to driver lifecycle and selected-driver update in the outer frame, keeping setup-all and
activation-one distinct and not equating state requests with UI/world readiness.
Native mounts, actual queue contents, completed GGE activation, object-74 Scenario launch delivery
and playable world ticks remain Unknown.
