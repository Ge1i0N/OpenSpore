#!/usr/bin/env python3
"""Audit EVERY claim this project has made about SporeApp.exe against an independent decompiler.

Session 13 request: "было бы хорошо с помощью гидры проверить вообще всё (начиная с Main),
чтобы понимать, всё ли мы правильно начали реверсить".

Three phases, because Ghidra's embedded Python (Jython) and our CPython cannot share a process:

    python tools/ghidra_audit.py request   # claims.json -> analysis/audit/request.json
    <Ghidra headless export>               # tools/ghidra_export.py -> facts.json + pseudo/*.c
    python tools/ghidra_audit.py verify    # claims.json + facts.json -> audit.md / audit.json

Design rules, learned the hard way in this project:

  * UNKNOWN is not PASS. If Ghidra has no function there, or we did not ask it about something,
    the check reports UNKNOWN and the claim stays unproven. A silent green light is worse than
    red, because we would build C++ on top of it.
  * SKIP is not PASS either. Data-derived claims (gmdl stride measured on 11,041 resources)
    cannot be settled by a decompiler at all; saying so explicitly keeps the audit honest about
    what it does NOT cover.
  * Evidence, not assertions. Every FAIL carries what Ghidra actually found plus the pseudocode
    file, so a human can adjudicate. Ghidra's pseudocode is a lossy reconstruction: on MSVC x86
    without RTTI it can produce plausible-but-wrong C. Disagreement is a signal to re-read the
    disassembly, never an automatic verdict that we were wrong.
  * Immediates are bucketed BY MNEMONIC, so `pushes_imm: 640` cannot be satisfied by an unrelated
    `cmp ... 640` elsewhere in the same function.
  * `pseudo_regex` checks exist for ORDERING claims (FNV-1 multiplies before it xors). Byte-level
    listing reading is a poor way to check evaluation order; the decompiler's expression tree is
    a genuinely different formulation of the same question, which is exactly the point.
"""
import argparse
import json
import os
import re
import sys
from collections import OrderedDict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLAIMS = os.path.join(ROOT, "analysis", "claims.json")
AUDIT_DIR = os.path.join(ROOT, "analysis", "audit")
REQUEST = os.path.join(AUDIT_DIR, "request.json")
FACTS = os.path.join(AUDIT_DIR, "facts.json")
PSEUDO = os.path.join(AUDIT_DIR, "pseudo")

PASS, FAIL, UNKNOWN, SKIP = "PASS", "FAIL", "UNKNOWN", "SKIP"
RANK = {FAIL: 0, UNKNOWN: 1, PASS: 2, SKIP: 3}


def load(path):
    with open(path, "rb") as f:
        return json.loads(f.read().decode("utf-8"))


def norm_addr(v):
    """'0x008482f0' / 8482032 / '8482f0' -> canonical '0x008482f0'."""
    if isinstance(v, int):
        n = v
    else:
        s = str(v).strip().lower()
        n = int(s, 16) if s.startswith("0x") else int(s, 16)
    return "0x%08x" % (n & 0xFFFFFFFF)


def norm_int(v):
    if isinstance(v, int):
        return v & 0xFFFFFFFF
    s = str(v).strip().lower()
    return int(s, 16) if s.startswith("0x") else int(s)


# --------------------------------------------------------------------- request building

def build_request():
    """Walk the ledger and ask Ghidra for exactly the facts the checks need."""
    req = {"functions": OrderedDict(), "vtables": OrderedDict(), "strings": OrderedDict(),
           "pointers": OrderedDict(), "xrefs": OrderedDict(), "insns": OrderedDict(),
           "vtable_slots": OrderedDict()}

    def add(bucket, key, val=True):
        bucket.setdefault(key, val)

    for claim in load(CLAIMS)["claims"]:
        for ck in claim.get("checks", []):
            kind = ck["kind"]
            if kind in ("func", "func_contains_insn", "pseudo_regex", "pseudo_not_regex",
                        "has_imul", "stack_cleanup", "func_exists"):
                add(req["functions"], norm_addr(ck["addr"]))
            if kind == "func":
                add(req["functions"], norm_addr(ck["addr"]))
                for s in ck.get("refs_string", []) or []:
                    add(req["strings"], norm_addr(s["addr"]))
                    add(req["functions"], norm_addr(ck["addr"]))
                for a_ in ck.get("refs_addr", []) or []:
                    add(req["functions"], norm_addr(ck["addr"]))
                for a_ in ck.get("reads_global", []) if isinstance(ck.get("reads_global"), list) else [ck.get("reads_global")]:
                    if a_:
                        add(req["functions"], norm_addr(ck["addr"]))
            elif kind == "func_contains_insn":
                add(req["functions"], norm_addr(ck["fn"]))
                add(req["insns"], norm_addr(ck["addr"]))
            elif kind in ("vtable_slot",):
                add(req["vtables"], norm_addr(ck["vtable"]))
                add(req["vtable_slots"], "0x%x" % int(ck["slot"]))
            elif kind == "data_string":
                add(req["strings"], norm_addr(ck["addr"]))
            elif kind == "data_pointer":
                add(req["pointers"], norm_addr(ck["addr"]))
            elif kind == "direct_callers":
                add(req["xrefs"], norm_addr(ck["addr"]))
            elif kind == "insn_at":
                add(req["insns"], norm_addr(ck["addr"]))
            elif kind == "func_in_range":
                add(req["functions"], norm_addr(ck["range"][0]))
            elif kind in ("pseudo_regex", "pseudo_not_regex"):
                add(req["functions"], norm_addr(ck["addr"]))
            elif kind in ("entry_point", "image_base", "subsystem", "section_executable",
                          "hash_vectors", "not_disasm_verifiable"):
                pass
            else:
                print("  ! unknown check kind %r in claim %s" % (kind, claim["id"]), file=sys.stderr)

    out = {k: sorted(v.keys()) for k, v in req.items() if k != "vtable_slots"}
    out["vtable_slots"] = sorted(req["vtable_slots"].keys(),
                                key=lambda s: int(s, 16))
    os.makedirs(AUDIT_DIR, exist_ok=True)
    with open(REQUEST, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    print("request -> %s" % REQUEST)
    for k in ("functions", "vtables", "strings", "pointers", "xrefs", "insns"):
        print("  %-10s %d" % (k, len(out[k])))
    return out


# --------------------------------------------------------------------- verification

class Ev:
    """Evidence for one claim: Ghidra's facts plus the pseudocode text."""

    def __init__(self, facts):
        self.facts = facts
        self._pseudo = {}

    def fn(self, addr):
        return self.facts.get("functions", {}).get(norm_addr(addr))

    def pseudo(self, addr):
        key = norm_addr(addr)
        if key not in self._pseudo:
            path = os.path.join(PSEUDO, key.replace("0x", "0x") + ".c")
            txt = None
            if os.path.exists(path):
                with open(path, "rb") as f:
                    txt = f.read().decode("utf-8", "replace")
            self._pseudo[key] = txt
        return self._pseudo[key]

    def string(self, addr):
        return self.facts.get("strings", {}).get(norm_addr(addr))

    def vslot(self, vtable, slot):
        return self.facts.get("vtables", {}).get(norm_addr(vtable), {}).get("0x%x" % int(slot))


def chk(func_facts, want, key, label, allow_partial=False):
    """Generic 'set membership' check -> (verdict, evidence)."""
    if func_facts is None:
        return UNKNOWN, "Ghidra was never asked about this function"
    if not func_facts.get("exists"):
        return UNKNOWN, "no function at %s in Ghidra (instruction_at=%s)" % (
            func_facts.get("resolved_from_containing") and "?" or "",
            func_facts.get("instruction_at"))
    have = func_facts.get(key) or []
    missing = [w for w in want if w not in have]
    if not missing:
        return PASS, "%d/%d %s present" % (len(want), len(want), label)
    if allow_partial and len(missing) < len(want):
        return PASS, "%d/%d %s present (missing %s)" % (
            len(want) - len(missing), len(want), label, ", ".join(map(str, missing)))
    return FAIL, "missing %s of %s" % (", ".join(map(str, missing)), label)


# Ghidra prints conditional jumps by their SEMANTIC name (JZ/JNZ/JCS/...), objdump by the historical
# ALIAS (je/jne/jb/...). Our docs and regexes are written in objdump spelling, so a check like
# `regex: "je"` FAILs against `JZ 0x007e40ea` even though it is the same instruction. Normalizing
# Ghidra's canonical form back to the alias set before matching removes that whole class of false
# FAIL. (Found by the audit: it was the ELEVENTH such invisible-API artifact, and unlike the others it
# looked like a real refutation of a documented branch.)
MNEMONIC_ALIAS = {
    "JZ": "JE", "JNZ": "JNE", "JCS": "JB", "JAE": "JNB", "JBE": "JNA", "JA": "JNBE",
    "JO": "JO", "JNO": "JNO", "JP": "JPE", "JNP": "JPO",
    "SETZ": "SETE", "SETNZ": "SETNE", "SETCS": "SETB", "SETAE": "SETNB",
    "CMOVZ": "CMOVE", "CMOVNZ": "CMOVNE", "CMOVL": "CMOVNGE", "CMOVG": "CMOVNLE",
}


def normalize_insns(text):
    if not text:
        return text
    out = text
    for ghidra, alias in MNEMONIC_ALIAS.items():
        out = re.sub(r"\b%s\b" % ghidra, alias, out)
    return out


def in_body(f, addr_hex):
    """True when Ghidra's function body actually COVERS `addr_hex`.

    A discontinuous body means Ghidra never looked at the bytes in the hole, so an absence there is
    not a refutation. See the export note on body_ranges.
    """
    ranges = f.get("body_ranges") or []
    try:
        a = norm_int(addr_hex)
    except Exception:
        return False
    for lo, hi in ranges:
        if norm_int(lo) <= a <= norm_int(hi):
            return True
    return False


def verify_check(ev, ck):
    """Returns (verdict, evidence_string)."""
    kind = ck["kind"]

    if kind == "not_disasm_verifiable":
        return SKIP, "data-derived: " + ck.get("reason", "")

    if kind in ("entry_point", "image_base"):
        meta = ev.facts.get("meta", {})
        have = meta.get(kind)
        want = norm_addr(ck["expect"])
        if have is None:
            return UNKNOWN, "no %s in Ghidra meta" % kind
        if norm_addr(have) == want:
            return PASS, "Ghidra says %s" % have
        return FAIL, "Ghidra says %s, we claimed %s" % (have, want)

    if kind in ("subsystem", "section_executable"):
        return SKIP, "PE-header fact, not exposed by the Ghidra export; check with tools/pe.py"

    if kind == "hash_vectors":
        # Pure arithmetic, independent of Ghidra: recompute and compare.
        algo = ck.get("algo")
        if algo != "fnv1_lower":
            return SKIP, "no recompute implementation for algo %r" % algo
        out = []
        for case in ck["cases"]:
            if case.get("expect") is None:
                out.append("%s: (no expectation recorded)" % case["text"])
                continue
            h = 0x811C9DC5
            for ch in case["text"].lower():
                h = ((h * 0x01000193) & 0xFFFFFFFF) ^ (ord(ch) & 0xFF)
            want = norm_int(case["expect"])
            if h == want:
                out.append("%s -> 0x%08x OK" % (case["text"], h))
            else:
                out.append("%s -> 0x%08x but claimed 0x%08x" % (case["text"], h, want))
        bad = [o for o in out if "but claimed" in o]
        return (FAIL, "; ".join(out)) if bad else (PASS, "; ".join(out))

    if kind == "data_string":
        s = ev.string(ck["addr"])
        if s is None:
            return UNKNOWN, "Ghidra found no readable string at %s" % ck["addr"]
        want = ck["text"].lower()
        if want in s.lower():
            return PASS, "%r" % s[:80]
        return FAIL, "memory at %s reads %r, we claimed %r" % (ck["addr"], s[:80], ck["text"])

    if kind == "data_pointer":
        have = ev.facts.get("pointers", {}).get(norm_addr(ck["addr"]))
        if have is None:
            return UNKNOWN, "no pointer value exported for %s" % ck["addr"]
        if norm_addr(have) == norm_addr(ck["expect"]):
            return PASS, "[%s] = %s" % (ck["addr"], have)
        return FAIL, "[%s] = %s, we claimed %s" % (ck["addr"], have, ck["expect"])

    if kind == "vtable_slot":
        have = ev.vslot(ck["vtable"], ck["slot"])
        if have is None:
            return UNKNOWN, "no value exported for vtable %s slot +%#x" % (ck["vtable"], ck["slot"])
        if norm_addr(have) == norm_addr(ck["expect"]):
            return PASS, "[%s+%#x] = %s" % (ck["vtable"], ck["slot"], have)
        return FAIL, "[%s+%#x] = %s, we claimed %s" % (
            ck["vtable"], ck["slot"], have, ck["expect"])

    if kind == "direct_callers":
        lst = ev.facts.get("callers", {}).get(norm_addr(ck["addr"]))
        if lst is None:
            return UNKNOWN, "no xrefs exported for %s" % ck["addr"]
        calls = [c for c in lst if c.get("is_call")]
        want = ck.get("expect_count")
        if want is not None:
            if len(calls) == want:
                return PASS, "%d call xrefs (as claimed)" % len(calls)
            return FAIL, "%d call xrefs, we claimed %d (all xrefs: %d)" % (
                len(calls), want, len(lst))
        if lst:
            return PASS, "%d xrefs" % len(lst)
        return FAIL, "no xrefs at all"

    if kind == "insn_at":
        s = ev.facts.get("insns", {}).get(norm_addr(ck["addr"]))
        if s is None:
            # Not "the claim is false" - Ghidra has NO instruction here, which for a gap inside a
            # discontinuous function body means it never disassembled those bytes at all.
            return UNKNOWN, ("no instruction at %s in Ghidra's listing (an undischarged gap - seed "
                             "it with kind=code, see analysis/audit/seeds_rea.tsv)" % ck["addr"])
        # Match against BOTH spellings: Ghidra's canonical mnemonic and the objdump alias our docs
        # use. Testing only the raw string is what made `je` FAIL against `JZ`.
        for cand in (s, normalize_insns(s)):
            if re.search(ck["regex"], cand, re.I):
                return PASS, "%s: %s" % (ck["addr"], s)
        return FAIL, "%s is %r (normalized %r), we expected /%s/" % (
            ck["addr"], s, normalize_insns(s), ck["regex"])

    if kind == "func_contains_insn":
        f = ev.fn(ck["fn"])
        s = ev.facts.get("insns", {}).get(norm_addr(ck["addr"]))
        if f is None or s is None:
            return UNKNOWN, "function or instruction not exported"
        if not f.get("exists"):
            return UNKNOWN, "Ghidra has no function covering %s" % ck["fn"]
        lo, hi = norm_int(f["body_start"]), norm_int(f["body_end"])
        at = norm_int(ck["addr"])
        inside = lo <= at <= hi
        ok = inside and bool(re.search(ck["regex"], s, re.I))
        if ok:
            return PASS, "%s inside %s..%s: %s" % (ck["addr"], f["body_start"], f["body_end"], s)
        if not inside:
            return FAIL, "%s is OUTSIDE the function body %s..%s (insn: %s)" % (
                ck["addr"], f["body_start"], f["body_end"], s)
        return FAIL, "instruction at %s is %r, expected /%s/" % (ck["addr"], s, ck["regex"])

    if kind == "func_in_range":
        f = ev.fn(ck["range"][0])
        if f is None or not f.get("exists"):
            return UNKNOWN, "no function at range start %s" % ck["range"][0]
        lo, hi = norm_int(f["body_start"]), norm_int(f["body_end"])
        rlo, rhi = norm_int(ck["range"][0]), norm_int(ck["range"][1])
        if lo <= rlo and rhi <= hi:
            return PASS, "one function %s..%s covers %s..%s" % (
                f["body_start"], f["body_end"], ck["range"][0], ck["range"][1])
        return FAIL, "Ghidra's function is %s..%s, which does NOT cover %s..%s (so the span is " \
                     "more than one function, or our extent claim is wrong)" % (
                f["body_start"], f["body_end"], ck["range"][0], ck["range"][1])

    if kind in ("pseudo_regex", "pseudo_not_regex"):
        txt = ev.pseudo(ck["addr"])
        if not txt:
            return UNKNOWN, "no pseudocode exported for %s" % ck["addr"]
        hit = re.search(ck["regex"], txt, re.S | re.I)
        if kind == "pseudo_regex":
            if hit:
                return PASS, "pseudocode matches /%s/: %r" % (ck["regex"], hit.group(0)[:90])
            return FAIL, "pseudocode for %s has NO match for /%s/" % (ck["addr"], ck["regex"])
        if hit:
            return FAIL, "pseudocode DOES match the forbidden /%s/: %r" % (
                ck["regex"], hit.group(0)[:90])
        return PASS, "forbidden pattern /%s/ absent" % ck["regex"]

    if kind == "func_exists":
        f = ev.fn(ck["addr"])
        if f is None:
            return UNKNOWN, "function %s not in the export" % ck["addr"]
        if f.get("exists"):
            return PASS, "Ghidra has a function here: %s (%s..%s, %d insns)" % (
                f.get("name"), f.get("body_start"), f.get("body_end"), f.get("instruction_count", -1))
        return FAIL, "Ghidra has NO function at %s (listing: %s)" % (
            ck["addr"], f.get("instruction_at"))

    if kind == "func":
        addr = norm_addr(ck["addr"])
        f = ev.fn(addr)
        if f is None:
            return UNKNOWN, "function %s not in the export" % addr
        if not f.get("exists"):
            return (UNKNOWN, "Ghidra has NO function at %s (listing: %s). Either our address is "
                             "wrong or Ghidra failed to carve a function there." %
                             (addr, f.get("instruction_at")))
        verdicts = []
        # Bound up front: several branches below read these, and leaving them to one branch made
        # `pushes_imm` raise UnboundLocalError when the check recorded no refs_string.
        imm = f.get("imm_by_mnemonic", {})
        refs = f.get("refs", [])
        strs = f.get("strings", {})
        if ck.get("calls"):
            want = [norm_addr(x) for x in ck["calls"]]
            have = f.get("calls", [])
            miss = [w for w in want if w not in have]
            if miss:
                verdicts.append((FAIL if ck.get("allow_partial") is not True else PASS,
                                 "direct calls missing: %s (has %d calls)" % (", ".join(miss), len(have))))
            else:
                verdicts.append((PASS, "calls %s present" % ", ".join(want)))
        if ck.get("calls_import"):
            want = [x.lower() for x in ck["calls_import"]]
            have = [x.lower() for x in f.get("imports", [])]
            miss = [w for w in want if not any(w in h for h in have)]
            if miss:
                verdicts.append((FAIL if not ck.get("allow_partial") else PASS,
                                 "imports missing: %s (has: %s)" % (", ".join(miss), ", ".join(have[:14]) or "none")))
            else:
                verdicts.append((PASS, "imports %s present" % ", ".join(ck["calls_import"])))
        if ck.get("calls_import_not"):
            want = [x.lower() for x in ck["calls_import_not"]]
            have = [x.lower() for x in f.get("imports", [])]
            bad = [w for w in want if any(w in h for h in have)]
            if bad:
                verdicts.append((FAIL, "FORBIDDEN import present: %s" % ", ".join(bad)))
            else:
                verdicts.append((PASS, "forbidden imports absent (%s)" % ", ".join(want)))
        if ck.get("refs_addr"):
            want = [norm_addr(x) for x in ck["refs_addr"]]
            have = f.get("refs", [])
            miss = [w for w in want if w not in have]
            if miss:
                verdicts.append((FAIL if not ck.get("allow_partial") else PASS,
                                 "refs missing: %s" % ", ".join(miss)))
            else:
                verdicts.append((PASS, "refs %s present" % ", ".join(want)))
        if ck.get("refs_string"):
            imm = f.get("imm_by_mnemonic", {})
            refs = f.get("refs", [])
            strs = f.get("strings", {})
            for s in ck["refs_string"]:
                a_ = norm_addr(s["addr"])
                ok_ref = a_ in refs
                got = strs.get(a_) or ev.string(a_)
                ok_txt = bool(got) and s["text"].lower() in got.lower()
                if ok_ref and ok_txt:
                    verdicts.append((PASS, "string %s (%r) referenced" % (a_, got[:40])))
                elif not ok_ref:
                    verdicts.append((FAIL if not ck.get("allow_partial") else PASS,
                                     "no reference from %s to string %s (%r)" % (addr, a_, got)))
                else:
                    verdicts.append((FAIL if not ck.get("allow_partial") else PASS,
                                     "string at %s reads %r, claimed %r" % (a_, (got or "")[:40], s["text"])))
        # A discontinuous body means Ghidra left bytes undisassembled, so a MISSING immediate is
        # only a refutation if there is no hole it could be hiding in. Downgrade to UNKNOWN there -
        # otherwise the auditor reports FAIL against a claim it never actually examined (this is
        # exactly how the audit's 250/10 retry values "disappeared" from the event dispatcher).
        def verdict_for(miss, label):
            if not miss:
                return None
            if f.get("body_contiguous", True):
                return FAIL, "%s missing: %s" % (label, ", ".join("0x%08x" % m for m in miss))
            holes = []
            ranges = f.get("body_ranges") or []
            try:
                pts = [(norm_int(lo), norm_int(hi)) for lo, hi in ranges]
                pts.sort()
                for (a0, a1), (b0, b1) in zip(pts, pts[1:]):
                    if b0 > a1 + 1:
                        holes.append("0x%08x..0x%08x (%d B)" % (a1 + 1, b0 - 1, b0 - a1 - 1))
            except Exception:
                pass
            return UNKNOWN, ("%s absent from Ghidra's view, but its body has undischarged gaps [%s]; "
                             "seed those with kind=code before treating this as a refutation"
                             % (label, "; ".join(holes) or "unlisted"))

        if ck.get("pushes_imm"):
            want = [norm_int(x) for x in ck["pushes_imm"]]
            have = imm.get("PUSH", [])
            miss = [w for w in want if w not in have]
            if miss:
                v = verdict_for(miss, "no PUSH of")
                if v is not None:
                    verdicts.append((FAIL if ck.get("allow_partial") else v[0], v[1]))
            else:
                verdicts.append((PASS, "push of %s present" % ", ".join("0x%08x" % w for w in want)))
        if ck.get("cmp_imm"):
            want = [norm_int(x) for x in ck["cmp_imm"]]
            have = imm.get("CMP", [])
            miss = [w for w in want if w not in have]
            if miss:
                v = verdict_for(miss, "no CMP of")
                if v is not None:
                    verdicts.append((FAIL if not ck.get("allow_partial") else v[0], v[1]))
            else:
                verdicts.append((PASS, "cmp of %s present" % ", ".join("0x%08x" % w for w in want)))
        if ck.get("uses_imm"):
            want = [norm_int(x) for x in ck["uses_imm"]]
            allimm = set()
            for v in imm.values():
                allimm.update(v)
            miss = [w for w in want if w not in allimm]
            if miss:
                v = verdict_for(miss, "immediate")
                if v is not None:
                    verdicts.append(v)
            else:
                verdicts.append((PASS, "immediates %s present" % ", ".join(
                    "0x%08x" % w for w in want)))
        if ck.get("reads_global"):
            want = [norm_addr(x) for x in (ck["reads_global"]
                                           if isinstance(ck["reads_global"], list)
                                           else [ck["reads_global"]])]
            have = f.get("refs", [])
            miss = [w for w in want if w not in have]
            if miss:
                verdicts.append((FAIL, "global read missing: %s" % ", ".join(miss)))
            else:
                verdicts.append((PASS, "reads global %s" % ", ".join(want)))
        if "stack_cleanup" in ck:
            have = f.get("stack_cleanup")
            if have is None:
                verdicts.append((UNKNOWN, "no RET-with-cleanup found"))
            elif have == norm_int(ck["stack_cleanup"]):
                verdicts.append((PASS, "ret %#x" % have))
            else:
                verdicts.append((FAIL, "ret %#x, we claimed ret %#x" %
                                 (have, norm_int(ck["stack_cleanup"]))))
        if ck.get("range_contains"):
            at = norm_int(ck["range_contains"])
            lo, hi = norm_int(f["body_start"]), norm_int(f["body_end"])
            if lo <= at <= hi:
                verdicts.append((PASS, "%s lies inside body %s..%s (one function)" %
                                 (ck["range_contains"], f["body_start"], f["body_end"])))
            else:
                verdicts.append((FAIL, "body is %s..%s, which does NOT contain %s" %
                                 (f["body_start"], f["body_end"], ck["range_contains"])))
        if "has_imul" in ck:
            n = f.get("imul_count", 0)
            if ck["has_imul"] and n >= 1:
                verdicts.append((PASS, "%d IMUL" % n))
            elif ck["has_imul"]:
                verdicts.append((FAIL, "no IMUL at all"))
            else:
                verdicts.append((PASS, "no IMUL, as claimed") if n == 0 else
                                (FAIL, "%d IMUL, claimed none" % n))
        if ck.get("flows_to"):
            want = [norm_addr(x) for x in ck["flows_to"]]
            have = set(f.get("branches", [])) | set(f.get("calls", [])) | set(f.get("refs", []))
            miss = [w for w in want if w not in have]
            if miss:
                verdicts.append((FAIL, "no flow to %s" % ", ".join(miss)))
            else:
                verdicts.append((PASS, "flows to %s" % ", ".join(want)))
        if not verdicts:
            return UNKNOWN, "check records no assertion"
        worst = min(verdicts, key=lambda v: RANK[v[0]])
        return worst[0], " | ".join("[%s] %s" % (v, e) for v, e in verdicts)

    return UNKNOWN, "unhandled check kind %r" % kind


def verify():
    claims = load(CLAIMS)
    if not os.path.exists(FACTS):
        print("no %s yet - run the Ghidra export first" % FACTS)
        return 2
    facts = load(FACTS)
    ev = Ev(facts)

    rows = []
    for claim in claims["claims"]:
        results = []
        for ck in claim.get("checks", []):
            verdict, evidence = verify_check(ev, ck)
            results.append({"kind": ck["kind"], "verdict": verdict, "evidence": evidence,
                            "note": ck.get("note"), "addr": ck.get("addr") or ck.get("fn")
                            or ck.get("vtable") or ck.get("range", [None])[0]})
        order = [RANK[r["verdict"]] for r in results]
        overall = ("SKIP" if not order or min(order) == RANK[SKIP]
                   else {0: "FAIL", 1: "UNKNOWN", 2: "PASS"}[min(order)])
        rows.append({"id": claim["id"], "doc": claim.get("doc"), "severity": claim.get("severity"),
                      "overall": overall, "checks": results})

    with open(os.path.join(AUDIT_DIR, "audit.json"), "w", encoding="utf-8") as f:
        json.dump({"meta": facts.get("meta"), "claims": rows}, f, indent=1)

    tally = {}
    for r in rows:
        tally[r["overall"]] = tally.get(r["overall"], 0) + 1
    lines = []
    lines.append("# Ghidra audit of every claim we have made")
    lines.append("")
    lines.append("Independent re-check of `analysis/claims.json` against Ghidra's own analysis of")
    lines.append("`%s`." % claims.get("_binary", "the oracle"))
    lines.append("")
    m = facts.get("meta", {})
    lines.append("- Ghidra %s, language %s, image base %s, entry %s, %s functions carved."
                 % (m.get("ghidra_version"), m.get("language"), m.get("image_base"),
                    m.get("entry_point"), m.get("function_count")))
    lines.append("- Claims: %s" % ", ".join("%s %d" % (k, tally[k])
                                            for k in ("FAIL", "UNKNOWN", "PASS", "SKIP")))
    lines.append("")
    lines.append("`FAIL` = Ghidra disagrees with the doc. `UNKNOWN` = Ghidra could not answer, so the")
    lines.append("claim is still only as good as our own objdump reading. `SKIP` = not a disassembly")
    lines.append("question at all. **Only `FAIL` is a correction; `UNKNOWN` is unfinished work.**")
    lines.append("")
    for want in ("FAIL", "UNKNOWN", "PASS", "SKIP"):
        subset = [r for r in rows if r["overall"] == want]
        if not subset:
            continue
        lines.append("## %s (%d)" % (want, len(subset)))
        lines.append("")
        lines.append("| claim | sev | doc | verdict |")
        lines.append("|---|---|---|---|")
        for r in subset:
            lines.append("| `%s` | %s | %s | **%s** |" % (r["id"], r.get("severity") or "-",
                                                          r.get("doc") or "-", r["overall"]))
        lines.append("")
        for r in subset:
            lines.append("### %s — %s" % (r["id"], r["overall"]))
            lines.append("")
            for c in r["checks"]:
                mark = {"PASS": "ok", "FAIL": "MISMATCH", "UNKNOWN": "?", "SKIP": "skip"}[c["verdict"]]
                lines.append("- `%s` [%s] `%s` %s" % (c["verdict"], mark, c["kind"], c["evidence"]))
                if c.get("note"):
                    lines.append("  - claim: %s" % c["note"])
            lines.append("")
    with open(os.path.join(ROOT, "analysis", "audit.md"), "w", encoding="utf-8") as f:
        f.write("\n".join(lines) + "\n")
    print("wrote analysis/audit.md and audit/audit.json")
    print("claims: " + ", ".join("%s %d" % (k, tally.get(k, 0))
                                  for k in ("FAIL", "UNKNOWN", "PASS", "SKIP")))
    for r in rows:
        if r["overall"] == "FAIL":
            print("  FAIL %s" % r["id"])
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("phase", choices=["request", "verify"])
    args = ap.parse_args()
    return build_request() is not None and 0 if args.phase == "request" else verify()


if __name__ == "__main__":
    sys.exit(main())
