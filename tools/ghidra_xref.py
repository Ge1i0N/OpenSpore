"""Who REFERENCES an address, or a string by its text, in the PERSISTENT Ghidra project?

The missing half of the bridge: ghidra_decompile.py answers "what does this function do", and the
audit answers "does this claim hold". Most real questions are the other direction - "who points at
this table / this string?" - and today's is: 11 of the 17 `mode` names are ASCII strings in the
binary and absent from the shipped data, so a mode is a REGISTERED CLASS. Finding the code that
references those names finds the registrar, and the registrar is the mode table.

    refs/ghidra/venv/Scripts/python.exe tools/ghidra_xref.py --string Game_Creature [more...]
    refs/ghidra/venv/Scripts/python.exe tools/ghidra_xref.py 0x1407a030 0x141cc10

Addresses may be given as file offsets (0x...) or VAs; `--string` searches the program's DEFINED
string data for an exact text match, so you never have to compute a VA by hand. Both forms print the
referencing addresses, whether each reference is a call or a data read, and the function each
reference lives in - which is the useful part, because a registrar is read from a table and the
function name tells you whose table it is.
"""
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 2
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        AF = prog.getAddressFactory()
        FM = prog.getFunctionManager()
        RM = prog.getReferenceManager()
        listing = prog.getListing()

        # Split the arguments into --string <text> and bare addresses.
        strings, addrs, i = [], [], 0
        while i < len(args):
            a = args[i]
            if a == "--string" and i + 1 < len(args):
                strings.append(args[i + 1])
                i += 2
                continue
            addrs.append(a)
            i += 1

        resolved = []
        for a in addrs:
            s = a.strip().lower().replace("0x", "")
            resolved.append((a, AF.getAddress("%08x" % (int(s, 16) & 0xFFFFFFFF))))

        if strings:
            # Walk the DEFINED data once and match on the rendered text. Ghidra renders both ASCII and
            # UTF-16 string data with surrounding quotes, so this catches either encoding without
            # knowing which section it is in or converting a file offset by hand.
            want = set(strings)
            di = listing.getData(True)
            while di.hasNext():
                d = di.next()
                rep = d.getDefaultValueRepresentation()
                if not rep:
                    continue
                txt = rep[1:-1] if len(rep) > 1 and rep[0] == '"' and rep[-1] == '"' else rep
                if txt in want:
                    resolved.append(("string %r" % (txt,), d.getMinAddress()))
                    want.discard(txt)
            for w in sorted(want):
                print("NO DEFINED STRING DATA matches %r - it may be an undefined byte sequence, or"
                      " spelled differently (try the exact case)" % w)

        print("targets resolved: %d" % len(resolved))
        for label, addr in resolved:
            print("=" * 100)
            fn_here = FM.getFunctionContaining(addr)
            print("%s  ->  %s%s" % (label, addr,
                  "   (inside function %s)" % fn_here.getName() if fn_here else
                  "   (not inside any carved function - likely data)"))
            refs = list(RM.getReferencesTo(addr))
            if not refs:
                print("  no references. Ghidra never created one here; if this is a table entry the"
                      " pointer may be an unanalysed dword - seed the containing region.")
                continue
            print("  %d reference(s):" % len(refs))
            seen_fn = {}
            for r in refs:
                frm = r.getFromAddress()
                # NOTE: this bridge exposes NEITHER `Reference.isCall()` NOR `getReferenceTypeName()`
                # on MemReferenceDB, though both are in the documented Java interface. Two more
                # entries for MEMORY.md's Ghidra API wall. The defensive ladder below tries the forms
                # that do exist and reports which one worked, so the tool cannot silently print "?"
                # and be trusted as an answer.
                tname = None
                for attempt in ("type_name", "reftype_getname", "reftype_str", "str"):
                    try:
                        if attempt == "type_name":
                            tname = r.getReferenceTypeName()
                        elif attempt == "reftype_getname":
                            tname = r.getReferenceType().getName()
                        elif attempt == "reftype_str":
                            tname = str(r.getReferenceType())
                        else:
                            tname = str(r)
                        if tname:
                            accessor = attempt
                            break
                    except (AttributeError, TypeError):
                        continue
                tname = tname or "?"
                kind = "CALL" if "CALL" in tname.upper() else ("JUMP" if "JUMP" in tname.upper()
                                                               else "DATA")
                owner = FM.getFunctionContaining(frm)
                oname = owner.getName() if owner else "<no function>"
                seen_fn.setdefault(oname, []).append((str(frm), kind))
                print("    %-6s from %-12s type=%-24s in %s"
                      % (kind, frm, tname, oname))
            print("  functions that reference it: %s"
                  % ", ".join("%s (%d)" % (k, len(v)) for k, v in sorted(seen_fn.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
