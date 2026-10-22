# Scenario object factory/dispatcher boundary

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
The findings below are static shipped-binary observations. They do not claim native execution,
resource completion, terrain/world creation or gameplay readiness.

Continuation of [startup-gge-scenario-object-construction.md](startup-gge-scenario-object-construction.md).
The common-tail status and lifecycle boundary is detailed in
[startup-gge-scenario-object-finalizer-lifecycle.md](startup-gge-scenario-object-finalizer-lifecycle.md).

## `EEDD40`: identity dispatcher, not a single world factory

`EEDD40` first passes the source value through `EEC980`, which copies a three-word source tuple
into local state. `EEC980` has a special `powerupType` lookup path: when the resolved metadata has
the observed type value `10`, it rewrites the tuple identity to `0xF0000001`; otherwise it preserves
the tuple values. The identity is then compared against multiple constants.

The body contains separate branches for several identity values, including paths that:

- obtain/create a service object through `B20BF0`, `B23630`, `C9F1F0`, `EEBFD0` or related helpers;
- initialize or update type-specific data and call virtual methods on the selected object;
- create a different object path through `B23920`/`C09B40` for one identity family;
- fall back to a generic service/object lookup and failure notification when no identity branch
  matches.

These branches demonstrate a type/identity dispatcher with multiple object families. They do not
support naming every returned object as terrain, planet, stage or avatar.

## Common finalization after a returned object

When a branch returns a non-null object, the common tail:

1. obtains component/handle `0x01186577` from the returned object;
2. conditionally sets object flag bit `0x8000`;
3. calls virtual offsets `+0x38`, `+0x3C` and `+0x40` with source/transform-related values;
4. performs type-specific post-processing for several observed identity values;
5. stores caller parameters in object fields corresponding to `+0x1C` and `+0x18`;
6. when the third caller parameter is nonzero, calls `EED640` and sets a local/component byte;
7. calls `B3D400` and `B228F0` before returning.

The common tail is concrete object initialization and service synchronization. `EED640` synthesizes
status fields and `B228F0` performs object-list lifecycle checks/cleanup; neither establishes a
terrain or world materialization contract. The component and virtual method meanings remain
Unknown.

## `F237B0`: play-specific named lookup

The play-side branch used by `F3F8C0` calls `F237B0`. Static analysis observes the literal
`EP1_powerup_spawnerReincarnate` passed to a named lookup helper, followed by a service query and
`F212C0` setup. `F212C0`:

- derives a service/object context from the source value;
- for non-strategy-1 mode, obtains component `0x0175CDC9`, resets observed fields at `+0x3C`,
  `+0x40`, `+0x44` and byte `+0xE5`;
- obtains component `0x01186577` and invokes its setters at virtual `+0x38`, `+0x3C`, `+0x40`;
- releases the named lookup handle and returns the object context.

This establishes a named play-side lookup/setup edge associated with a powerup-spawner identity.
It does not establish actor spawn, avatar activation, terrain generation, stage activation or a
playable world. The exact ownership and runtime success of the named lookup remain Unknown.

## Separation from `F432C0`

`F432C0` has separate existing-marker and creation branches. If its owner-side marker pointer at
`+0x8C` exists, it updates marker vectors and calls `F3D930`. If absent, it allocates a
`Simulator/cScenarioMarker` record, copies source vectors and also calls `F3F8C0` with that
record. Thus it is **not marker-only**: the nested `EEDD40 → B23920 → cHerd +0x54` path can
conditionally assign a current creature. The
[herd assignment follow-up](startup-gge-scenario-herd-assignment-join.md) pins this join and
corrects the earlier scope limit. Neither record allocation nor conditional assignment proves
successful creature setup, captain identity, world materialization or playable readiness.

```text
F3F8C0
  ├─ play strategy → F237B0 → named powerup-spawner lookup/setup
  ├─ fallback      → EEDD40 → identity-dispatched object setup
  └─ transform diff → F432C0 → marker-record update/create, nested F3F8C0 on creation
```

## Evidence and residual Unknowns

Focused REA records for the selected authority:

- `ev_f69452b5a7cb234ebf334cf8266c5af85ba38081ea38445803fd21d642ab5ee3` — `EEDD40` dossier;
- `ev_dca29ef7addefc56bf1bc9cee0e58879ad59da6142700366a1a690d9291e34f4` — `EEDD40` instructions;
- `ev_ce210fadcb3813c920e91904dbc320b72d32060ecd6cc3db831982b6f97b0b2c` — `EEC980` tuple normalization;
- `ev_aedc3067197e156d7abbeac11af67f27b2587109b34f4eb390a4a84d5fa200b1` — `F237B0` and
  `EP1_powerup_spawnerReincarnate` lookup;
- `ev_06158c27982a819324f6b291a9ade9ba85fd4b11d265245048137158d562bf5f` — `F212C0` setup;
- `ev_a2c37f03ee9b365b59b1432f92e0ff817b5b9410664b294d12e15cfee94a62fc` — `F432C0` marker path.

Still Unknown:

- successful lookup/allocation and resource population;
- returned native class for each `EEDD40` identity branch;
- consumer join to terrain, planet, stage, actor or avatar creation;
- input activation, gameplay tick and playable-world readiness.
