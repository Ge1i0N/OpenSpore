# Scenario object finalizer and lifecycle synchronization

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static shipped-binary evidence only. No native execution, resource completion, terrain/world
creation or gameplay tick was observed.

Continuation of [startup-gge-scenario-object-factory-dispatch.md](startup-gge-scenario-object-factory-dispatch.md).

## `EED640`: status synthesis

`EED640` consumes a source tuple, a service/object context and a five-word output record. It first
resolves the source identity and compares it with a sentinel classification. For non-sentinel
objects it combines:

- source/context metadata;
- component `0x0175CDC9` and its observed state value `9`;
- `F25450`, which excludes several identity classifications;
- `F25500`, which accepts identity values `0x24682294` or `0x476A98C7`;
- `F25560`/`F25520`, which inspect additional identity/status values.

It then writes the output record:

```text
word 0: preserves/adds bit 0x2 and conditionally bit 0x10
word 1: derived predicate
word 2: derived classification code (observed 0, 1, or 3)
word 3: source +0x220 value
word 4: status bits, including 0x10, 0x4 and 0x2 combinations
```

This is status/eligibility synthesis. It does not mount a resource or create terrain. The
classification code meanings remain Unknown.

## `B228F0`: object-list lifecycle synchronization

`B228F0` walks the owner linked list at `+0x10C`. For each entry it:

1. obtains component `0x01186577`;
2. checks virtual methods `+0xAC`, `+0x90` and `+0xA0`;
3. reads component/object flags and may synchronize a transform/value through virtual `+0x34`
   and `+0x64`;
4. keeps the object when its predicates pass;
5. otherwise unlinks/releases the entry through `F47220` and publishes internal event
   `0x01A0219E` with event payload fields.

The cleanup path also calls `B20620`, which clears associated lists and releases retained entries,
then compacts a vector through `E25BB0`. These are concrete ownership/lifecycle effects. They are
not evidence of a terrain loader, stage activation or a successful playable-world transition.

## Boundary result

```text
EEDD40 common tail
  → component 0x01186577 setup
  → EED640 status synthesis when caller parameter is nonzero
  → B3D400/B228F0 lifecycle synchronization
  → object/record return
```

```text
B228F0
  → iterate object list
  → component/virtual predicate checks
  ├─ pass: retain and optionally synchronize value
  └─ fail: unlink/release + event 0x01A0219E
```

The strongest supported semantic is **object lifecycle and status synchronization**. Returned
object class, resource population, terrain/stage consumer, actor activation and gameplay remain
Unknown.

## Evidence

Focused REA records for the selected authority:

- `ev_44f33e4eb0a1966fe9214fc006c069cd4614134d8058d5cebe2508d2e89d630a` — `EED640` status
  synthesis;
- `ev_e77cae7fff352858dc388e7ccdece1be53d02b8784aa16f0d4ef7097d21867e0` — `B228F0` lifecycle
  synchronization;
- `ev_b07040bb4fa60a4411e0134f1705d3bdaa4932845a7376d917008ca60dfe457a` — `B20620` list
  release/clear;
- `ev_c7a1391a300d3be0fbcfac19e9ded136fb16e486ce558f471f17ec92a1d627e0` — `E25BB0` vector
  compaction;
- `ev_0430baf0c71f296060a659cd0950a0359805fb87e1df8e78347d2b6e7be773bc` — `F25450` identity
  exclusion predicate;
- `ev_240b6c79573a7be1073f4d95cea8b3069e345236bd3dc1e1f0028db9d1c5faed` — `F25500` identity
  predicate;
- `ev_7ff3c95de198a366edb353afb777c579041dbe92c46a03c6d77de03c373b0dd0` — `F25520` status
  predicate;
- `ev_0f5c378615a916dd780ac0e79115785baa768783e5b257492438b07eec1a5238` — `F25560` status
  predicate.
