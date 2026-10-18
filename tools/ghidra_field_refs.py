"""Find bounded instruction references to a structure field displacement in persistent Ghidra."""
from __future__ import annotations

import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921")


def main() -> int:
    needle = sys.argv[1].lower() if len(sys.argv) > 1 else "+ 0x5c"
    limit = int(sys.argv[2]) if len(sys.argv) > 2 else 120
    start_text = sys.argv[3] if len(sys.argv) > 3 else None
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        listing = prog.getListing()
        fm = prog.getFunctionManager()
        af = prog.getAddressFactory()
        start = prog.getMinAddress() if start_text is None else af.getAddress(
            "%08x" % (int(start_text.lower().replace("0x", ""), 16) & 0xFFFFFFFF))
        it = listing.getInstructions(start, True)
        hits = 0
        while it.hasNext() and hits < limit:
            ins = it.next()
            text = str(ins).lower()
            if needle not in text:
                continue
            owner = fm.getFunctionContaining(ins.getAddress())
            print("%s  %s  function=%s" %
                  (ins.getAddress(), ins, owner.getName() if owner else "<no function>"))
            hits += 1
        print("matches=%d limit=%d needle=%s" % (hits, limit, needle))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
