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
    codes = [r for r in rows if r[1] == "code"]
    say("function seeds: %d (applied highest address first, like REA does)" % len(funcs))
    say("code seeds:     %d (the holes inside already-carved bodies)" % len(codes))

    import pyghidra
    pyghidra.start(verbose=False)
    from pyghidra import open_project
    from java.lang import Object
    from ghidra.program.flatapi import FlatProgramAPI
    from ghidra.app.cmd.disassemble import ClearCodeUnitsCmd
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
        code_applied = code_skipped = 0
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

            # ---- code seeds: fill the HOLES inside an already-carved body -------------------
            # Two walls stop the walk and they are NOT the same thing:
            #   * a decoded INSTRUCTION inside a real function -> genuine territory, stop;
            #   * a defined DATUM -> Ghidra merely TYPED the bytes. The hole exists precisely because
            #     the MSVC alignment padding `8d 49 00` became `?? 8Dh ?? 49h ?? 00h`, so treating a
            #     datum as a stop condition can never fill anything. It must be CLEARED and the walk
            #     continued. That was the first version's bug: it reported all 7 holes "already clear"
            #     and filled none of them, because every hole starts at exactly such a mis-typed datum.
            for hexaddr, kind, name in codes:
                addr = AF.getAddress("%08x" % (int(hexaddr, 16) & 0xFFFFFFFF))
                n_new = 0
                n_cleared = 0
                stop = "step budget"
                for _ in range(4000):
                    ins = LISTING.getInstructionAt(addr)
                    if ins is not None:
                        # The gap is bounded by definition (it sits between two body ranges), so the
                        # next decoded instruction IS the far edge. Do NOT consult
                        # getFunctionContaining() as a stop condition: it reports the enclosing
                        # function for an address in the hole (its body test is coarse), which made
                        # the first hole "already carved" and stopped the walk instantly.
                        stop = "reached a decoded instruction"
                        break
                    do = LISTING.getDataAt(addr)
                    if do is not None:
                        try:
                            # clearCodeUnits is NOT a method on FlatProgramAPI or on ProgramDB in this
                            # Ghidra version (both raised, and that cost two rounds). It lives on the
                            # app command ghidra.app.cmd.disassemble.ClearCodeUnitsCmd, which is the
                            # same thing the GUI's "clear" action runs.
                            cmd = ClearCodeUnitsCmd(addr, do.getMaxAddress(), False)
                            if not cmd.applyTo(prog, mon):
                                stop = "clear rejected: %s" % cmd.getStatusMsg()
                                break
                            n_cleared += 1
                        except Exception as exc:
                            stop = "clear failed: %s" % exc
                            break
                        continue
                    try:
                        api.disassemble(addr)
                    except Exception as exc:
                        stop = "disassemble failed: %s" % exc
                        break
                    ins = LISTING.getInstructionAt(addr)
                    if ins is None:
                        stop = "disassemble produced nothing"
                        break
                    n_new += 1
                    addr = ins.getMaxAddress().next()
                if n_new:
                    code_applied += 1
                    say("  ~ %s: +%d instructions, %d data items cleared (stopped: %s)" %
                        (hexaddr, n_new, n_cleared, stop))
                else:
                    code_skipped += 1
                    say("  ! %s: filled nothing (%s)" % (hexaddr, stop))
        finally:
            prog.endTransaction(tx, True)

        say("applied %d, already-carved %d, failed %d; code holes filled %d, blocked %d"
            % (applied, skipped, failed, code_applied, code_skipped))

        # Deliberately NO analysis here. Two attempts at driving it from a script both failed, and the
        # second is the informative one: analyzeAll dispatches analyzer workers across a ConcurrentQ,
        # and one Ghidra transaction is not visible to those threads, so every worker died with
        # db.NoTransactionException at DBHandle.checkTransaction. Ghidra's own headless pipeline owns
        # that transaction correctly, so analysis is a SEPARATE pass - run afterwards:
        #     analyzeHeadless.bat <projDir> <projName> -process SporeApp.exe.unpacked.exe
        # which re-analyzes the changed locations and saves. This script only creates and saves.

        # GhidraFile.save takes a TaskMonitor only; the change comment lives on the transaction.
        df.save(mon)
        say("saved. function count now %d" % FM.getFunctionCount())
    finally:
        prog.release(consumer)
    return 0


if __name__ == "__main__":
    sys.exit(main())
