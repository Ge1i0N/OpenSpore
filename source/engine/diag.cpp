#include "engine/diag.h"

#include <windows.h>
#include <winsock2.h>  // WIN32_LEAN_AND_MEAN strips this, but dbghelp.h needs it
#include <dbghelp.h>

#include <csetjmp>
#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <setjmp.h>
#include <stdlib.h>

#include "engine/log.h"

#ifdef _DEBUG
#include <crtdbg.h>
#endif

#pragma comment(lib, "dbghelp.lib")

namespace os::diag {
namespace {

bool g_installed = false;
u32 g_assert_count = 0;
u32 g_unreachable_count = 0;
HANDLE g_crash_file = INVALID_HANDLE_VALUE;
CHAR g_crash_path[MAX_PATH] = {};

// Abort recovery: the field guide's rule is that suppressing the assert *dialog* without
// neutralising the *abort* is worse than useless (silent death, no stack, no log tail).
// We go further: a SIGABRT anywhere in the process longjmps back to the frame loop, so the
// run survives and the log records exactly where it tried to die.
jmp_buf g_abort_checkpoint;
volatile bool g_abort_armed = false;
volatile LONG g_abort_hits = 0;
ULONGLONG g_abort_window_start = 0;
LONG g_abort_window_hits = 0;

void crash_write(const char* text) {
    if (!text) return;
    size_t len = strlen(text);
    if (g_crash_file != INVALID_HANDLE_VALUE && len) {
        DWORD written = 0;
        WriteFile(g_crash_file, text, (DWORD)len, &written, nullptr);
    }
    log::raw_write(text, len);
}

void crash_printf(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    _vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);
    crash_write(buf);
}

bool sym_init() {
    static bool done = false;
    if (done) return true;
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS);
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) return false;
    done = true;
    return true;
}

bool describe_pc(void* pc, char* out, size_t cap) {
    if (!sym_init()) return false;
    char sym_buf[sizeof(SYMBOL_INFO) + 256] = {};
    SYMBOL_INFO* sym = reinterpret_cast<SYMBOL_INFO*>(sym_buf);
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(line);
    DWORD disp32 = 0;
    DWORD64 disp64 = 0;
    BOOL have_line = SymGetLineFromAddr64(GetCurrentProcess(), (DWORD64)(uintptr_t)pc, &disp32, &line);
    if (SymFromAddr(GetCurrentProcess(), (DWORD64)(uintptr_t)pc, &disp64, sym)) {
        if (have_line) {
            _snprintf_s(out, cap, _TRUNCATE, "%s @ %s:%lu", sym->Name, line.FileName, line.LineNumber);
        } else {
            _snprintf_s(out, cap, _TRUNCATE, "%s + 0x%llx", sym->Name, (unsigned long long)disp64);
        }
        return true;
    }
    if (have_line) {
        _snprintf_s(out, cap, _TRUNCATE, "%s:%lu", line.FileName, line.LineNumber);
        return true;
    }
    _snprintf_s(out, cap, _TRUNCATE, "0x%p", pc);
    return false;
}

const char* exception_name(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "ARRAY_BOUNDS";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "MISALIGNMENT";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIV0";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIV0";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
        case 0xE06D7363: return "CPP_EXCEPTION";       // MSVC: 'E' 'm' 's' 'c' + 1
        case 0xE0434352: return "CLR_EXCEPTION";
        default: return "UNKNOWN";
    }
}

// C4702 (unreachable code) is unavoidable here: ExitProcess is terminal, so the return that
// the signature demands can never execute. Suppressed for the whole function body rather
// than one line, because MSVC attributes the warning to the function, not the statement.
#pragma warning(push)
#pragma warning(disable : 4702)
LONG WINAPI unhandled_filter(EXCEPTION_POINTERS* ep) {
    log_stack("unhandled exception", nullptr, ep);
    log::flush();
    // Distinct, greppable exit code so a crash is never mistaken for a clean shutdown.
    DWORD code = ep->ExceptionRecord->ExceptionCode;
    ExitProcess(0x4F530000u ^ code);  // 'OS' prefix + the win32 code
    return EXCEPTION_EXECUTE_HANDLER;
}
#pragma warning(pop)

void abort_handler(int sig) {
    (void)sig;
    LONG hits = InterlockedIncrement(&g_abort_hits);
    // Repeatable recovery, bounded: N aborts in quick succession means we are stuck in a
    // loop, not that one bad frame happened. Then we let it die with a distinct code.
    ULONGLONG now = GetTickCount64();
    if (now - g_abort_window_start > 1000) {
        g_abort_window_start = now;
        g_abort_window_hits = 0;
    }
    if (++g_abort_window_hits > 5) {
        crash_printf("abort storm: %ld hits, %ld in the last second - giving up\n", hits, g_abort_window_hits);
        log::flush();
        ExitProcess(0x4F53AB02u);
    }
    char stack[512];
    stack[0] = '\0';
    void* frames[32];
    USHORT n = RtlCaptureStackBackTrace(0, 32, (void**)frames, nullptr);
    for (USHORT i = 0; i < n && i < 12; ++i) {
        char d[192];
        describe_address(frames[i], d, sizeof(d));
        char l[224];
        _snprintf_s(l, sizeof(l), _TRUNCATE, "  #%u 0x%p %s\n", i, frames[i], d);
        strncat_s(stack, sizeof(stack), l, _TRUNCATE);
    }
    crash_printf("\n=== abort() intercepted (hit #%ld, %ld in window) ===\n%s\n", hits, g_abort_window_hits, stack);
    log::flush();
    if (g_abort_armed) {
        // Longjmp back into the guarded frame: the process survives, the log keeps the stack.
        ::longjmp(g_abort_checkpoint, 1);
    }
    ExitProcess(0x4F53AB01u);
}

#ifdef _DEBUG
// CRT debug report hook. Plain C stdio under a critical section, because this runs inside
// the CRT's own report path (re-entering the logger or allocator risks recursion/deadlock).
// _CRT_REPORT_HOOK is __cdecl; declaring this NTAPI was the first build's error.
int crt_report_hook(int report_type, char* msg, int* ret) {
    (void)report_type;
    if (msg) {
        size_t len = strlen(msg);
        log::raw_write(msg, len);
        if (len == 0 || msg[len - 1] != '\n') log::raw_write("\n", 1);
    }
    *ret = 1;  // handled: do not let the CRT show its own dialog
    return 1;
}
#endif

}  // namespace

void early_neutralise() {
    static bool done = false;
    if (done) return;
    done = true;
    // 1. Never let the CRT show a modal box, and never let abort() be silent.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    signal(SIGABRT, abort_handler);
#ifdef _DEBUG
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
#endif
    // 2. Unhandled exceptions: symbolised stack to the log, then a distinct exit code.
    SetUnhandledExceptionFilter(unhandled_filter);
}

void install(const char* crash_log_path) {
    early_neutralise();
    if (g_installed) return;
    g_installed = true;

    if (crash_log_path) {
        lstrcatA(g_crash_path, crash_log_path);
        g_crash_file = CreateFileA(crash_log_path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
#ifdef _DEBUG
    // Route the CRT's own reports into our (share-readable) log file as well, via a plain-C
    // hook. This is added on top of early_neutralise()'s stderr routing.
    _CrtSetReportHook(crt_report_hook);
#endif
    crash_printf("\n=== OpenSpore diagnostic harness installed (pid %lu) ===\n", GetCurrentProcessId());
}

void assert_failed(const char* expr, const char* file, int line, const char* msg) {
    InterlockedIncrement((LONG*)&g_assert_count);
    // Goes through the normal logger, so the per-site ledger suppresses the storm while the
    // first occurrence keeps its exact expression and file:line (field guide §4.2).
    if (msg) {
        log::write(log::Level::Error, file, "assert", "%s  [%s]", expr, msg);
    } else {
        log::write(log::Level::Error, file, "assert", "%s", expr);
    }
    char stack[512];
    stack[0] = '\0';
    void* frames[24];
    USHORT n = RtlCaptureStackBackTrace(1, 24, (void**)frames, nullptr);
    for (USHORT i = 0; i < n && i < 8; ++i) {
        char d[192];
        describe_address(frames[i], d, sizeof(d));
        char l[224];
        _snprintf_s(l, sizeof(l), _TRUNCATE, "  #%u %s\n", i, d);
        strncat_s(stack, sizeof(stack), l, _TRUNCATE);
    }
    crash_printf("assert %s at %s:%d\n%s", expr, file, line, stack);
    // and RETURN: the caller continues. This is the automatic "Ignore".
}

void unreachable(const char* file, int line, const char* msg) {
    InterlockedIncrement((LONG*)&g_unreachable_count);
    log::write(log::Level::Fatal, file, "unreachable", "%s", msg ? msg : "unreachable reached");
    crash_printf("\n=== unreachable at %s:%d: %s ===\n", file, line, msg ? msg : "");
    log_stack("unreachable", nullptr, nullptr);
    log::flush();
    // [[noreturn]] is preserved: callers end their last branch with this and have no return.
    // Distinct exit code so it is never confused with an assert or a 0xC0000005.
    ExitProcess(0x4F5300FEu);
}

bool describe_address(void* pc, char* out, size_t cap) {
    if (!pc) {
        _snprintf_s(out, cap, _TRUNCATE, "(nil)");
        return false;
    }
    return describe_pc(pc, out, cap);
}

void log_stack(const char* why, CONTEXT* ctx, EXCEPTION_POINTERS* ep) {
    char line[256];
    if (ep) {
        EXCEPTION_RECORD* r = ep->ExceptionRecord;
        crash_printf("\n--- stack: %s ---\n", why);
        crash_printf("exception 0x%08lx (%s) flags 0x%08lx addr 0x%p\n", r->ExceptionCode, exception_name(r->ExceptionCode),
                     r->ExceptionFlags, r->ExceptionAddress);
        if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
            crash_printf("  access type=%llu target=0x%p\n", (unsigned long long)r->ExceptionInformation[0],
                         (void*)(uintptr_t)r->ExceptionInformation[1]);
        }
        if (ep->ContextRecord) ctx = ep->ContextRecord;
    } else {
        crash_printf("\n--- stack: %s ---\n", why);
    }

    STACKFRAME64 sf = {};
    ADDRESS64 addr = {};
    DWORD machine = IMAGE_FILE_MACHINE_I386;
    if (!ctx) {
        // No context handed to us: capture this thread's own stack. RtlCaptureStackBackTrace
        // is the architecture-independent route and needs no CONTEXT at all.
        void* frames[48];
        USHORT n = RtlCaptureStackBackTrace(1, 48, (void**)frames, nullptr);
        for (USHORT i = 0; i < n; ++i) {
            char d[192];
            describe_address(frames[i], d, sizeof(d));
            _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%u 0x%p %s\n", i, frames[i], d);
            crash_write(line);
        }
        return;
    }
#ifdef _WIN64
    machine = IMAGE_FILE_MACHINE_AMD64;
    sf.AddrPC.Offset = ctx->Rip;
    sf.AddrFrame.Offset = ctx->Rbp;
    sf.AddrStack.Offset = ctx->Rsp;
#else
    sf.AddrPC.Offset = ctx->Eip;
    sf.AddrFrame.Offset = ctx->Ebp;
    sf.AddrStack.Offset = ctx->Esp;
#endif
    sf.AddrPC.Mode = AddrModeFlat;
    sf.AddrFrame.Mode = AddrModeFlat;
    sf.AddrStack.Mode = AddrModeFlat;

    if (!sym_init()) {
        crash_write("  (dbghelp: SymInitialize failed)\n");
        return;
    }
    for (int i = 0; i < 48; ++i) {
        if (!StackWalk64(machine, GetCurrentProcess(), GetCurrentThread(), &sf, ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)) {
            break;
        }
        if (sf.AddrPC.Offset == 0) break;
        char d[192];
        describe_address((void*)(uintptr_t)sf.AddrPC.Offset, d, sizeof(d));
        _snprintf_s(line, sizeof(line), _TRUNCATE, "  #%02d 0x%016llx %s\n", i,
                    (unsigned long long)sf.AddrPC.Offset, d);
        crash_write(line);
    }
}

u32 assert_count() { return (u32)g_assert_count; }
u32 unreachable_count() { return (u32)g_unreachable_count; }

// The setjmp must live in a frame with no C++ objects and no C++ calls of its own:
// MSVC C4611 ("interaction between _setjmp and C++ object destruction is non-portable")
// fires for ANY function call made from a frame containing _setjmp, because a longjmp back
// out of a callee skips that callee's destructors. We accept that, deliberately:
//   * the invariant that keeps it safe: `fn` is a plain function pointer and the guarded
//     region (states::Manager::tick/render) allocates nothing that owns a lock;
//   * the known cost: if a future guarded region takes a lock and aborts inside it, that
//     lock stays held. The log's own critical section is entered only via raw_write on the
//     crash path (no re-entry), so the abort handler cannot deadlock on it;
//   * the alternative is strictly worse: per field guide §4, an un-neutralised abort is a
//     SILENT process death with no stack and no log tail. Best-effort recovery plus a
//     recorded stack beats a clean corpse.
// MSVC is our only compiler, so "non-portable" is not a cost we pay.
#pragma warning(push)
#pragma warning(disable : 4611)
extern "C" {
int os_guarded_call(void (*fn)(void*), void* ctx) {
    if (_setjmp(g_abort_checkpoint) != 0) return 1;  // recovered from an abort
    fn(ctx);
    return 0;
}
}
#pragma warning(pop)

bool run_guarded(void (*fn)(void*), void* ctx, const char* what) {
    g_abort_armed = true;
    int rc = os_guarded_call(fn, ctx);
    g_abort_armed = false;
    if (rc != 0) {
        char buf[256];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "aborted inside guarded region: %s\n", what ? what : "?");
        crash_write(buf);
    }
    return rc != 0;
}

}  // namespace os::diag
