#pragma once
// Observation infrastructure. Built before any engine code, per field guide §4:
// "Instrument before you experiment."
//
// Three hard rules, all of which come from bugs the field guide documents as costing days:
//   1. No modal dialog may ever block the process. assert() in the UCRT ends in abort();
//      suppressing only the dialog turns a failed assert into a SILENT process death, so
//      we neutralise the abort too and make every assert log-and-continue.
//   2. All CRT debug reports go to a file through plain-C stdio under a critical section
//      (the hook runs inside the CRT's own report path; re-entering the logger or the
//      allocator risks recursion/deadlock).
//   3. A crash must produce a symbolised stack in the log, never just an exit code.
//      The original itself does this: it imports dbghelp SymInitialize / SymLoadModule64 /
//      SymGetSymFromAddr64 / SymGetLineFromAddr64 / SymSetOptions / UnDecorateSymbolName /
//      SymCleanup and ships the string "Stack trace is:" at 0x013f8cb0 (referenced from
//      .text). We reproduce the capability, not the code.
#include <windows.h>

#include "common.h"

namespace os::diag {

// Install everything: CRT report hook, non-fatal assert machinery, unhandled-exception
// filter, and the watson/abort neutralisers. Idempotent. `crash_log` receives the stacks.
void install(const char* crash_log_path);

// The minimal, allocation-free half of install(). MUST be the first statement of
// wWinMain: any CRT assert raised before this point (a static initialiser, a bad
// setvbuf size, a failed _fsopen) would otherwise put up a modal dialog and block the
// process, which PROMPT.md and field guide §4 forbid outright.
void early_neutralise();

// Called by OS_ASSERT: logs expression+file:line+per-site counter and RETURNS, so the
// caller continues. This is the "click Ignore automatically" behaviour the field guide
// demands, implemented without a dialog at all.
void assert_failed(const char* expr, const char* file, int line, const char* msg);

// For the game's own "unreachable" helpers: [[noreturn]] is preserved because callers
// rely on it, but the exit code is distinct so a stop in the log is unambiguous.
[[noreturn]] void unreachable(const char* file, int line, const char* msg);

// Symbolise `pc` (module-relative is handled internally) into `out`. Never allocates.
bool describe_address(void* pc, char* out, size_t cap);

// Walk and log the current thread's stack. Safe to call from an exception filter.
void log_stack(const char* why, CONTEXT* ctx, EXCEPTION_POINTERS* ep);

// Frame-loop guard. `run_guarded` executes fn(ctx) with the abort handler armed. If
// anything inside calls abort()/assert-with-abort, the stack is logged and control returns
// here, with true reported, so one bad frame kills the run but not the process.
//
// Design note: this frame contains ONLY pointers and a global jmp_buf - no object with a
// non-trivial destructor. That is deliberate: longjmp out of fn() will not unwind fn()'s
// locals (their destructors are skipped, so a resource leak is possible on the emergency
// path), but it will never corrupt the caller's frame. Keeping the checkpoint frame
// trivially destructible is what makes the recovery repeatable.
bool run_guarded(void (*fn)(void*), void* ctx, const char* what);

u32 assert_count();
u32 unreachable_count();

}  // namespace os::diag

// Non-fatal assert: logs the exact expression, file:line and a per-site counter, then
// continues. Use instead of <cassert> everywhere in this project.
#define OS_ASSERT(expr)                                                             \
    do {                                                                            \
        if (!(expr)) ::os::diag::assert_failed(#expr, __FILE__, __LINE__, nullptr); \
    } while (0)

#define OS_ASSERT_MSG(expr, msg)                                                    \
    do {                                                                            \
        if (!(expr)) ::os::diag::assert_failed(#expr, __FILE__, __LINE__, (msg));    \
    } while (0)

#define OS_UNREACHABLE(msg) ::os::diag::unreachable(__FILE__, __LINE__, (msg))
