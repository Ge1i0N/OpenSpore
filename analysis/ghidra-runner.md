# Bounded Ghidra/PyGhidra runs

The persistent project at `refs/ghidra/proj/OpenSporeOracle` is the fast path: focused xref/list/
decompile probes reuse the existing analysis instead of importing the 25 MB GA `.text` again. A fresh
REA/Ghidra analysis is a different operation and can take many minutes.

## Runner

Use `tools/ghidra_run.py` for every potentially expensive PyGhidra script:

```powershell
# Normal focused probe: 120-second deadline
python tools/ghidra_run.py tools/ghidra_xref.py --string UI/DebugConsole

# Larger decompile or first-time seeded operation: explicitly extend the deadline
python tools/ghidra_run.py --timeout 600 tools/ghidra_decompile.py 0x00802DC0
```

The runner uses the pinned interpreter at `refs/ghidra/venv/Scripts/python.exe`, starts the probe in
a separate process, forwards output immediately, and emits a heartbeat every 10 seconds. Each run is
saved to `analysis/logs/ghidra-<timestamp>-pid<PID>.log`.

## Timeout policy

The default is **120 seconds**. A timeout is not inferred from silence: Ghidra may legitimately spend
time without printing. The deadline is based on wall-clock lifetime, while the heartbeat proves that
the parent runner is still responsive.

At the deadline on Windows:

1. send `CTRL_BREAK_EVENT` to the child process group;
2. wait 10 seconds by default (`--grace 20` can extend this);
3. if the JVM is still alive, run `taskkill /PID <pid> /T /F`.

The runner exits with **124** for a deadline, preserving the distinction from a probe's ordinary
non-zero failure. The log records `graceful-stop` or `hard-kill`. Hard kill is acceptable for a
disposable read-only probe, but a timeout should not be described as a completed Ghidra result.

For a known large feature, raise only the caller's deadline:

```powershell
python tools/ghidra_run.py --timeout 900 --grace 30 tools/ghidra_export.py --request analysis/audit/large.json
```

Do not raise the global default merely because one previous run was slow. Persistent-project probes
should remain bounded and focused; a full import/reanalysis needs an explicit larger budget and a
separate audit note.

For direct lifecycle caller checks:

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_callers.py `
  0x00802B20 0x00802C10 0x008029D0 0x00802B10 0x00802A40
```

The result lists static references only. Calls through vtables, message tables or computed dispatch
remain explicitly unresolved rather than being reported as absent.

To inspect the Canvas listener field without a broad reanalysis:

```powershell
python tools/ghidra_run.py --timeout 180 tools/ghidra_field_refs.py "+ 0x5c" 120 0x00800000
```
