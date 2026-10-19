# Player metadata slot and ScenarioPlayMode tutorial continuation

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump -d -M intel` and PE inspection only. No Ghidra, original game process, callback
or save-writing path was executed. Static branch/call evidence does not establish runtime success.

Continuation of [player initialization](startup-gge-scenarioplay-player-initialization.md) and
[local play phases](startup-gge-scenarioplay-service-phase-progression.md).

## Correction: player slot `+0x1C` is metadata, not an Update

Vtable `0x01472CC8 +0x1C` points to `C79F70`. Its complete short body (`C79F70..C79F86`)
passes:

```text
player receiver
descriptor storage 0x01578D18
UTF-16LE "cPlayer" at 0x01472D1C
```

to `695B20`, using the caller argument as that helper's receiver, then returns. It has no local
frame-time loop, player activation call or avatar/world allocation.

`695B20` looks in the table at `0x0152D69C` using the supplied object pointer. On a miss it calls
`695120`, builds a descriptor context with `692840` and numeric ID `0x01A80D26`, and calls
`695750`, normalizing its result to Boolean. On a hit it appends pointers to two associated
lists and returns true. The inspected `695120` prefix allocates a `0x74`-byte descriptor and
processes the supplied name. This supports a metadata/descriptor association role, not a player
Update contract. The complete metadata consumer semantics remain Unknown.

The previous description of `C79F70` as an activation/update candidate is withdrawn. No conclusion
about all other cPlayer slots follows from this correction.

## `F1F960 → EF72C0`: one-shot continuation after phase-3 elapsed counter

The phase-3 branch at `F1FBB0` calls `EF72C0` only if play byte `+0xFD == 0` and the unsigned
counter at `+0xC0` is strictly greater than `1000`. Earlier in this phase the counter increments
only when the service state and play `+0xB4` checks permit it. This is not an unconditional
wall-clock delay.

After `EF72C0` returns, the caller sets play `+0xFD = 1` without checking a result. Therefore:

```text
EF72C0 action taken OR action skipped by its guards
  → caller sets +0xFD = 1
```

This byte records that the caller passed the continuation, not success of a resource load, tutorial
display, actor spawn or world readiness. No retry from this call site is shown while the latch
remains set.

## `EF72C0`: gated tutorial/service requests

The body (`EF72C0..EF74A8`) requires all three predicates before entering its request branch:

1. ScenarioMode launch context `+0xD0` is `1` or `2` (distinct from strategy `+0xCC`).
2. Service `67DCF0` virtual `+0x30(0x04EA96CB)` returns nonzero. The existing
   [configuration audit](config-scripts.md) associates this ID with `OptionTutorialsEnabled`.
3. Local byte `BL` is false. It defaults to zero. If property service global `0x015FD91C`
   exists, virtual `+0x24` finds key `0x07BE69AD`, and the property type word is `1`, `41E8E0`
   supplies the byte instead. An unavailable/missing/wrong-type property follows the native
   default-zero branch; this is not proof that the property was observed false at runtime.

When admitted, it invokes `B32330(0x04BF38A7)` through `B3D480` and makes four calls to
`67AAB0` on the service returned by `67CAB0` (global `0x015FCC60`). The request data includes
four three-word tuples from `0x015AD21C`, `0x015AD228`, `0x015AD234`, `0x015AD240`; the final
call additionally passes descriptor pointer `0x016C7B80`. A lazy initializer points that descriptor
at `0x0148B504`.

`0x0148B504` is **table data, not text**: its first DWORD is code address `EF7C00`, followed by
other callback addresses. A string decode at that location must not become a resource-name claim.
The tuple identities and exact queue parameter names remain Unknown.

## Immediate consumer: request queue, not world factory

The examined `67AAB0..67AC32` body builds a `0x134`-byte request through `679040` and copies it
into a deque-like container at service `+0x10` through `679B00`/`679FB0`. It then updates request
fields, conditionally retains/replaces callback/context pointers, and, when the computed queue
count is one, calls `67A550(0, 1)` before returning. This is not necessarily a deferred-only queue.

`67A550` checks the same option and processes queued entries through `67A170`/`679E10`. The
inspected prefix of `67A170` queries property/resource service `67DDF0`, allocates a small
`0x78`-byte object, constructs it through `816DA0`, and calls `817120`; a false return branches
away from its success continuation. `679E10` includes window/service cleanup. These are bounded
presentation/service effects, not an identified terrain, planet or stage factory. Their complete
transitive effects and original-runtime acceptance remain Unknown.

## Descriptor callback `EF7C00`

The first descriptor slot is `EF7C00`. Its body constructs a typed property value containing
true, sends it to property service key `0x07BE69AD` through virtual `+0x14`, calls helpers
`6B1F20/6B1CE0` with ID `0x011AC192`, then calls `B32360(0x04BF38A7)` and performs local
cleanup. This is a concrete potential consumer of the queued descriptor, not proof the callback
was delivered or the property persisted. It was not executed in this investigation.

## Boundary result

- **Corrected:** `C79F70` is a metadata/descriptor association wrapper, not the suspected player
  activation/update method.
- **Established statically:** phase-3 caller gate and latch, tutorial-option/property predicates,
   four service requests, immediate queue-consumer candidates and a descriptor callback.
- **Unknown:** tutorial/resource availability, queue/callback execution, native player/avatar
  activation, world attachment, terrain/stage construction, input and gameplay readiness.
- Object-74 `Play Scenario 0x070D7509` still has no new producer/delivery join here.

Next focused work should return to actual player/avatar association writers or entry setup (for
example player field `+0x1248` setter `C755B0` and its decoded call sites), not treat these metadata
or tutorial paths as launch gates. Byte-search candidates require instruction/caller validation.

## Reproduction scope

Complete inspected bodies: `C79F70..C79F86`, `695B20..695C03`, `EF72C0..EF74A8`,
`67AAB0..67AC32`, `679040..6790D5`, `67A550..67A630`, `679E10..679FAD`, and
`EF7C00..EF7C84`. Partial helper slices: `695120..69529F`, `67A170..67A2FF`.
Selected calls, branch predicates, data pointers and vtable slots are pinned by
`tools/verify_startup_state_route.py`; this is static verification only.
