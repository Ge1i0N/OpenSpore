# GGE Play Spore catalogue rebuild order and per-record presentation

Authority: selected GA-unpacked executable, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Static short disassembly only; no original runtime, save writer, content manager or UI callback
was executed. Selected instructions are pinned by
[`verify_startup_state_route.py`](../tools/verify_startup_state_route.py).
Continuation of [startup-gge-play-spore.md](startup-gge-play-spore.md).

## `DF95A0`: source counts, output allocation and placement

`DF95A0` begins by calling `DF94E0` on the current output vector at `this+8`. `DF94E0` releases
per-record UI owners at record offset `+0xB8`, clears those pointers, destroys each old `0xBC`
record and shrinks the output vector to zero.

The rebuild then obtains two source ranges:

```text
B3D3A0 → source manager +0xF0/+0xF4 → count A (existing/saved source)
BA6450(B3D3A0()) → second range begin/end → count B (candidate/new source)
```

The native arithmetic is:

```text
countA = ([manager+F4] - [manager+F0]) / 4
countB = ([range+4] - [range+0]) / 4
total  = countA + countB

DE9DD0(output, total)       resize/allocate total records, stride 0xBC
DF7CB0(output, base + countA*0xBC, countB)   write candidate/new region
DF7F30(output, base, countA)                 write existing/saved region
DF9200(output)                                create per-record UI data
DF8300(output)                                create marker/effect presentation
```

The destination calculation at `DF95E7..F4` passes `base + countA*0xBC` to `DF7CB0`, while
`DF7F30` receives the output base and `countA`. Thus the **output layout is saved/existing
records first, candidate/new records second**, even though the candidate writer is called first.
This is output placement, not a claim about source-manager enumeration order.

Each record is `0xBC` bytes. Consumers read position at `+0x80`, stage at `+0xA8`, flags at
`+0xAC/+0xAD`, and per-record UI owner at `+0xB8`. These are bounded consumer-based labels,
not recovered original symbols.

## Candidate/new writer `DF7CB0`

`DF7CB0` limits its source loop to the supplied count and reads candidate source elements from
the range returned by `BA6450`. For each candidate it calls `5C66D0(source)` and reads three
float values, adding the scene offset constant at `0147EA80` and scaling with `013F9458` before
passing them to the record builder.

The builder call at `DF7DAD` is followed by candidate source metadata queries and a source-specific
record method. The function writes each record into the supplied destination region with stride
`0xBC`; it does not prove that any candidate resource or UI object was successfully mounted.

## Existing/saved writer `DF7F30`

`DF7F30` iterates the existing source range from `B3D3A0`, calls `5C66D0(source)` for the same
position-shaped triple, and reads additional metadata through source-manager interfaces. Its
output destination starts at the base supplied by `DF95A0`, so it fills the saved/existing prefix.
Missing dependencies do not justify synthesizing an empty ready catalogue.

This is the static source split behind the retained “saved then candidate” catalogue statement.
It must not be confused with later UI projection or with a save write path.

## `DF9200`: per-record UI data creation

After both source writers return, `DF9200` loops over all `total` records. For each record it:

1. releases an old `record+0xB8` owner if present;
2. allocates a `0x90`-byte `UI/GGE_game_ui_data` object;
3. stores the record pointer at the UI-data object and retains the object in `record+0xB8`;
4. loads the configured `GGEGameStarUIv4` layout name from `016A0D08` through `DF7690`;
5. selects stage configuration using record stage `+0xA8` and applies bounded widget/control
   flags through `810650`.

The UI-data object is per-record and follows the rebuilt output array. A pointer write to
`record+0xB8` is not evidence that `UI/GGEGameStarUIv4` mounted successfully or that its root
window was attached.

## `DF8300`: marker/effect presentation

`DF8300` iterates the same `0xBC` records, skips records whose flag at `+0xAD` is clear, and
chooses marker effects using the flag at `+0xAC` and stage `+0xA8`:

```text
0xCC1D3EEB
0xF0EB627C
0x2283C798
```

It calls the controller/effect path with record position `+0x80`, applies the scene transform
and retains effect/presentation state. This is downstream of record construction and separate
from `DF6010`'s later camera/viewer projection. It does not create terrain, a stage world or an
actor/avatar.

## Resulting bounded route

```text
DF9740 state-2 gate
  → DF95A0
      → clear old records/UI owners
      → count existing + candidate sources
      → resize total 0xBC records
      → candidate writer into suffix
      → saved writer into prefix
      → per-record 0x90 UI data
      → marker/effect presentation
  → DF6C10 continuation
  → later DF6010 viewer projection
```

This closes the static catalogue placement and construction order. Unknown remains explicit for
source manager contents, successful layout/effect attachment, camera/viewer production, object-74
Scenario launch conversion, native ScenarioMode delivery, stage/world construction and gameplay.
