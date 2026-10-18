# Content loading — how the original reads its resources

Authority: disassembly of `refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`. This is the layer above
DBPF/RefPack: how the *game* asks for a resource by (type, group, instance) and gets bytes.

## The resource manager

- **`0x008de4b0`** returns the resource manager singleton from **global `0x1667aa0`** (a one-line
  getter; `0x008de4c0` is its setter). This is the object that owns the open packages and resolves
  resource ids to data.
- Resource queries go through the manager's vtable. `0x00847780` is the query used at a trigger
  site: it calls `0x8de4b0`, then `[mgr]->vtable+0x58` with the descriptor, then (on success) reads
  the resource and builds an object.

## The resource descriptor is `{group, type, ...}`

Sites build a small struct on the stack and pass it to a query:

```
5eb206:  mov [esp+0x28], 0x461227d      ; group
5eb20e:  mov [esp+0x24], 0x24a0e52      ; type = trigger
5eb21e:  call 0x686450                  ; query -> index, or 0xffffffff
```

```
6a9fa8:  mov [esp+0x28], 0x658b0ab2     ; group
6a9fb0:  mov [esp+0x2c], 0x24a0e52      ; type = trigger
6a9fbc:  call 0x847780                  ; load
```

## `trigger` = type id `0x024A0E52`

`upstream/SporeMaster3/data/reg_type.txt` lists `trigger 0x24a0e52`, and our own inventory found
`trigger` resources in the Czech NameGeneration package under `0x024A0E52`. The game references that
id as an immediate in exactly **three** places (via `tools/find_imm.py 0x024A0E52`):

| site | function | what it does |
|---|---|---|
| `0x005eb20e` | ~`0x5eb1de` | builds `{group 0x461227d, trigger}` and calls `0x686450` (query -> index or -1) |
| `0x006a9fb0` | ~`0x6a9f50` | builds `{group 0x658b0ab2, trigger}` and calls `0x847780` (load) |
| `0x007cc3ed` | ~`0x7cc3d9` | stores `{0x9a06678c, trigger}` in a class at `[this+0x14]/[this+0x18]` and calls `0x7cc1c0` |

The game's scripts/configs are `trigger` resources. The historical table below called all first
descriptor words group IDs; this is **not valid for SporeAppStates**, whose key is now pinned as
`{instance=0xD6396913, type=trigger, group=0}`. Resolving hashed identities against our naming
registry (`reg_file.txt`, 125,272 names, FNV-1-lower) gives names, but hashing alone does not tell
which resource-key field an identity occupies. Other historical descriptor labels below have
not been re-audited in this focused startup pass.

| hashed identity | resolved name | used at |
|---|---|---|
| `0x658b0ab2` | **`properties`** | the trigger load at `0x6a9fb0` — the game's **properties** config |
| `0x9a06678c` | **`ConfigManager`** | `0x7cc3ed` — the **ConfigManager** |
| `0xd6396913` | **`SporeAppStates`** | **instance**, group 0: `run` pushes at `0xf48505`, calls at `0xf4850a`; `67DCE0` manager vtable `01413698 +0x1C = 7E2370` — the app's **state list** |
| `0x461227d` | (not in the registry) | `0x5eb20e` — another config group |

So the startup path loads `SporeAppStates` (the state machine's states), and the config path loads
`properties` and `ConfigManager` — all as `trigger` resources. That answers "where the game reads its
config/state text": **through the resource manager (`0x8de4b0`), with a full resource key.**

## Status

- **Reversed:** the resource-manager getter (`0x8de4b0`/`0x1667aa0`), two query entry points
  (`0x686450`, `0x847780`), the `{group, type}` descriptor shape, and that the game loads `trigger`
  configs from three sites.
- **Not yet reversed:** the query functions' internals, the group-name resolution for those three
  sites, and where the loaded trigger text is parsed into game state.

**Startup-specific update:** the last item is partially superseded by
[startup-state-registration.md](startup-state-registration.md): `7E2370 → 847780` invokes the
parser; manager initialization registers `state`/`transition` handlers; `7E5B60` commits parsed
state records and indexes. Lexer/line dispatch and all state actions are not yet fully traced.
