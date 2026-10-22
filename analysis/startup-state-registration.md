# Startup: SporeAppStates consumer and state registration

Authority: read-only short `objdump -d -M intel` ranges and file-backed PE bytes from
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
No game process, native parser, resource writer or save path was executed.
Selected instructions/vtables are pinned by
[`tools/verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
These are static facts and bounded interpretations, not runtime delivery observations.

## 1. Resolve the object, not just the slot number

The recent startup notes conflated AppMode's graphics-device slot with its separate state manager.
The concrete identity chain is:

```text
AppMode setup
  7E703C: call 7E5870                 allocate 0x174, ctor 7E52D0
  7E7056: store manager at AppMode+0x34
  7E7075: manager virtual +0x10       initialize parser/handlers
  7E707B: call 67DEF0(manager)        publish global 015FD8B4

67DCE0: return [015FD8B4]
67DEF0: [015FD8B4] = argument

manager ctor 7E52D0: vtable 01413698
  +0x10 = 7E54E0    initialization
  +0x1C = 7E2370    trigger load + initial-state selection
  +0x28 = 7E3BF0    state switch/request entry
  +0x38 = 7E30E0    name-to-ID lookup
```

`015FD8B4` is a non-file-backed global: its runtime value cannot be read from the PE. The chain
above follows the native setter and producer, not an invented static pointer value. The byte scan
found setter calls at `7E707B` and cleanup site `7E6650`; this is not an exhaustive indirect-write
proof. **AppMode** has vtable `01413B50`, where `+0x1C = 7E7430` creates the graphics device.
It is not the state-list consumer.

## 2. Parser and grammar handlers exist before the trigger load

Manager initialization `7E54E0` obtains a parser through `8409A0` (allocates `0x1C0`, constructor
`847540`, vtable `0141CAD0`) and retains it at manager `+0x158` (`7E5518`). At `7E5540..47` it
passes `manager+0x10` through parser virtual `+0x10`.

It constructs the following block handlers and registers them through parser virtual
`+0x14 = 8460B0`:

| verb source | handler vtable | open callback | close callback |
|---|---|---|---|
| `[0153F5F0] → 01413194`, `state` | `01413260` | `7E3FB0` | `7E5B60` at `+0x14` |
| `[0153F5F4] → 01413188`, `transition` | `014133EC` | `7E35A0` | `7E5C70` at `+0x14` |

The two registrations are at `7E5639` and `7E5653`. `8460B0` stores a handler in its registry
and invokes handler `+0x08 = 7E1C10`. That callback receives the owner/context pointer and stores
`context-0x10` at handler `+0x30` (`7E1C32..39`). This connects handler state writes to the same
manager, rather than a second guessed state table.

## 3. Resource loading reaches the parser

At `F484F5`, `run` calls `67DCE0`, then makes the following four-argument call at `F4850A`:

```text
manager +0x1C = 7E2370(instance=0xD6396913, group=0, launchArgs=edi, flag=1)
```

`F48505` is the **push**, not the indirect call. `7E2370` builds a three-dword key in
`{instance, type=0x024A0E52, group}` order, stores the final byte argument at manager `+0x81`,
and calls `847780(parser=[manager+0x158], key)` at `7E23B9`.

`847780` is more than a resource-object constructor:

1. `84779C..AD`: get resource manager through `8DE4B0`, query virtual `+0x58` with the key.
2. `8477D6`: obtain a resource/stream interface through virtual `+0x34`; failure skips parsing.
3. `8477EA`: obtain the input pointer through that interface's `+0x18`.
4. `84785D/84786C`: install `include` and `sinclude` handlers through parser `+0x14`.
5. `847875`: parser `+0x30 = 846FC0` preparation.
6. `84787F`: parser `+0x4C = 841360(input)`; that wrapper invokes parser `+0x3C` and returns a
   Boolean, with a separate failure/exception path.
7. `847893`: parser `+0x34 = 844F70(!result)` finalization, then release temporary interfaces.

`7E23C1..D4` returns immediately when the helper returns false. Thus selecting the initial state
is conditional on this static parse-success branch. No successful resource mount or parse was
observed at runtime. The lexer and line-dispatch body behind parser `+0x3C` remain to be carved;
the handler registry and the callback endpoints are established separately here.

## 4. A parsed state becomes indexed records

The already recovered `state` open callback `7E3FB0` stages the name and actions on the manager,
sets manager `+0x28` to either explicit `-id` or FNV-1-lower(name), checks for duplicate IDs in the
map at `+0xB8`, then pushes the handler onto the parser block stack through `+0x8C = 845390`.
See [states.md](states.md) for the explicit-ID semantics.

On the zero-argument-value branch of close callback `7E5B60`:

- `7E5B75..9D` computes the current index from `(end-begin)/0x50` in manager `+0xA4/+0xA8`,
  then appends the staged record at manager `+0x2C` through `7E5AE0` (copy ctor `7E2B90`).
- `7E5BA9` transforms the staged name through `840A80`; the name-index map at `+0xF8` then
  receives the ID (`7E5BC6`). Exact string-folding semantics of `840A80` are not re-audited here.
- `7E5BD5..DA` inserts the ID into the `+0xB8` map and stores the record index as its value.
- If the previous-state ID at `+0x7C` resolves, `7E5C24` writes this ID to the previous
  `0x50`-byte record's `+0x4C`; `7E5C2F` updates `+0x7C` to the current ID.
- The common tail clears the staged name and resets manager `+0x28` to `0xFFFFFFFF`.

The nonzero branch skips the record/index commit and goes to the common tail. Calling this
argument an error flag is plausible but remains an interpretation until its producer is traced.
Do not confuse the manager's **staged identity at +0x28** with an independently allocated
state object: the callback accesses the manager through handler `+0x30`.

Name lookup `7E30E0` queries map `+0xF8` and returns its stored ID, or `0xFFFFFFFF` if missing.
It does not simply hash every requested name, which would fail for explicit-ID `Main`.

## 5. Initial-state selection is inside the consumer

After successful parsing, `7E2370` selects states in this order:

1. If `-preload` is present: look up `Preload`; if found, invoke manager `+0x28`.
2. Continue to the normal choice (there is **no early return** after the Preload request):
   - getter `67DCC0` object's virtual `+0x50` true: choose `Demo`;
   - otherwise if `-state` is supplied: convert its UTF-16 value through `93C4E0`, look up it;
   - otherwise: look up `Main`.
3. If that lookup is not `0xFFFFFFFF`, call manager `+0x28` at `7E24C5`.

If an explicit `-state` lookup fails, this code does not fall back to Main. It skips the request.
The demo predicate's producer is outside this focused audit. This is static selection/request
logic, not proof that the requested state, GGE UI or a playable world completed activation.

## Next boundary

The coarse startup gap is now connected: native manager creation → grammar registration →
trigger parser invocation → state-record indexing → initial-state request. The continuation
[startup-state-dispatch.md](startup-state-dispatch.md) traces parser `+0x3C`, callback resolution,
the switch/action executor and `Main`'s message-based `gotoState` to the mode activation endpoints.
Message delivery/order and every runtime outcome remain Unknown.
The lexer, every action handler, actual mounts, graphics/world readiness and Scenario launch
delivery are not claimed complete by this audit.
