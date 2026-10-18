"""Decompile one function from the PERSISTENT Ghidra project and print it.

Cheap (seconds) compared to a REA CLI query, which re-imports and re-analyzes the whole 25 MB binary
every time (~20 min). This is the batch/persistent half of the toolchain; see
analysis/toolchain-rea.md for why both halves exist.

    refs/ghidra/venv/Scripts/python.exe tools/ghidra_decompile.py 0x7e2260 [more addrs...]
"""
import os
import sys

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\12.1.4-20260921")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    addrs = sys.argv[1:]
    if not addrs:
        print("usage: ghidra_decompile.py <0xADDR> [...]")
        return 2
    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project, program_context
    from ghidra.app.decompiler import DecompInterface

    proj = open_project(os.path.join(ROOT, "refs", "ghidra", "proj"), "OpenSporeOracle")
    with program_context(proj, "/SporeApp.exe.unpacked.exe") as prog:
        AF = prog.getAddressFactory()
        FM = prog.getFunctionManager()
        d = DecompInterface()
        if not d.openProgram(prog):
            print("could not open the program in the decompiler")
            return 3
        for h in addrs:
            s = h.strip().lower().replace("0x", "")
            addr = AF.getAddress("%08x" % (int(s, 16) & 0xFFFFFFFF))
            fn = FM.getFunctionAt(addr) or FM.getFunctionContaining(addr)
            print("=" * 100)
            if fn is None:
                print("%s: NO FUNCTION (Ghidra never carved it; see analysis/audit/seeds.tsv)" % h)
                continue
            print("%s -> %s  body %s..%s" % (h, fn.getName(), fn.getBody().getMinAddress(),
                                             fn.getBody().getMaxAddress()))
            res = d.decompileFunction(fn, 120, pyghidra.task_monitor())
            if res is None or not res.decompileCompleted():
                print("  decompile did not complete")
                continue
            print(res.getDecompiledFunction().getC())
        d.dispose()
    return 0


if __name__ == "__main__":
    sys.exit(main())
