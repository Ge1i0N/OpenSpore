#!/usr/bin/env python3
"""Turn our claim ledger into Ghidra's OWN view of the oracle binary.

This is phase 2 of the audit (see tools/ghidra_audit.py for phases 1 and 3):

    python tools/ghidra_audit.py request
    <analyzeHeadless.bat -import ...>            # one-time, ~20 min for a 25 MB .text
    python tools/ghidra_export.py                # this file
    python tools/ghidra_audit.py verify

Why a driver and not a GhidraScript: **Ghidra 12 removed its embedded Jython**, so
`-postScript foo.py` fails with "Ghidra was not started with PyGhidra. Python is not available".
The supported path is PyGhidra (real CPython + JPype), which is why this runs from the project's
own venv (`refs/ghidra/venv`, installed from the wheels Ghidra ships in
`$GHIDRA_INSTALL_DIR/Ghidra/Features/PyGhidra/pypkg/dist`).

Design: this script is deliberately DUMB about semantics. It only reports what Ghidra's own
analysis contains - function bodies, direct calls, external calls, references, immediates bucketed
by mnemonic, vtable words, strings, xref callers, and the decompiled pseudocode. It never judges a
claim; tools/ghidra_audit.py does that, so the judgement is reviewable without Ghidra.

Why the audit exists: every address claim in analysis/claims.json was read out of objdump LISTINGS
by hand. A decompiler is a different, independent implementation of the same question, so
disagreement between the two is exactly the signal we want. Note the flip side: Ghidra's pseudocode
is a LOSSY reconstruction (MSVC x86, /GR-, heavy SEH), so agreement is corroboration and
disagreement is a prompt to re-read the disassembly - never an automatic verdict.
"""
import argparse
import json
import os
import re
import sys

# Module state populated by export(). The helpers stay module-level (rather than closures) so the
# file reads like a GhidraScript, but every ghidra import must happen AFTER the JVM is started.
G = {}


def _init_ghidra():
    """Start Ghidra and bind the API objects the helpers use."""
    import pyghidra
    if not pyghidra.started():
        pyghidra.start(verbose=False)
    from ghidra.app.decompiler import DecompInterface
    from ghidra.program.model.listing import FunctionManager
    from ghidra.program.model.scalar import Scalar

    G["DecompInterface"] = DecompInterface
    G["FunctionManager"] = FunctionManager
    G["Scalar"] = Scalar
    # NOTE: do NOT import an "ExternalAddress" class. Ghidra has no such type at
    # ghidra.program.model.address (that import is exactly what failed the first run here);
    # externality is a property of the Address itself, so is_external() asks the object.


# A whole function body is scanned (run spans 0xf47e90..0xf4880a, ~700 instructions), so the
# instruction text cap must sit well above the largest function we make claims about.
MAX_LIST = 512
CAP = 6000


def a(hexstr):
    """'0x008482f0' / 8482032 / '8482f0' -> Ghidra Address in the default (image) space."""
    AF = G["AF"]
    if isinstance(hexstr, int):
        n = hexstr
    else:
        s = str(hexstr).strip().lower()
        n = int(s[2:], 16) if s.startswith("0x") else int(s, 16)
    return AF.getAddress("%08x" % (n & 0xFFFFFFFF))


def hx(addr):
    if addr is None:
        return None
    return "0x%08x" % (int(addr.getOffset()) & 0xFFFFFFFF)


def is_external(target):
    """True for an address in the external space (IMPORT:0001xxxx style).

    Asked of the object rather than by isinstance, because there is no importable
    ExternalAddress class in this Ghidra version.
    """
    try:
        return bool(target.isExternal())
    except Exception:
        return False


def ext_name(ext_addr):
    for meth in ("getName", "getFileName", "getPath"):
        try:
            v = getattr(ext_addr, meth)()
            if v:
                return str(v)
        except Exception:
            pass
    return str(ext_addr)


def _raw_bytes(address, n):
    """Read `n` bytes. Ghidra's getBytes fills a caller-supplied buffer; there is no
    (Address,int,bool) overload, which is what made the first export return no strings.

    Use jpype.JArray, NOT `jarray` (that module only exists inside Jython; importing it raised here
    and lost 24 of 32 function records, because string_at() is on function_facts' hot path).
    """
    MEM = G["MEM"]
    try:
        # JArray("b") raises ClassNotFoundException('b'); the element type must be the JByte CLASS.
        # Getting this wrong silently returned None, which the audit reported as "no readable string"
        # for four perfectly good strings.
        from jpype import JArray, JByte
        buf = JArray(JByte)(n)
        got = MEM.getBytes(address, buf)
        return bytes(bytearray(buf))[:int(got)]
    except Exception:
        return None


def string_at(address):
    """A defined string, else a raw ASCII / UTF-16LE scan. Never guesses.

    NOTE: `DataDB.isString()` does not exist in this Ghidra version (calling it raised and silently
    dropped every string), so a defined datum is accepted when it has a usable `getValue()` that
    returns text. Ghidra's own analyzers already typed these as `unicode`/`ds`, e.g.
    0x01413cc4 -> unicode "Locale/", 0x01413ac8 -> ds "Could not create the Graphics Device.\n...".
    """
    LISTING = G["LISTING"]
    try:
        do = LISTING.getDataAt(address)
        if do is not None:
            try:
                v = do.getValue()
            except Exception:
                v = None
            if isinstance(v, str) and v:
                return v
            # A NON-string datum must fall through to the raw scan, not be stringified. Ghidra typed
            # the tag at 0x013ebc48 as the integer 0x41 even though the bytes there are literally
            # "App\0"; str(0x41) produced the bogus FAIL "reads '0x41', claimed 'App'".
    except Exception:
        pass
    for width in (1, 2):
        raw = _raw_bytes(address, 1024)
        if not raw:
            continue
        out = []
        if width == 2:
            for i in range(0, len(raw) - 1, 2):
                lo = int(raw[i]) & 0xFF
                hi = int(raw[i + 1]) & 0xFF
                if hi != 0 or lo == 0 or lo < 32 or lo > 126:
                    break
                out.append(chr(lo))
        else:
            for b in raw:
                b = int(b) & 0xFF
                if b == 0 or b < 32 or b > 126:
                    break
                out.append(chr(b))
        if len(out) >= 2:
            return "".join(out)
    return None


def external_for(target):
    """Resolve an IAT slot (or an external address) to an imported FUNCTION name.

    This is the piece that makes the main-loop claims checkable: the original calls PeekMessageW
    THROUGH A REGISTER (`mov edi, ds:0x13cc62c`), so there is no direct call reference to follow -
    only the IAT slot's own reference to the external location.
    """
    if is_external(target):
        return ext_name(target)
    FM, REFMGR, EXTMGR = G["FM"], G["REFMGR"], G["EXTMGR"]
    try:
        fn = FM.getFunctionAt(target)
        if fn is not None and fn.isExternal():
            return str(fn.getName())
    except Exception:
        pass
    try:
        for ref in REFMGR.getReferencesFrom(target):
            to = ref.getToAddress()
            if is_external(to):
                return ext_name(to)
    except Exception:
        pass
    try:
        loc = EXTMGR.getExternalLocation(target)
        if loc is not None:
            return str(loc.getName())
    except Exception:
        pass
    return None


def function_facts(address):
    """Everything Ghidra knows about the function at/containing `address`."""
    FM, LISTING, REFMGR, Scalar = G["FM"], G["LISTING"], G["REFMGR"], G["Scalar"]
    out = {"exists": False}
    fn = FM.getFunctionAt(address)
    if fn is None:
        # A claim may cite an address INSIDE a function (a call site, a range start) rather than its
        # entry point. Resolve that, so "no function starts here" is not reportable as a refutation.
        fn = FM.getFunctionContaining(address)
        if fn is not None:
            out["resolved_from_containing"] = True
    if fn is None:
        ins = LISTING.getInstructionAt(address)
        out["instruction_at"] = str(ins) if ins is not None else None
        return out
    out["exists"] = True
    out["name"] = str(fn.getName())
    out["external"] = bool(fn.isExternal())
    body = fn.getBody()
    out["body_start"] = hx(body.getMinAddress())
    out["body_end"] = hx(body.getMaxAddress())
    out["size"] = int(body.getNumAddresses())
    out["entry"] = hx(fn.getEntryPoint())
    try:
        out["calling_convention"] = str(fn.getCallingConvention().getName())
    except Exception:
        pass

    calls, imports, refs, branches = [], [], [], []
    insns, strings = {}, {}
    mnems = set()
    ret_cleanup = None
    imul_count = 0
    # Immediates bucketed BY MNEMONIC: `pushes_imm: 640` must not be satisfiable by an unrelated
    # `cmp ... 640` elsewhere in the same function.
    imm_by_mn = {}

    it = LISTING.getInstructions(body, True)
    n = 0
    while it.hasNext() and n < 20000:
        ins = it.next()
        n += 1
        iaddr = ins.getAddress()
        ihx = hx(iaddr)
        try:
            rep = str(ins)
        except Exception:
            rep = None
        if rep is not None:
            insns[ihx] = rep
        try:
            mn = str(ins.getMnemonicString()).upper()
        except Exception:
            mn = None
        if mn:
            mnems.add(mn)
            if mn == "IMUL":
                imul_count += 1
            if mn == "RET":
                for oi in range(ins.getNumOperands()):
                    for obj in ins.getOpObjects(oi):
                        if isinstance(obj, Scalar):
                            ret_cleanup = int(obj.getValue())
        if mn:
            for oi in range(ins.getNumOperands()):
                try:
                    objs = ins.getOpObjects(oi)
                except Exception:
                    continue
                for obj in objs:
                    if isinstance(obj, Scalar):
                        imm_by_mn.setdefault(mn, set()).add(int(obj.getValue()) & 0xFFFFFFFF)

        try:
            refs_iter = list(REFMGR.getReferencesFrom(iaddr))
        except Exception:
            refs_iter = []
        for ref in refs_iter:
            rt = ref.getReferenceType()
            to = ref.getToAddress()
            if to is None:
                continue
            if is_external(to):
                nm = ext_name(to)
                if nm and nm not in imports:
                    imports.append(nm)
                continue
            th = hx(to)
            if rt.isCall():
                if th not in calls:
                    calls.append(th)
                nm = external_for(to)
                if nm and nm not in imports:
                    imports.append(nm)
            elif rt.isJump():
                if th not in branches:
                    branches.append(th)
            if th not in refs:
                refs.append(th)
                s = string_at(to)
                if s:
                    strings[th] = s

    out["truncated"] = bool(n >= 20000 or len(insns) > CAP)
    out["instruction_count"] = n
    out["calls"] = calls[:MAX_LIST]
    out["imports"] = imports[:MAX_LIST]
    out["branches"] = branches[:MAX_LIST]
    out["refs"] = refs[:MAX_LIST]
    out["strings"] = strings
    out["immediates"] = sorted(imm_by_mn.get("MOV", set()))[:MAX_LIST]
    out["imm_by_mnemonic"] = dict((k, sorted(v)[:MAX_LIST]) for k, v in imm_by_mn.items())
    out["mnemonics"] = sorted(mnems)
    out["imul_count"] = imul_count
    out["stack_cleanup"] = ret_cleanup
    out["insns"] = insns
    return out


def decompile(address):
    """Ghidra's own pseudocode for the function containing `address`."""
    FM = G["FM"]
    fn = FM.getFunctionContaining(address)
    if fn is None:
        fn = FM.getFunctionAt(address)
    if fn is None:
        return None
    decomp = G["DecompInterface"]()
    # setAutoUpdate() does not exist on this DecompInterface (it aborted every pseudocode export in
    # the first run). Options are reachable via getOptions()/setOptions(); defaults are fine here.
    try:
        if not decomp.openProgram(G["prog"]):
            return None
        res = decomp.decompileFunction(fn, 120, G["monitor"])
        if res is None or not res.decompileCompleted():
            return None
        return str(res.getDecompiledFunction().getC())
    except Exception as exc:
        return "/* decompile failed: %s */" % exc
    finally:
        try:
            decomp.dispose()
        except Exception:
            pass


def _meta(fn, fallback=None):
    """Meta provenance must never abort an export over a missing accessor."""
    try:
        v = fn()
        return v
    except Exception:
        return fallback


def _entry_point(prog):
    """The PE entry point, as Ghidra knows it.

    ProgramDB has no getEntrypoint() in this Ghidra version, so read the label the PE loader places
    at the OEP. Returns None (=> the audit reports UNKNOWN) rather than a value we computed
    ourselves, which would make the check circular.
    """
    names = ("EntryPoint", "ENTRY_POINT", "entry", "Entry Point", "start", "_start")
    st = prog.getSymbolTable()
    for name in names:
        try:
            it = st.getSymbols(name)
            while it.hasNext():
                s = it.next()
                addr = s.getAddress()
                if not addr.isExternal():
                    return hx(addr)
        except Exception:
            continue
    return None


def _safe_domain_path(prog):
    try:
        df = prog.getDomainFile()
        if df is None:
            return None
        for meth in ("getPath", "getAbsolutePath", "toString"):
            try:
                return str(getattr(df, meth)())
            except Exception:
                continue
    except Exception:
        pass
    return None


def export(prog, mon, request_path, facts_path, pseudo_dir):
    """The body, with `prog`/`mon` bound. Reads request_path, writes facts_path."""
    # Every manager comes off the Program instance. The static forms
    # (Listing.getListing / FunctionManager.getFunctionManager / ReferenceManager.get...) do not
    # exist in this Ghidra version - that was the second export failure here.
    G.update({
        "prog": prog,
        "monitor": mon,
        "MEM": prog.getMemory(),
        "LISTING": prog.getListing(),
        "REFMGR": prog.getReferenceManager(),
        "FM": prog.getFunctionManager(),
        "AF": prog.getAddressFactory(),
        "EXTMGR": prog.getExternalManager(),
    })

    req = json.load(open(request_path, "rb"))
    facts = {
        "meta": {
            "name": _meta(lambda: str(prog.getName())),
            # GhidraFile has no getPath() in this version; the domain path is provenance only, and
            # a missing accessor must never abort an export.
            "domain_path": _safe_domain_path(prog),
            "language": _meta(lambda: str(prog.getLanguageID())),
            "image_base": _meta(lambda: hx(prog.getImageBase())),
            # ProgramDB has no getEntrypoint() in this Ghidra version; the entry point is a symbol.
            "entry_point": _entry_point(prog),
            "function_count": _meta(lambda: int(G["FM"].getFunctionCount())),
        },
        "functions": {}, "vtables": {}, "strings": {}, "pointers": {},
        "callers": {}, "insns": {}, "errors": [],
    }
    try:
        import pyghidra
        facts["meta"]["ghidra_version"] = str(pyghidra.ApplicationInfo().version()) \
            if hasattr(pyghidra, "ApplicationInfo") else None
    except Exception:
        pass
    try:
        from ghidra.framework import Application
        facts["meta"]["ghidra_version"] = str(Application.getApplicationVersion())
    except Exception:
        pass

    want = req.get("functions", [])
    for i, h in enumerate(want):
        try:
            address = a(h)
            f = function_facts(address)
            facts["functions"][h.lower()] = f
            if pseudo_dir and f.get("exists"):
                code = decompile(address)
                if code:
                    os.makedirs(pseudo_dir, exist_ok=True)
                    safe = re.sub(r"[^0-9a-zA-Z_.-]", "_", h.lower())
                    with open(os.path.join(pseudo_dir, safe + ".c"), "wb") as fp:
                        fp.write(code.encode("utf-8", "replace"))
                    f["pseudo"] = True
                    f["pseudo_lines"] = code.count("\n") + 1
        except Exception as exc:
            facts["errors"].append("function %s: %s" % (h, exc))
        if i % 5 == 0 or i + 1 == len(want):
            print("  export: functions %d/%d" % (i + 1, len(want)))

    slots = req.get("vtable_slots", [])
    for h in req.get("vtables", []):
        try:
            base = a(h)
            MEM = G["MEM"]
            table = {}
            for o in slots or ["0x%x" % x for x in range(0, 0x100, 4)]:
                off = int(str(o), 16)
                try:
                    # BIG-ENDIAN IS FALSE HERE. Memory.getInt(Address, boolean) takes
                    # `bigEndIndexOrder`, not "ignore alignment". Passing True byte-swapped every
                    # vtable word and produced 10 bogus FAILs in the first audit (0x007e6850 came
                    # back as 0x50687e00, which is the same four bytes reversed).
                    v = MEM.getInt(a(int(base.getOffset()) + off), False)
                    table["0x%x" % off] = "0x%08x" % (int(v) & 0xFFFFFFFF)
                except Exception:
                    pass
            facts["vtables"][h.lower()] = table
        except Exception as exc:
            facts["errors"].append("vtable %s: %s" % (h, exc))

    for h in req.get("strings", []):
        try:
            s = string_at(a(h))
            if s is not None:
                facts["strings"][h.lower()] = s
        except Exception as exc:
            facts["errors"].append("string %s: %s" % (h, exc))

    for h in req.get("pointers", []):
        try:
            facts["pointers"][h.lower()] = "0x%08x" % (int(G["MEM"].getInt(a(h), False)) & 0xFFFFFFFF)
        except Exception as exc:
            facts["errors"].append("pointer %s: %s" % (h, exc))

    REFMGR = G["REFMGR"]
    for h in req.get("xrefs", []):
        try:
            address = a(h)
            froms = []
            for ref in REFMGR.getReferencesTo(address):
                froms.append({"addr": hx(ref.getFromAddress()),
                              "type": str(ref.getReferenceType()),
                              "is_call": bool(ref.getReferenceType().isCall())})
                if len(froms) >= MAX_LIST:
                    break
            facts["callers"][h.lower()] = froms
        except Exception as exc:
            facts["errors"].append("xrefs %s: %s" % (h, exc))

    LISTING = G["LISTING"]
    for h in req.get("insns", []):
        try:
            ins = LISTING.getInstructionAt(a(h))
            facts["insns"][h.lower()] = str(ins) if ins is not None else None
        except Exception as exc:
            facts["errors"].append("insn %s: %s" % (h, exc))

    with open(facts_path, "wb") as fp:
        fp.write(json.dumps(facts, indent=1, sort_keys=True).encode("utf-8"))
    print("wrote %s" % facts_path)
    print("  functions %d, vtables %d, strings %d, xref sets %d, insns %d, errors %d"
          % (len(facts["functions"]), len(facts["vtables"]), len(facts["strings"]),
             len(facts["callers"]), len(facts["insns"]), len(facts["errors"])))
    for e in facts["errors"][:10]:
        print("  ! %s" % e)
    return facts


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--project-dir", default=os.path.join(root, "refs", "ghidra", "proj"))
    ap.add_argument("--project", default="OpenSporeOracle")
    ap.add_argument("--program", default="/SporeApp.exe.unpacked.exe",
                    help="path of the program INSIDE the Ghidra project")
    ap.add_argument("--request", default=os.path.join(root, "analysis", "audit", "request.json"))
    ap.add_argument("--facts", default=os.path.join(root, "analysis", "audit", "facts.json"))
    ap.add_argument("--pseudo", default=os.path.join(root, "analysis", "audit", "pseudo"))
    args = ap.parse_args()

    for p, what in ((args.request, "request (run tools/ghidra_audit.py request first)"),
                    (os.path.join(args.project_dir, args.project + ".gpr"), "Ghidra project")):
        if not os.path.exists(p):
            print("missing %s: %s" % (what, p), file=sys.stderr)
            return 2

    _init_ghidra()
    import pyghidra
    from pyghidra import open_project, program_context

    print("opening %s / %s" % (args.project_dir, args.project))
    proj = open_project(args.project_dir, args.project)
    with program_context(proj, args.program) as prog:
        export(prog, pyghidra.task_monitor(), args.request, args.facts, args.pseudo)
    return 0


if __name__ == "__main__":
    sys.exit(main())
