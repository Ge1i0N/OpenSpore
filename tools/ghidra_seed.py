#!/usr/bin/env python3
"""Apply analysis/audit/seeds.tsv to the persistent Ghidra project, then re-analyze.

Why this exists: the audit found seven addresses our claims cite where Ghidra had carved NO function.
tools/gen_seeds.py proved six of them ARE real function entries (each bounded by an int3 gap) that
Ghidra never reached, because they are reachable only through vtable / indirect calls - six of the
seven start with the MSVC SEH prologue `mov eax,fs:0x0`, which Ghidra left typed as data. Seeding
makes Ghidra disassemble and analyze them, turning 7 UNKNOWN verdicts into real ones.

REA exposes the same capability as REA_GHIDRA_SEED_FILE. This is our own equivalent so the audit can
run against the ALREADY-analyzed project instead of paying a fresh ~20 minute import per experiment:
Ghidra's project is the reusable asset, whereas REA deliberately uses an ephemeral `-readOnly
-deleteProject` session per run and cannot accumulate this across invocations.

Idempotent: an address Ghidra already carved into a function is skipped, so re-running is cheap.

Run:  refs/ghidra/venv/Scripts/python.exe tools/ghidra_seed.py [seeds.tsv]
"""
import os
import sys

os.environ.setdefault("GHIDRA_INSTALL_DIR", r"C:\Users\DartPower\scoop\apps\ghidra\current")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_SEEDS = os.path.join(ROOT, "analysis", "audit", "seeds.tsv")
PROJECT_DIR = os.path.join(ROOT, "refs", "ghidra", "proj")
PROJECT = "OpenSporeOracle"
PROGRAM = "/SporeApp.exe.unpacked.exe"


def say(*args):
    """Print flushed: the first run died inside a Java callback and lost every buffered line, which
    made it impossible to tell whether the seeds had been applied."""
    sys.stdout.write(" ".join(str(x) for x in args) + "\n")
    sys.stdout.flush()


def read_seeds(path):
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            rows.append((parts[0].strip(), parts[1].strip(),
                         parts[2].strip() if len(parts) > 2 else None))
    return rows


def main():
    seeds_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SEEDS
    rows = read_seeds(seeds_path)
    say("seeds: %d from %s" % (len(rows), seeds_path))
    funcs = [r for r in rows if r[1] == "function"]
    say("function seeds: %d (applied highest address first, like REA does)" % len(funcs))

    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project
    from java.lang import Object
    from ghidra.program.flatapi import FlatProgramAPI
    from ghidra.program.model.symbol import SourceType
    from ghidra.program.util import GhidraProgramUtilities

    proj = open_project(PROJECT_DIR, PROJECT)
    data = proj.getProjectData()
    df = data.getFile(PROGRAM)
    if df is None:
        say("program %s not in project" % PROGRAM)
        return 2

    mon = pyghidra.task_monitor()
    consumer = Object()
    # readOnly=False so we can write; forceTransaction=False so we control the transaction ourselves.
    prog = df.getDomainObject(consumer, False, False, mon)
    try:
        api = FlatProgramAPI(prog, mon)
        FM = prog.getFunctionManager()
        LISTING = prog.getListing()
        AF = prog.getAddressFactory()

        tx = prog.startTransaction("OpenSpore seed")
        applied = skipped = failed = 0
        try:
            for hexaddr, kind, name in sorted(funcs, key=lambda r: -int(r[0], 16)):
                addr = AF.getAddress("%08x" % (int(hexaddr, 16) & 0xFFFFFFFF))
                if FM.getFunctionAt(addr) is not None:
                    skipped += 1
                    continue
                # Disassemble first: an address typed as DATA must become code before it can be a
                # function. clearCodeUnits is a no-op when nothing is defined there.
                try:
                    api.disassemble(addr)
                except Exception as exc:
                    say("  disassemble %s failed: %s" % (hexaddr, exc))
                if LISTING.getInstructionAt(addr) is None:
                    failed += 1
                    say("  ! %s still has no instruction after disassemble" % hexaddr)
                    continue
                try:
                    # FlatProgramAPI.createFunction takes (Address, String) only - there is no
                    # SourceType overload in this Ghidra version.
                    fn = api.createFunction(addr, name or "os_%s" % hexaddr[2:])
                    if fn is not None:
                        applied += 1
                        say("  + %s -> %s" % (hexaddr, fn.getName()))
                    else:
                        failed += 1
                        say("  ! %s createFunction returned None" % hexaddr)
                except Exception as exc:
                    failed += 1
                    say("  ! %s: %s" % (hexaddr, exc))
        finally:
            prog.endTransaction(tx, True)

        say("applied %d, already-carved %d, failed %d" % (applied, skipped, failed))

        # Re-run analysis so the new code gets references, imports and function boundaries - without
        # it the exported facts would list no calls and every check would FAIL spuriously.
        if applied:
            say("re-analyzing (this is the long part; the project is persistent afterwards)...")
            # GhidraProgramUtilities has no analyzeProgram() in this version. The pair that works is
            # FlatProgramAPI.analyzeAll(program) + markProgramAnalyzed, which is exactly what
            # pyghidra's own _analyze_program does; without the bundle-host reference the analyzer
            # refuses to run from a script context.
            # analyzeAll needs a transaction in this context. Without one every analyzer worker dies
            # with db.NoTransactionException (the first run here), because FlatProgramAPI only opens
            # its own transaction for the mutations IT makes, not for the analyzer services it drives.
            from ghidra.app.script import GhidraScriptUtil
            GhidraScriptUtil.acquireBundleHostReference()
            tax = prog.startTransaction("OpenSpore re-analyze")
            try:
                api.analyzeAll(prog)
                GhidraProgramUtilities.markProgramAnalyzed(prog)
            finally:
                prog.endTransaction(tax, True)
                GhidraScriptUtil.releaseBundleHostReference()

        # GhidraFile.save takes a TaskMonitor only; the change comment lives on the transaction.
        df.save(mon)
        say("saved. function count now %d" % FM.getFunctionCount())
    finally:
        prog.release(consumer)
    return 0


if __name__ == "__main__":
    sys.exit(main())
