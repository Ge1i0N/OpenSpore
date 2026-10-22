# ScenarioPlayMode player request: factory dispatch join

Authority: `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Read-only `objdump -d -M intel` and PE-byte inspection. No native process, save writer or Ghidra
analysis was run in this pass. Conditional static reachability is not runtime instantiation.

Continuation of [startup-gge-scenarioplay-service-phase-progression.md](startup-gge-scenarioplay-service-phase-progression.md).

## 1. Publish the factory service

Inside startup body `F47E90`, the sequence at `F47EFA..F47F29` allocates `0x148` bytes through
`F47240`, constructs through `920960` when non-null, stores the pointer at startup-owner `+0x24`,
and passes it to `920330`. The setter writes global `0x01667B84`; getter `920320` returns it.
Constructor `920960` installs vtable `0x0143D9BC`:

| Slot | Target | Observed role |
|---:|---|---|
| `+0x04` | `920DC0` | Register a class factory and its enumerated class IDs. |
| `+0x14` | `920C50` | Select a registered factory by numeric class ID. |
| `+0x20` | `9203A0` | Create object, then query requested interface. |
| `+0x24` | `920360` | Select/use class factory, then invoke its creation method. |

This pins the receiver of the virtual call used by `B20BF0`. It does not prove the startup call
ran or the global remained populated. The teardown-side call at `F47798` passes zero to the same
setter before releasing the owned service.

## 2. Register the Simulator class factory

`B221A0` lazily initializes the class-factory singleton at `0x0167C30C`, setting vtable
`0x0145ECAC`. Its relevant slots are:

| Slot | Target | Observed role |
|---:|---|---|
| `+0x04` | `B21060` | Enumerate keys from class-factory table `+0x10`. |
| `+0x08` | `B210D0` | Lookup class ID and invoke the stored function pointer. |

`B25CB0` obtains this singleton and calls `B24720` to fill its constructor table. In that body,
`ESI` is advanced by `0x10`; the previously recovered key/value writes therefore populate the
same table read by `B21060/B210D0`:

```text
classFactory +0x10 table:
  0x02C21781 → B1E960 → allocation "Simulator/cPlayer" → C7B930
```

After filling the table, `B25CB0` obtains the singleton again and calls `692810`. If its byte
`+0x0C` is clear and the global factory service is non-null, `692810` calls service virtual
`+0x04(classFactory, classFactory +0x08 value)` and stores the returned byte at `+0x0C`.
`692F20`, used during singleton construction, initializes that priority/value field to the passed
zero and the registration byte to zero.

`920DC0` enumerates numeric IDs through class-factory slot `+0x04` into a bounded local array
(capacity argument `0x80`) and inserts tuples containing class ID, factory pointer and supplied
priority into the service table at `+0x108`. Successful enumeration/registration is unobserved;
this does not assert the final runtime table population.

## 3. Dispatch the player request

The request site is unchanged: `B20F40 → B20BF0` calls service slot `+0x20` with class ID
`0x02C21781`, interface ID `0x017F243B`, and two zero arguments. With the pinned service vtable:

```text
B20BF0
  → 9203A0 (service +0x20)
      → 920360 (service +0x24)
          → 920C50 (service +0x14), because supplied factory argument == 0
          → selected classFactory +0x08(classID, trailing argument)
              → B210D0 for Simulator singleton
                  → lookup classID in singleton +0x10
                  → call entry +4
                      → B1E960 for 0x02C21781
                          → F47240(0x12D8, "Simulator/cPlayer", ...)
                          → C7B930 when allocation is non-null
      → returned object +0x0C(interfaceID 0x017F243B)
```

`920C50` matches numeric class ID and selects the factory with the greatest **signed** priority
among matching entries (a later equal/lower priority does not replace the current choice). Thus
this is a proven conditional dispatch route, not a claim that Simulator wins against every other
possible runtime registration.

`B210D0` returns null on a missing constructor-table entry; otherwise it invokes the entry's
function pointer. `B1E960` returns null on failed allocation. `9203A0` returns null when creation
fails. For a non-null object it calls object virtual `+0x0C` with the requested interface ID; if
that query fails, it calls object slots `+0` and `+4`, then returns null. A successful query returns
the interface pointer, not a universal world-ready result.

For this constructor family, `C7B930` installs object vtable `0x01472CC8`, whose `+0x0C` slot is
`C755D0`. That method handles `0x02C216ED` directly and delegates other IDs to `B183A0`.
`B183A0` accepts `0x017F243B` and returns the receiver. This closes the requested interface path
for the constructed cPlayer family, conditional on successful construction and selection.

## Result and limits

- **Established statically:** service publication/vtable, Simulator singleton registration path,
  numeric class-ID selection, constructor-table invocation, and cPlayer interface-query route.
- **Conditional:** the Simulator factory is registered and selected for `0x02C21781`, allocation
  succeeds, and the constructor returns the expected object family.
- **Unknown:** actual native instantiation, resource/avatar population, world/terrain/stage
  attachment, input activation and completed gameplay tick.
- **Unchanged:** object-74 `Play Scenario 0x070D7509` delivery/producer join is not answered here.

The returned player's `+0x28(0)` initialization callback (`C7C870` for this vtable) is now bounded in
[player initialization](startup-gge-scenarioplay-player-initialization.md): collectables setup,
listener registration and a separate service query. It does not establish avatar/world attachment.
No native initialization callback was executed.

## Reproduction scope

Instruction anchors: `920320/920330`, `F47EFA..F47F29`, `920960`, `692810`, `B221A0`,
`B25CB0`, `B24720..B25C06` (constructor registration table), `920DC0`, `920C50`, `920360`,
`9203A0`, `B21060/B210D0`, `C755D0`, and `B183A0`. The selected calls, slots, branches and
data transfers are pinned in `tools/verify_startup_state_route.py`. Raw byte scans were used only
to find candidates; decoded instructions and explicit vtable slots support the conclusions.
