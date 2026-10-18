# Play Spore record reset and lifecycle signal

Reviewed **2026-10-17**. Static-only persistent Ghidra 12.1.4 extracts from
`refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe`, SHA-256
`086f299216f6a70d2c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.
Analyst roles below are not recovered original symbols. No GA process was launched.

## Findings and boundaries

| Question | Retained evidence | Result |
|---|---|---|
| What is removed before rebuilding cards? | [DF94E0 instructions](00df94e0.asm.txt), [C](00df94e0.c.txt) | Non-null record+B8 goes to DF9160; B4/B8 are zeroed; DE5E90 destroys contents and the logical span becomes empty. No backing allocation free in this body. |
| Are old records copied? | DF954E..DF956A in the same listing | No on this path: `CMP EDI,EDI; JZ` skips the apparent B26F90 copy loop. The decompiler explicitly marks it unreachable. |
| What does the content destructor do? | [DE5E90 instructions](00de5e90.asm.txt), [C](00de5e90.c.txt) | Walks stride BC and conditionally frees three record-owned buffers. It does not free the vector's backing storage. |
| Where is the new logical size set? | [DE9DD0 instructions](00de9dd0.asm.txt), [C](00de9dd0.c.txt) | Chooses grow/shrink helpers from requested count and the existing BC-stride span. Allocation success was not observed. |
| What is the builder order? | [DF95A0 instructions](00df95a0.asm.txt), [C](00df95a0.c.txt) | Reset, saved/candidate counts, resize, candidates at saved-count offset, saved at front, DF9200 UI data, tail jump DF8300 effects. No live vector values are captured. |
| When is 080D7451 sent? | [DF9620 instructions](00df9620.asm.txt), [C](00df9620.c.txt); [DF85B0 instructions](00df85b0.asm.txt), [C](00df85b0.c.txt) | Enter sends pointer to dword1 late in its body; exit sends pointer to dword0 before cleanup. These are static call-site observations, not a traced runtime sequence. |
| Who checks that ID? | [E03F50 instructions](00e03f50.asm.txt), [C](00e03f50.c.txt) | E04215 compares ID, reads the payload dword and calls E03F10 with ECX=listener+0C. Listener registration/delivery remain unresolved. |
| What does the boolean do? | [E03F10 instructions](00e03f10.asm.txt), [C](00e03f10.c.txt) | Writes receiver+D. False returns directly; true only reaches E03560/E03C00 if receiver+C==0 and the +34/+38 span is nonempty. No false-path cleanup, visibility toggle, catalogue readiness or state request is established. |

E03F10 field offsets are relative to its adjusted receiver, **not** to the listener pointer or
global GGE controller. The payload has no null check on the inspected handler branch. A return1
from E03F50 does not prove a listener was registered or that anything became ready.

## Reproduction

```powershell
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --publication
```

This opens the existing persistent project once and retains eight bounded C/instruction pairs.
Do not run another probe against that same project concurrently. It needs no import/reanalysis,
profile discovery or save write. These are local Ghidra extracts, not REA Evidence IDs.
