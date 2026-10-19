# Scenario object state synchronization and invalidation

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static shipped-binary evidence only. No native execution, resource completion, terrain/world
creation or gameplay tick was observed.

Continuation of [startup-gge-scenario-object-finalizer-lifecycle.md](startup-gge-scenario-object-finalizer-lifecycle.md).

## `F3D930`: record/component synchronization

When the supplied object is the owner marker at `owner +0x8C`, `F3D930` copies its vectors and
scalar into ScenarioMode backing storage:

```text
object +0x04/+0x08/+0x0C → owner-side +0x150/+0x154/+0x158
object +0x10/+0x14/+0x18/+0x1C → owner-side +0x15C..+0x168
object +0x20 → owner-side +0x16C
```

It then resolves the associated service object through `F3D620`, obtains component
`0x01186577`, and calls virtual setters `+0x38`, `+0x3C` and `+0x40` with the record vectors and
scalar.

Additional component lookups for identities `0xB033B403` and `0xD0036E08` perform type-specific
post-processing through `C3ABA0`, `C046F0`, `C6BBC0` and `EECBD0`. The exact component classes and
their side effects remain Unknown.

The function then calls `B3D410`, `B48870`, and `EED640`, stores the resulting status at a component
record near `+0xA8`, sets byte `+0xA7 = 1`, and invokes owner virtual `+0x84`. If the object is
still the current marker, selected vector fields are copied back into the owner backing storage.

This establishes a bidirectional state/status synchronization loop. It does not establish terrain
materialization or a world/stage activation boundary.

## `B22C50`: component update and invalidation

`B22C50` obtains a service object from the supplied source and component `0x01186577`. Optional
arguments are applied through component virtual setters `+0x38` and `+0x3C`. If the source exposes
the observed fields/tags:

```text
0x03D08E15 → component byte +0x71 = 0
0x93336C12 → component field +0x58 = 0
0x4A0CA460 → component field +0x8C = 1.0f
```

It then invokes an object virtual predicate at `+0x24`. A false result calls `B22560`, which queues
the object for release/cleanup and invokes `B20CC0`. This is an object validity/lifecycle edge;
the tags' semantic names and their relation to world construction remain Unknown.

## Gameplay-adjacent phantom policy

`B48870`, reached from the synchronization path, observes the configuration key
`useGameplayPhantom`. Depending on that key, object identity and controller mode, it invokes
phantom-related component methods and update helpers. This is evidence of a gameplay-adjacent
phantom policy, not evidence that a gameplay tick, actor spawn, input activation or playable stage
was reached.

```text
F3D930
  → vectors/component setters
  → EED640 status synthesis
  → B48870 phantom-policy branch
  → owner callback +0x84

B22C50
  → optional component updates/tags
  → virtual validity predicate +0x24
  ├─ pass: return object
  └─ fail: B22560/B20CC0 release queue
```

## Evidence and residual Unknowns

Focused REA records for the selected authority:

- `ev_4975bf661ce7cdbb95de0c1e914a1a325009b91d8fd69bb0e54f5b58b1c613c7` — `F3D930` state
  synchronization;
- `ev_c7ce962ee7b22f685f0fb2f16841b0c75d5b923eb7366b38bf956fad70ea` — `B22C50` component
  update/invalidation;
- `ev_4120b4e4d11b2c3807c06ebbdc1c808dcc5922cd713d58ae75495ae2f3b0825a` — `B22560` release
  queue;
- `ev_6d1c8bca771c7bb51e8375392039dee74be5aab0e6fd9fd3232a7466c1478a54` — `B20CC0` object
  invalidation and event publication;
- `ev_1b53857d8179ca457df2f7d5741d086dfa0f5842ee6dbaaed4dc062c6acc3af1` — `B48870`
  `useGameplayPhantom` policy;
- `ev_de5f1096c739e3d5bda3619df82ceb92c080d02b90a3a16a1148cd01a976fa35` — `F3D620` lookup.

Still Unknown:

- component class meanings and successful runtime state;
- semantic names of source tags and status values;
- consumer join to terrain, stage, actor or avatar creation;
- input activation, gameplay tick and playable-world readiness.
