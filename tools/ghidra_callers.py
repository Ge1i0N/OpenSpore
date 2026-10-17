"""Print direct/static references to selected procedures in the persistent Ghidra project.

This is intentionally narrower than a whole-program call graph: indirect vtable dispatch remains
explicitly unresolved, while direct CALL/data references to the selected lifecycle helpers are listed.
"""
from __future__ import annotations

import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921")


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: ghidra_callers.py <0xADDR> [...]")
        return 2
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        af = prog.getAddressFactory()
        fm = prog.getFunctionManager()
        rm = prog.getReferenceManager()
        for raw in sys.argv[1:]:
            addr = af.getAddress("%08x" % (int(raw.lower().replace("0x", ""), 16) & 0xFFFFFFFF))
            fn = fm.getFunctionAt(addr) or fm.getFunctionContaining(addr)
            print("=" * 100)
            print("target %s function=%s" % (addr, fn.getName() if fn else "<no function>"))
            refs = list(rm.getReferencesTo(addr))
            if not refs:
                print("no static references; indirect/vtable dispatch remains possible")
                continue
            for ref in refs:
                source = ref.getFromAddress()
                owner = fm.getFunctionContaining(source)
                kind = str(ref.getReferenceType())
                print("from %s type=%s function=%s" %
                      (source, kind, owner.getName() if owner else "<no function>"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
