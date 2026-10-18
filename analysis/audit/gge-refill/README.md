# GA StarManager candidate refill evidence

Reviewed **2026-10-17**. Static-only persistent Ghidra extracts from the selected GA-unpacked
binary. No original process was executed, no save was written and no candidate generator was run.

## Recovered flow

`BB61C0` is the candidate-refill body, not a static package enumerator. Its bounded instruction
order is:

```text
BB2580
  → if available-starter-world vector +A0 is empty, BAC7B0(0) repopulates it
  → inspect candidates +DC/+E0 and count entries whose BB9C60(2) state bit is clear
  → combine saved count +F0/+F4 with that count
  → quota = min(global maximum - current count, global refill target - current eligible count)
  → scan possible-start vector +118/+11C through BB24D0
  → fallback BB3E10 generates a position/candidate when no usable possible-start entry remains
  → clear candidate state via BB9C40/BB9C70 and remove a selected starter-world from +A0
  → BA8980 performs planet work
  → AddRef and append the candidate to +DC/+E0
  → if changed and controller byte +0x64 is clear, BB4D40 writes stars.db
  → MessageManager vtable +0x18 sends event 0x062D91C2
```

The exact quota values are runtime globals `0x0156C518` and `0x0156C508`; their configuration
producer is not established here. The position generator uses random coordinates constrained by
global radius values, rejects proximity to saved/candidate positions using threshold `0x013EC4B0`,
and bounds its retry loops at 1000 attempts. This is why persisted starters cannot be promoted to
the post-refill candidate vector without executing or separately observing the producer.

`BB24D0` rejects candidates when the StarManager state/flag checks fail, when global state
`B1FD00` is active, when the candidate is the Sol record (`+0x148`), when `BB2350` rejects the
record, or when an owned candidate fails the observed planet/count condition. `BB9C60` reads
`object+0x5C & mask`; `BB9C40` writes `object+0x4C`; `BB9C70` sets/clears `object+0x5C` bits.

## SDK cross-check

`Spore/Simulator/SubSystem/StarManager.h` corroborates the GA field layout:

```text
+A0  mAvailableStarterWorlds
+DC  mStarterStarRecords
+F0  mSavedGameStarRecords
+118 mPossibleStartLocations
+228 planetRecords.pkp database
```

The SDK method names are useful semantic leads, but its address table does not map literally to this
GA build. For example, the SDK's `0xBB61E0 cStarManager_Dispose`, `0xBAB590 CollectStarterWorldIDs`,
`0xBA7690 GetStarterWorldGeneratedTerrainKey` and `0xBB59C0 Initialize` land on different GA bodies
(`BB61C0` refill, `BAB510`, `BA7660` and `BB5950` respectively). No name is promoted from those
address mismatches.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --refill
```

The generated C/instruction pairs in this directory are bounded evidence and do not imply runtime
availability of the StarManager, terrain services, quota globals or refill completion.
