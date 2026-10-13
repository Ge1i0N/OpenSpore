r"""Address guesser: propose oracle addresses for beta-PDB names, using STRING references.

The honest situation (verified):
  * the beta PDB has NAMES but no code, and is a DIFFERENT link (0/12 addresses match);
  * no reference binary matches the GA oracle (all Also* builds have different CodeView GUIDs);
  * the GA build is /GR- (RTTI off) and has almost no assert function-name strings.
So a full address map is NOT derivable. What IS derivable is a set of CANDIDATE matches from a
strong, verifiable signal: a function that references a distinctive string, matched to a PDB
function name that contains that same token. Example: the codec registers the name "Refpack"
(oracle string 0x0143e5fc), so the function referencing it is a Refpack method.

Method:
  1. For every `push imm32` / `mov r32, imm32` in .text whose target is a printable .rdata string,
     attribute the string to the enclosing candidate function (nearest start <= the instruction).
  2. Tokenise PDB names (Namespace::Class::Method) into distinctive tokens.
  3. For each (function, string) where the string is distinctive, find PDB names containing it.
  4. Emit candidate (address, pdb name, driving string, score). Score = string length * (rarity).

Nothing here is trusted as truth; it is a ranked GUESS list for a human to check, and it is the
reusable half of a matcher (the other half needs a named reference binary).

Usage: python tools/name_guess.py [--min-len 6] [--top 200]
"""
from __future__ import annotations

import argparse
import bisect
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT))

NAMES = ROOT / "analysis" / "pdb_names.txt"
TOKEN = re.compile(r"[A-Za-z][A-Za-z0-9_]{4,}")


def load_pdb_names():
    if not NAMES.exists():
        print("run tools/pdb_symbols.py first (needs analysis/pdb_names.txt)")
        raise SystemExit(2)
    names = [n for n in NAMES.read_text(encoding="utf-8").splitlines() if n]
    return names


def build_token_index(names):
    """token (lowercase) -> set of name indices. Only keep tokens that are distinctive enough."""
    idx = defaultdict(set)
    for i, n in enumerate(names):
        for t in set(TOKEN.findall(n)):
            idx[t.lower()].add(i)
    return idx


def oracle_string_refs(pe, starts):
    """Return (func_addr -> list of (string, string_va))."""
    text = pe.sections[".text"]
    t0 = text["va"]
    blob = pe.section_bytes(".text")
    refs = defaultdict(list)
    order = sorted(starts)

    def enclosing(addr):
        i = bisect.bisect_right(order, addr) - 1
        return order[i] if i >= 0 else None

    def read_str(va):
        sec = pe.section_of(va)
        if sec is None or sec["name"] not in (".rdata", ".data"):
            return None
        try:
            b = pe.cstr(va, 64)
        except Exception:  # noqa: BLE001
            return None
        if len(b) < 3 or not all(32 <= c < 127 for c in b):
            return None
        return b.decode("latin-1")

    # push imm32 (0x68) and mov r32, imm32 (0xB8..0xBF)
    for i in range(len(blob) - 5):
        op = blob[i]
        imm_off = None
        if op == 0x68:
            imm_off = i + 1
        elif 0xB8 <= op <= 0xBF:
            imm_off = i + 1
        if imm_off is None:
            continue
        va = struct.unpack("<I", blob[imm_off : imm_off + 4])[0]
        s = read_str(va)
        if not s:
            continue
        fn = enclosing(t0 + i)
        if fn is not None:
            refs[fn].append((s, va))
    return refs


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--min-len", type=int, default=6)
    ap.add_argument("--top", type=int, default=200)
    a = ap.parse_args()

    from tools.pe import PE
    from tools.functions import candidate_starts

    pe = PE()
    starts = candidate_starts(pe)
    names = load_pdb_names()
    tokidx = build_token_index(names)
    print(f"# oracle {pe.path.name}: {len(starts):,} candidate functions")
    print(f"# PDB names: {len(names):,}")

    refs = oracle_string_refs(pe, starts)
    print(f"# functions with string refs: {len(refs):,}")

    # how many FUNCTIONS reference each string (specificity): a string used by one function is a
    # far better address guess than one used by twenty (e.g. "Pollinator").
    freq = Counter()
    for _fn, ss in refs.items():
        for s in {s.lower() for s, _ in ss}:
            freq[s] += 1

    cands = []  # (score, tier, func_addr, string, pdb_name)
    for fn, ss in refs.items():
        seen = set()
        for s, _va in ss:
            sl = s.lower()
            if len(s) < a.min_len:
                continue
            if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]{2,}", s):  # plain identifier-ish strings only
                continue
            if sl in seen:
                continue
            seen.add(sl)
            ids = tokidx.get(sl)
            if not ids:
                continue
            r = len(ids)                 # how many PDB names share this token
            if r > 50:
                continue
            f = freq[sl]                 # how many oracle functions reference this string
            score = len(s) * (50.0 / r) / f
            tier = "HIGH" if (f == 1 and r <= 10 and len(s) >= 8) else "med"
            for i in ids:
                cands.append((score, tier, fn, s, names[i]))

    cands.sort(key=lambda c: -c[0])
    # keep the best candidate per function
    best = {}
    for score, tier, fn, s, nm in cands:
        if fn not in best:
            best[fn] = (score, tier, s, nm)

    highs = sum(1 for v in best.values() if v[1] == "HIGH")
    print(f"# candidate (address -> PDB name) guesses: {len(best):,}  (HIGH confidence: {highs})")
    rows = sorted(best.items(), key=lambda kv: -kv[1][0])[: a.top]
    print(f"{'address':>10}  {'tier':>4}  {'score':>7}  {'string':<30} name")
    for fn, (score, tier, s, nm) in rows:
        print(f"0x{fn:08x}  {tier:>4}  {score:7.1f}  {s[:30]:<30} {nm[:66]}")

    # write the artifact
    lines = [
        "# Address guesses — oracle address -> beta-PDB name (by string reference)",
        "",
        "> Generated by `tools/name_guess.py`. These are **guesses**, ranked by a verifiable signal:",
        "> the function references a distinctive string that also appears in the PDB name. NOT truth.",
        "> A full address map is not derivable (no beta code, no matching reference build, RTTI off);",
        "> this is the strongest self-contained signal we have. Check the top rows by disassembly.",
        "",
        f"- candidate functions: {len(starts):,}",
        f"- functions with string refs: {len(refs):,}",
        f"- candidate matches: {len(best):,} (HIGH confidence: {highs})",
        "",
        "| address | tier | score | driving string | proposed PDB name |",
        "|---|---|---|---|---|",
    ]
    for fn, (score, tier, s, nm) in rows:
        lines.append(f"| `0x{fn:08x}` | {tier} | {score:.1f} | `{s}` | `{nm}` |")
    lines.append("")
    (ROOT / "analysis" / "name_guesses.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"\nwrote analysis/name_guesses.md ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
