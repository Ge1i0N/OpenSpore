"""Prove that SporeApp.exe.unpacked.exe's .text is a *correct* decryption.

Low entropy alone proves nothing. Two things make this decisive:

  1. Cross-section agreement. `.rdata`/`.data`/`.rsrc`/`.reloc`/`.tls`/`CONST` of the
     unpacked file are **byte-identical** to the encrypted original (sha256, see
     analysis/build_identity.md). So if the decrypted `.text` embeds addresses that land on
     meaningful things in those untouched sections, that cannot be coincidence.
  2. A **control**. Every metric below is computed twice: once on the unpacked `.text` and
     once on the encrypted original's `.text`, which is known garbage. A metric only counts
     as evidence if the two separate by orders of magnitude. Without the control, a
     "plausible-looking" number is just a number.

Metrics (all properties of real MSVC x86 that a wrong key destroys):

  A. int3 padding run **ends** 16-aligned (the next function start is the aligned edge).
  B. exact branch census from a *linear* disassembly window: every call/jmp target inside
     .text and off a 0xCC byte.
  C. absolute `ds:0x...` operands inside the image; the offenders are printed, not averaged.
  D. IAT slot reachability: the slot address appears as a 4-byte immediate somewhere in
     .text (a `jmp [slot]` thunk is only one of several forms - FF 15 call [slot],
     push [slot], mov reg,[slot] all count).
  E. known .rdata string addresses appear as immediates in .text.
  F. function starts (the aligned ends of int3 runs) begin with a real opcode.
  G. .text byte-for-byte: how much of the decrypted .text differs from the encrypted one
     (should be ~all of it, and the diff must not be a repeating pattern).

Method notes at the bottom record the wrong invariants an earlier revision of this file
used, so nobody re-derives them.

Usage: python tools/verify_unpacked.py
Writes: analysis/unpacked_verification.md
"""
from __future__ import annotations

import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

_ROOT = Path(__file__).resolve().parent.parent
if str(_ROOT) not in sys.path:
    sys.path.insert(0, str(_ROOT))

import pefile

from tools.pe import PE

ORIG = _ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe"
UNPACKED = _ROOT / "refs" / "bins" / "ReleaseGA" / "SporeApp.exe.unpacked.exe"
OBJDUMP = Path(r"C:\Users\DartPower\scoop\apps\mingw\current\bin\objdump.exe")
OUT = _ROOT / "analysis" / "unpacked_verification.md"

# Strings whose addresses must be referenced by the decrypted code if it is genuine.
# ('u' = the bytes there are UTF-16LE, 'a' = ASCII)
CROSS_REF_STRINGS = {
    "devDirs": (0x013F8DC8, "u"),
    "shipDirs": (0x013F8DB4, "u"),
    "Stack trace is:": (0x013F8CB0, "a"),
    "SP_Havok": (0x013F8D24, "a"),
    "utfres": (0x013F8D94, "u"),
    "UI/MVJ/FileCache": (0x013F8EA8, "a"),
    "Failed to initialize palette layout page %x": (0x013F8368, "a"),
}

X86_START_OK = set(
    [0x55, 0x8B, 0x56, 0x57, 0x51, 0x52, 0x53, 0x50, 0xA1, 0xE8, 0xE9, 0xEB, 0xCC, 0xC3, 0xC2,
     0x81, 0x83, 0x33, 0x68, 0x6A, 0x0F, 0xFF, 0x8D, 0x3B, 0x32, 0x3A, 0x2B, 0x25, 0x03, 0x01,
     0x85, 0x84, 0x38, 0x39, 0x24, 0x04, 0x08, 0x28, 0x29, 0x30, 0x31, 0x3C, 0x3D, 0x3E, 0x36,
     0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x66, 0x67, 0xF0, 0xF3, 0xF6, 0xF7, 0xFE,
     0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
     0xB8, 0xB9, 0xBA, 0xBB, 0xBE, 0xBF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7,
     0xC6, 0xC7, 0xC1, 0xD1, 0xD3, 0xE3, 0xE2, 0xE0, 0xE1, 0xA0, 0xA8, 0xA9, 0xAB, 0xAC, 0xAD,
     0xAE, 0xAF, 0xAA, 0x8A, 0x88, 0x89, 0x8C, 0x8E, 0x8F, 0x86, 0x87, 0x90, 0x91, 0x92, 0x93,
     0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9C, 0x9D, 0x9E, 0x9F, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC,
     0xDD, 0xDE, 0xDF, 0x2C, 0x2D, 0x37, 0x35, 0x18, 0x1C, 0x1D, 0x1E, 0x1F, 0x38, 0x22, 0x27,
     ]
    + list(range(0x40, 0x50))  # inc
)


def disasm(path: Path, lo: int, hi: int) -> str:
    cmd = [str(OBJDUMP), "-d", "-M", "intel", f"--start-address=0x{lo:x}", f"--stop-address=0x{hi:x}", str(path)]
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace").stdout


def read_text(path: Path) -> tuple[PE, bytes, int, int]:
    pe = PE(path)
    t = pe.sections[".text"]
    return pe, pe.section_bytes(".text"), t["va"], t["va"] + t["vsize"]


def metrics(target: Path, control: Path, window: int = 0x10000) -> dict:
    """Compute every metric on `target`, and the same on `control` (the encrypted .text)."""
    out: dict = {}
    for label, path in (("unpacked", target), ("encrypted", control)):
        pe, blob, t0, t1 = read_text(path)
        m: dict = {"pe": pe, "blob": blob, "t0": t0, "t1": t1}

        # A. int3 run ends aligned
        runs = [(mm.start(), len(mm.group())) for mm in re.finditer(rb"\xcc{2,}", blob)]
        ends = [t0 + off + n for off, n in runs]
        m["runs"] = runs
        m["ends"] = ends
        m["A_aligned"] = sum(1 for e in ends if e % 16 == 0)
        m["A_total"] = len(ends)

        # B/C. exact, from a linear disassembly window
        txt = disasm(path, t0, min(t1, t0 + window))
        insn = re.compile(r"^\s+(?P<addr>[0-9a-f]+):\s+(?:[0-9a-f]{2} )+\s+(?P<mnem>[a-z0-9]+)\s*(?P<ops>.*)$", re.M)
        m["insns"] = 0
        m["B_total"] = m["B_bad"] = 0
        m["C_abs"] = 0
        m["C_bad"] = []
        m["D_slotrefs"] = {}
        m["hist"] = Counter()
        img_lo, img_hi = pe.base, pe.base + pe.pe.OPTIONAL_HEADER.SizeOfImage
        data, rdata = pe.sections[".data"], pe.sections[".rdata"]
        for mm in insn.finditer(txt):
            mn, ops = mm["mnem"], (mm["ops"] or "").strip()
            m["insns"] += 1
            if mn in ("call", "jmp") and ops.startswith("0x"):
                try:
                    tgt = int(ops.split()[0], 16)
                except ValueError:
                    continue
                m["B_total"] += 1
                if not (t0 <= tgt < t1) or blob[tgt - t0] == 0xCC:
                    m["B_bad"] += 1
                    m.setdefault("B_examples", []).append((int(mm["addr"], 16), ops))
            if "ds:0x" in ops:
                try:
                    tgt = int(ops.split("ds:0x")[1].split()[0].rstrip(",)"), 16)
                except (IndexError, ValueError):
                    continue
                m["C_abs"] += 1
                if not (img_lo <= tgt < img_hi):
                    m["C_bad"].append((int(mm["addr"], 16), f"{mn} {ops}"))
                else:
                    sec = (".text" if t0 <= tgt < t1
                           else ".rdata" if rdata["va"] <= tgt < rdata["va"] + rdata["vsize"]
                           else ".data" if data["va"] <= tgt < data["va"] + max(data["vsize"], data["raw"])
                           else "other")
                    m["hist"][sec] += 1
        # D. IAT slot reachability across the WHOLE .text (not just the window)
        pep = pefile.PE(str(path))
        pep.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        slots: dict[int, str] = {}
        for e in getattr(pep, "DIRECTORY_ENTRY_IMPORT", []):
            dll = e.dll.decode("latin-1")
            rva = e.struct.FirstThunk
            for imp in e.imports:
                name = imp.name.decode("latin-1") if imp.name else f"#{imp.ordinal}"
                slots[pe.base + rva] = f"{dll}!{name}"
                rva += 4
        m["slots"] = slots
        m["D_covered"] = {va: nm for va, nm in slots.items() if struct.pack("<I", va) in blob}
        m["D_thunks"] = {va: nm for va, nm in slots.items()
                         if bytes([0xFF, 0x25]) + struct.pack("<I", va) in blob}
        # E. string cross-refs
        m["E"] = {}
        for nm, (va, kind) in CROSS_REF_STRINGS.items():
            n = blob.count(struct.pack("<I", va))
            try:
                s = pe.cstr(va, 64).decode("latin-1", "replace") if kind == "a" else pe.wstr(va)
            except Exception as exc:  # noqa: BLE001
                s = f"<read error {exc}>"
            m["E"][nm] = (va, n, s)
        # F. function starts
        starts = [e for e in ends if e % 16 == 0 and e < t1]
        firsts = Counter(blob[e - t0] for e in starts)
        good = sum(v for k, v in firsts.items() if k in X86_START_OK)
        m["F_good"], m["F_total"] = good, len(starts)
        m["F_firsts"] = firsts
        out[label] = m
    # G. diff between the two .text blobs
    bu, bc = out["unpacked"]["blob"], out["encrypted"]["blob"]
    n = min(len(bu), len(bc))
    diff = sum(1 for i in range(0, n, 1) if bu[i] != bc[i])
    out["G"] = {"compared": n, "differing": diff, "pct": 100 * diff / max(1, n)}
    return out


def main() -> int:
    M = metrics(UNPACKED, ORIG)
    U, E = M["unpacked"], M["encrypted"]
    # Chance baselines, stated so every threshold below is arguable.
    CHANCE_ALIGN = 1 / 16  # a uniformly-random address is 16-byte aligned with p=1/16
    CHANCE_OPCODE = len(X86_START_OK) / 256  # a random byte lands in the allowed set
    n_e_a, n_e_f = E["A_total"], E["F_total"]
    L: list[str] = []
    L.append("# Verifying `SporeApp.exe.unpacked.exe`'s .text is a correct decryption")
    L.append("")
    L.append(f"- candidate : `{UNPACKED.relative_to(_ROOT)}` ({UNPACKED.stat().st_size:,} bytes)")
    L.append(f"- control   : `{ORIG.relative_to(_ROOT)}` (encrypted .text, known garbage)")
    L.append(f"- `.text`   : 0x{U['t0']:08x}..0x{U['t1']:08x} ({len(U['blob']):,} bytes), entropy 6.638 vs 8.000")
    L.append("- `.rdata`/`.data`/`.rsrc`/`.reloc`/`.tls`/`CONST` are **byte-identical** between the two")
    L.append("  files (sha256, `analysis/build_identity.md`), so agreement between decrypted code and")
    L.append("  those sections is evidence, not coincidence.")
    L.append("")
    L.append("Every metric is reported against an explicit **chance baseline**, because the control's")
    L.append("denominators are degenerate: encrypted bytes almost never form 0xCC runs at all "
             f"({E['A_total']} runs vs {U['A_total']}), so a *rate* on the control is meaningless without")
    L.append(f"the baseline (random alignment = {CHANCE_ALIGN:.4f}, random opcode-in-set = {CHANCE_OPCODE:.3f}).")
    L.append("")
    L.append("## Metric table with control")
    L.append("")
    L.append("| # | invariant | unpacked | encrypted (control) | chance baseline | verdict |")
    L.append("|---|---|---|---|---|---|")

    def row(tag, name, uv, ev, sep_ok, baseline=""):
        L.append(f"| {tag} | {name} | {uv} | {ev} | {baseline} | {'**PASS**' if sep_ok else '**FAIL**'} |")
        return sep_ok

    ok = []
    ua = U["A_aligned"] / max(1, U["A_total"])
    ea = E["A_aligned"] / max(1, E["A_total"])
    ok.append(row("A", "int3 run ends 16-aligned",
                  f"{U['A_aligned']:,}/{U['A_total']:,} = {100*ua:.2f}%",
                  f"{E['A_aligned']:,}/{E['A_total']:,} = {100*ea:.2f}%",
                  ua > 0.98 and abs(ea - CHANCE_ALIGN) < 0.05,
                  f"ctrl expect {100*CHANCE_ALIGN:.2f}%"))
    ok.append(row("A2", "int3 run **count** (padding exists at all)",
                  f"{U['A_total']:,}", f"{E['A_total']:,}",
                  U["A_total"] > 100 * max(1, E["A_total"]), "random data has ~no CC runs"))
    ok.append(row("B", "call/jmp targets valid (exact, first 0x10000 of .text)",
                  f"{U['B_total']-U['B_bad']:,}/{U['B_total']:,} = {100*(1-U['B_bad']/max(1,U['B_total'])):.2f}%",
                  f"{E['B_total']-E['B_bad']:,}/{E['B_total']:,} = {100*(1-E['B_bad']/max(1,E['B_total'])):.2f}%",
                  U["B_bad"] == 0 and E["B_bad"] > 0))
    # C: judge by *structure* of the offenders, not their count. Real code's out-of-image
    # operands are a tiny set of meaningful values (0x0); garbage's are ~all-distinct noise.
    # Offender values come back as the hex text after "ds:0x"; normalise to int so the
    # comparison is not a str-vs-int type mismatch (an earlier revision compared "0" to 0x0
    # and reported a spurious FAIL).
    def vals(bad):
        out = []
        for a, t in bad:
            try:
                out.append(int(t.split("ds:0x")[-1].split()[0].rstrip(",)"), 16))
            except ValueError:
                out.append(None)
        return out
    uvals = Counter(vals(U["C_bad"]))
    evals = vals(E["C_bad"])
    ok.append(row("C", "absolute ds: operands: in-image + offender structure",
                  f"{U['C_abs']-len(U['C_bad']):,}/{U['C_abs']:,} in-image; offenders={ {hex(k): v for k, v in uvals.items()} }",
                  f"{E['C_abs']-len(E['C_bad']):,}/{E['C_abs']:,} in-image; offenders: "
                  f"{len(set(evals))} distinct of {len(evals)}",
                  U["C_abs"] > 100 and len(U["C_bad"]) <= 0.05 * U["C_abs"]
                  and set(uvals) <= {0x0}
                  and len(set(evals)) > 0.9 * len(evals),
                  "real code's offenders are 0x0, not noise"))
    ok.append(row("D", "IAT slots referenced somewhere in .text",
                  f"{len(U['D_covered'])}/{len(U['slots'])}",
                  f"{len(E['D_covered'])}/{len(E['slots'])}",
                  len(U["D_covered"]) == len(U["slots"]) and len(E["D_covered"]) < 0.05 * len(E["slots"])))
    ok.append(row("D2", "IAT slots with an FF 25 jmp thunk",
                  f"{len(U['D_thunks'])}/{len(U['slots'])}",
                  f"{len(E['D_thunks'])}/{len(E['slots'])}",
                  len(U["D_thunks"]) > 0))
    ue = sum(1 for _, n, _ in U["E"].values() if n > 0)
    ee = sum(1 for _, n, _ in E["E"].values() if n > 0)
    ok.append(row("E", "known .rdata string addresses referenced",
                  f"{ue}/{len(U['E'])}", f"{ee}/{len(E['E'])}",
                  ue == len(U["E"]) and ee == 0))
    uf = U["F_good"] / max(1, U["F_total"])
    ef = E["F_good"] / max(1, E["F_total"])
    ok.append(row("F", "function starts begin with a real opcode",
                  f"{U['F_good']:,}/{U['F_total']:,} = {100*uf:.2f}%",
                  f"{E['F_good']:,}/{E['F_total']:,} = {100*ef:.2f}%",
                  uf > 0.97 and ef < 0.90,
                  f"ctrl expect {100*CHANCE_OPCODE:.1f}% (n={n_e_f}, noise))"))
    G = M["G"]
    equal = G["compared"] - G["differing"]
    exp_equal = G["compared"] / 256
    ok.append(row("G", ".text byte equality vs the encrypted original",
                  f"{equal:,} equal / {G['compared']:,} = {100*equal/G['compared']:.4f}%",
                  "-",
                  abs(equal - exp_equal) < 0.10 * exp_equal,
                  f"independence predicts {exp_equal:,.0f} equal bytes"))
    L.append("")
    L.append(f"- control alignment rate {100*ea:.2f}% vs chance {100*CHANCE_ALIGN:.2f}% (n={n_e_a}) -> at chance")
    L.append(f"- control opcode rate {100*ef:.1f}% vs chance {100*CHANCE_OPCODE:.1f}% (n={n_e_f}) -> at chance")
    L.append(f"- the control's degenerate denominators are why A2/F-chance are stated separately.")
    L.append("")
    L.append("## Detail: .text equality (metric G)")
    L.append("")
    L.append(f"- {G['differing']:,} of {G['compared']:,} bytes differ ({G['pct']:.2f}%).")
    L.append(f"- equal bytes: {equal:,}; byte-wise independence predicts {exp_equal:,.0f} "
             f"({100*equal/exp_equal:.2f}% of prediction).")
    L.append("- Interpretation: the two .text blobs are statistically independent byte streams, i.e. the")
    L.append("  unpack rewrote the **whole** section rather than patching part of it. A partial or tampered")
    L.append("  dump would show a large excess of equal bytes.")
    L.append("")
    L.append("## Detail: string cross-references (the decisive test)")
    L.append("")
    L.append("| string | address | refs (unpacked) | refs (encrypted ctrl) | bytes at that address |")
    L.append("|---|---|---|---|---|")
    for nm, (va, n, s) in sorted(U["E"].items()):
        en = E["E"][nm][1]
        L.append(f"| `{nm}` | 0x{va:08x} | {n} | {en} | `{s}` |")
    L.append("")
    L.append("## Detail: absolute-operand histogram (where decrypted code points)")
    L.append("")
    L.append(f"- unpacked: {dict(U['hist'])}")
    L.append(f"- encrypted control: {dict(E['hist'])}")
    if U["C_bad"]:
        L.append(f"- the {len(U['C_bad'])} out-of-image operands in the unpacked window:")
        for a, t in U["C_bad"][:8]:
            L.append(f"  - 0x{a:08x}: `{t}`")
    if E["C_bad"]:
        L.append(f"- control out-of-image operands (first 5 of {len(E['C_bad'])}):")
        for a, t in E["C_bad"][:5]:
            L.append(f"  - 0x{a:08x}: `{t}`")
    L.append("")
    L.append("## Detail: IAT coverage")
    L.append("")
    L.append(f"- {len(U['D_covered'])}/{len(U['slots'])} slots referenced by the decrypted .text; "
             f"{len(U['D_thunks'])} of those via an `FF 25` jmp thunk.")
    L.append(f"- control (encrypted .text): {len(E['D_covered'])}/{len(E['slots'])} referenced.")
    for va, nm in list(U["D_covered"].items())[:8]:
        L.append(f"  - 0x{va:08x} -> `{nm}`")
    miss = {va: nm for va, nm in U["slots"].items() if va not in U["D_covered"]}
    if miss:
        L.append(f"- unreferenced slots ({len(miss)}): {list(miss.values())[:16]}")
        L.append("  (an import nobody calls is legitimate; it is not evidence against the dump)")
    L.append("")

    all_ok = all(ok)
    L.append("## Verdict")
    L.append("")
    if all_ok:
        L.append("**PASS on every metric, with clean separation from the encrypted control.**")
        L.append("`refs\\bins\\ReleaseGA\\SporeApp.exe.unpacked.exe` is a genuine post-decryption dump of the")
        L.append("GA build and is the **level-1 disassembly oracle** for this project. This restores the field")
        L.append("guide's authority order #1 (disassembly of the original binary), which was unusable while")
        L.append("`.text` was encrypted.")
    else:
        bad = [i for i, v in enumerate(ok) if not v]
        L.append(f"**NOT FULLY VERIFIED** - metrics {bad} did not separate. Do not treat the dump as")
        L.append("authoritative until the flagged rows are explained.")
    L.append("")
    L.append("## Method notes (wrong invariants, recorded so they are not re-derived)")
    L.append("")
    L.append("1. **int3 run *start* alignment.** Revision 1 asserted each CC run starts 16-aligned and")
    L.append("   reported 52,843 failures. The invariant was backwards: MSVC aligns the *next function*, so")
    L.append("   the run **end** is the aligned edge. Function at 0x00401000 is 9 bytes")
    L.append("   (`55 8B EC A1 00 0C 5D 01 C3`); the CC run is 0x00401009..0x00401010; 0x00401010 is the next")
    L.append("   start and is aligned. The dump was fine.")
    L.append("2. **Raw E8/E9 byte scanning as a branch census.** Counts immediates and data bytes as")
    L.append("   branches (18% 'bad' on genuine code). Replaced by an exact linear-disassembly window (B).")
    L.append("3. **`FF 25` thunks as the only import form.** Revision 1 demanded a jmp thunk per IAT slot and")
    L.append("   got 100/495, then called that a failure. Most imports are reached by `FF 15 call [slot]`,")
    L.append("   `push [slot]` or `mov reg,[slot]`; the correct metric is 'slot address appears in .text' (D),")
    L.append("   with D2 kept as the thunk-only sub-count.")
    L.append("4. **No control.** The first two revisions judged numbers in isolation. Every metric here is")
    L.append("   now paired with the same measurement on the encrypted .text; a metric that does not separate")
    L.append("   the two is not evidence and is marked as such.")
    L.append("5. **Thresholds ignoring the chance baseline.** Revision 3 required the control's opcode rate to")
    L.append("   be <10%, but the allowed-opcode set covers 64.5% of all byte values, so random data scores")
    L.append("   ~64.5% and the test was unsatisfiable by construction. Likewise it required the control's")
    L.append("   alignment rate to be <2%, when random addresses are 1/16 = 6.25% aligned. Both are now")
    L.append("   compared against the computed baseline, and the baseline is printed in the table.")
    L.append("6. **str vs int comparisons.** The absolute-operand offenders were compared as text ('0') against")
    L.append("   an integer (0x0), producing a spurious FAIL on a metric that was actually clean.")
    L.append("7. `objdump` over the whole 16 MB .text is avoided: a 0x10000 linear window plus raw byte scans")
    L.append("   for the whole-section metrics (A/D/E/F) gives the same evidence far cheaper")
    L.append("   (field guide §2: never dump the whole exe).")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(L) + "\n", encoding="utf-8")
    # The console is cp1251 here and chokes on U+FFFD from lossy decodes; the report file is
    # UTF-8 and is the artifact that matters (field guide §2: print via ascii() when the
    # console codepage fights you).
    sys.stdout.buffer.write(("\n".join(L) + "\n").encode("ascii", "replace"))
    print(f"wrote {OUT.relative_to(_ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
