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


def dump_dwords(start, count):
    """Print `count` DWORDs from `start`, resolving each: a pointer into an executable block is named
    as the function it lands in; a pointer at printable data is shown as the string it points to;
    anything else is shown as a bare value. Reading a VTABLE this way shows the interface directly."""
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        AF = prog.getAddressFactory()
        FM = prog.getFunctionManager()
        mem = prog.getMemory()
        for i in range(count):
            va = (start + 4 * i) & 0xFFFFFFFF
            a = AF.getAddress("%08x" % va)
            try:
                raw = mem.getInt(a) & 0xFFFFFFFF
            except Exception:
                print("0x%08X  <unreadable>" % va)
                continue
            note = ""
            ta = AF.getAddress("%08x" % raw)
            blk = mem.getBlock(ta) if raw > 0x1000 else None
            if blk is not None:
                if blk.isExecute():
                    fn = FM.getFunctionContaining(ta)
                    note = "-> CODE %s" % (fn.getName() if fn else "(uncarved)")
                else:
                    try:
                        s = mem.getBytes(ta, 40)
                        txt = bytearray()
                        for c in s:
                            if c == 0:
                                break
                            txt.append(c if 32 <= c < 127 else 0x2E)
                        if len(txt) >= 4:
                            note = "-> \"%s\"" % txt.decode("latin-1")
                        else:
                            note = "-> data"
                    except Exception:
                        note = "-> data"
            print("  slot +0x%02X  0x%08X  %s" % (4 * i, raw, note))
    return 0


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    # --dump <addr> [count]: print DWORDs at an address, resolving each as a code pointer when it
    # points into an executable block, or as a C string when it points at printable data. This is how
    # a VTABLE is read: the shared slots are the base class, the differing ones are the overrides.
    if sys.argv[1] == "--dump":
        addr = int(sys.argv[2].strip().lower().replace("0x", ""), 16)
        count = int(sys.argv[3]) if len(sys.argv) > 3 else 24
        return dump_dwords(addr, count)
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
