"""Run one persistent-project PyGhidra probe with a visible, bounded lifetime.

Examples:
    python tools/ghidra_run.py tools/ghidra_xref.py --string UI/DebugConsole
    python tools/ghidra_run.py --timeout 600 tools/ghidra_decompile.py 0x00802dc0

The child process owns JPype/Ghidra. This is intentional: a timeout in the parent can then
terminate a stuck JVM without taking the caller's Python process with it. Windows gets a
CTRL_BREAK_EVENT first; taskkill /T /F is the bounded fallback after the grace period.
"""
from __future__ import annotations

import argparse
import datetime as dt
import os
import queue
import signal
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_PYTHON = ROOT / "refs" / "ghidra" / "venv" / "Scripts" / "python.exe"
DEFAULT_LOG_DIR = ROOT / "analysis" / "logs"
TIMEOUT_EXIT = 124


def parse_args() -> tuple[argparse.Namespace, list[str]]:
    ap = argparse.ArgumentParser(description=__doc__, add_help=True)
    ap.add_argument("--timeout", type=float, default=float(os.environ.get("GHIDRA_RUN_TIMEOUT", "120")),
                    help="hard deadline in seconds (default: 120; use 600+ for large probes)")
    ap.add_argument("--grace", type=float, default=10.0,
                    help="seconds between CTRL_BREAK_EVENT and hard kill (default: 10)")
    ap.add_argument("--heartbeat", type=float, default=10.0,
                    help="heartbeat interval in seconds (default: 10)")
    ap.add_argument("--python", type=Path, default=DEFAULT_PYTHON,
                    help="child Python executable; defaults to the pinned Ghidra venv")
    ap.add_argument("--log", type=Path, default=None,
                    help="output log path; default: analysis/logs/ghidra-<timestamp>-pid<PID>.log")
    ap.add_argument("script", type=Path, help="PyGhidra probe script")
    args, child_args = ap.parse_known_args()
    if args.timeout <= 0 or args.grace < 0 or args.heartbeat <= 0:
        ap.error("timeout must be positive; grace must be non-negative; heartbeat must be positive")
    return args, child_args


def timestamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d-%H%M%S")


def terminate_child(proc: subprocess.Popen[str], grace: float, log) -> str:
    """Request a graceful stop, then terminate the whole Windows process tree if needed."""
    log.write("[runner] deadline reached; sending CTRL_BREAK_EVENT\n")
    log.flush()
    graceful = False
    if os.name == "nt":
        try:
            proc.send_signal(signal.CTRL_BREAK_EVENT)
            graceful = True
        except (OSError, ValueError):
            pass
    else:
        try:
            proc.send_signal(signal.SIGINT)
            graceful = True
        except (OSError, ValueError):
            pass
    end = time.monotonic() + grace
    while proc.poll() is None and time.monotonic() < end:
        time.sleep(0.2)
    if proc.poll() is not None:
        return "graceful-stop"
    log.write("[runner] graceful stop did not finish; hard-killing process tree\n")
    log.flush()
    if os.name == "nt":
        subprocess.run(["taskkill", "/PID", str(proc.pid), "/T", "/F"],
                       stdout=log, stderr=subprocess.STDOUT, check=False,
                       creationflags=subprocess.CREATE_NO_WINDOW)
    else:
        proc.kill()
    return "hard-kill"


def main() -> int:
    args, child_args = parse_args()
    script = args.script.resolve()
    python = args.python.resolve()
    if not script.is_file():
        print(f"[ghidra-run] script not found: {script}", file=sys.stderr)
        return 2
    if not python.is_file():
        print(f"[ghidra-run] child Python not found: {python}", file=sys.stderr)
        return 2
    args.log = (args.log or (DEFAULT_LOG_DIR / f"ghidra-{timestamp()}-pid{os.getpid()}.log")).resolve()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    command = [str(python), "-u", str(script), *child_args]
    env = os.environ.copy()
    env["PYTHONUNBUFFERED"] = "1"
    env["PYTHONIOENCODING"] = "utf-8:replace"
    env.setdefault("GHIDRA_HEADLESS_MAXMEM", "2G")
    started = time.monotonic()
    print(f"[ghidra-run] timeout={args.timeout:.0f}s grace={args.grace:.0f}s")
    print(f"[ghidra-run] log={args.log}")
    print(f"[ghidra-run] command={' '.join(command)}", flush=True)
    with args.log.open("w", encoding="utf-8", newline="\n") as log:
        log.write(f"[runner] started={dt.datetime.now().isoformat()} timeout={args.timeout} grace={args.grace}\n")
        log.write(f"[runner] command={' '.join(command)}\n")
        log.flush()
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
        proc = subprocess.Popen(command, cwd=str(ROOT), env=env, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, bufsize=1,
                                creationflags=creationflags)
        lines: queue.Queue[str] = queue.Queue()

        def reader() -> None:
            assert proc.stdout is not None
            for line in proc.stdout:
                lines.put(line)

        thread = threading.Thread(target=reader, name="ghidra-output", daemon=True)
        thread.start()
        next_heartbeat = started + args.heartbeat
        while proc.poll() is None:
            try:
                line = lines.get(timeout=0.25)
                log.write(line)
                log.flush()
                sys.stdout.write(line)
                sys.stdout.flush()
            except queue.Empty:
                pass
            now = time.monotonic()
            if now >= next_heartbeat:
                elapsed = now - started
                heartbeat = f"[ghidra-run] heartbeat elapsed={elapsed:.1f}s child_pid={proc.pid}\n"
                log.write(heartbeat)
                log.flush()
                sys.stdout.write(heartbeat)
                sys.stdout.flush()
                next_heartbeat = now + args.heartbeat
            if now - started >= args.timeout:
                mode = terminate_child(proc, args.grace, log)
                proc.wait()
                print(f"[ghidra-run] TIMEOUT mode={mode} log={args.log}", flush=True)
                return TIMEOUT_EXIT
        while True:
            try:
                line = lines.get_nowait()
            except queue.Empty:
                break
            log.write(line)
            sys.stdout.write(line)
        log.write(f"[runner] completed elapsed={time.monotonic() - started:.1f}s exit={proc.returncode}\n")
    print(f"[ghidra-run] completed exit={proc.returncode} log={args.log}", flush=True)
    return int(proc.returncode or 0)


if __name__ == "__main__":
    raise SystemExit(main())
