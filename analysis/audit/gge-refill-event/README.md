# Refill event versus GGE refresh

Reviewed **2026-10-17**. Static GA evidence only; no original process was executed. The selected
binary SHA-256 is `086f299216f6a70d2c23be0fb04b4b7c570d3c23be0fb04b4b7c570d3bbd94ca0b26bcad53681b3e9fe7`.

## Result

`BB61C0` sends MessageManager event `0x062D91C2` after `BB4D40` writes the database. The now-seeded
vtable-only consumer at `0x00B29960` compares that event ID and calls `0x00B26770`; it does **not** call
`DF95A0`, `DF9740`, `DF9620` or a GGE layout method.

```text
B29960:
  if (message_id == 0x062D91C2)
      return B26770(this);

B26770:
  manager = B3D3A0();
  if (!BAAE90(manager)) {
      this[+0x46] = 1;
      00809E60(&0x01568A7C, &0x01568A70);
  }
```

The full bounded decompile/instruction pair is retained in this directory. The consumer also has
separate simulator/database branches for IDs `0x01CD20F0`, `0x0685DBFA` and `0x0689C9B9`; those
branches perform path/save or simulator work and are not merged into the refill-event conclusion.

The role of `+0x46`, `BAAE90` and the two `00809E60` arguments is not named. This is a
Simulator/StarManager-side reaction, not a proven GGE refresh or catalogue publication.

`DEA1B0` registers GGE controller listener `param+8` for 13 other message IDs, including
`0x04360E14`, `0x056BBD7F`, `0x044DB12E`, `0x05BD6378`, `0x05DD52C7`, `0x06203FDC`,
`0x04519B5F`, `0x0625D27D`, `0x67C75BD6`, `0x634297AD`, `0x275A9C8C`, `0xEDA117DE` and
`0x08085C4C`. The registration is corroborated by `DEA1B0` instructions, but no one of these
IDs is promoted to a GGE refresh meaning merely from registration. `DF95A0` remains statically
reached by Play Spore state enter/refresh and the controller route `DE32B0`.

## Reproduction

```powershell
python tools/find_imm.py 0x062d91c2 --ctx 12
python tools/ghidra_run.py --timeout 180 tools/ghidra_decompile.py 0x00B26770
python tools/ghidra_run.py --timeout 180 tools/ghidra_list.py 0x00B29960 40
python tools/ghidra_run.py --timeout 180 tools/gge_evidence.py --refill-event
```

The raw consumer listing is retained next to this note. The uncarved function boundary at
`B29960` is preserved as a listing observation rather than fabricated into a Ghidra function.
