"""Print raw INSTRUCTIONS in an address range from the persistent project.

Needed because the addresses that matter are sometimes inside bytes Ghidra never carved into a
function (no `FUN_...`, xrefs reporting "<no function>") - the wall that has now bitten this project
several times. A decompiler cannot describe a function that does not exist; the bytes are still there
and the listing always shows them.

    refs/ghidra/venv/Scripts/python.exe tools/ghidra_list.py 0xb5c8a0 120

Prints address and instruction, marks CALL targets (so a run of factory registrations is visible at a
glance), and announces when it enters a carved function.
"""
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    start = int(sys.argv[1].strip().lower().replace("0x", ""), 16)
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        AF = prog.getAddressFactory()
        FM = prog.getFunctionManager()
        listing = prog.getListing()
        start_addr = AF.getAddress("%08x" % (start & 0xFFFFFFFF))

        ins = listing.getInstructionAt(start_addr)
        if ins is None:
            print("no instruction exactly at 0x%08X" % start)
        it = listing.getInstructions(start_addr, True)
        n = 0
        fn_prev = None
        while it.hasNext() and n < count:
            i = it.next()
            if i is None:
                break
            owner = FM.getFunctionContaining(i.getAddress())
            oname = owner.getName() if owner else "<no function>"
            if oname != fn_prev:
                print("-- %s  (starts here or earlier) --" % oname)
                fn_prev = oname
            mark = ""
            if "CALL" in i.getMnemonicString().upper():
                tgts = [str(r.getToAddress()) for r in i.getReferencesFrom()]
                mark = "   -> " + ", ".join(tgts)
            print("%s  %s%s" % (i.getAddress(), i.toString(), mark))
            n += 1
        print("-- %d instruction(s) shown from 0x%08X --" % (n, start))
    return 0


if __name__ == "__main__":
    sys.exit(main())
