#include "engine/log.h"

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <share.h>  // _SH_DENYNO, _fsopen_s

namespace os::log {
namespace {

// Per-call-site ledger: this is what makes the log "consolidated" instead of chatty.
// Keyed by the __FILE__:__LINE__ string literal pointer, which is stable for the process.
constexpr u32 kMaxSites = 256;
constexpr u32 kReportsPerSite = 8;   // full prints per site before going silent
constexpr u32 kTotalReports = 4000;  // hard ceiling for the whole process

struct Site {
    const char* key;
    u32 prints;
    u64 hits;
};

Site g_sites[kMaxSites];
u32 g_site_count = 0;
u32 g_total_reports = 0;

FILE* g_file = nullptr;
Level g_file_level = Level::Info;
Level g_console_level = Level::Info;
bool g_console_attached = false;
bool g_ready = false;
u64 g_lines = 0;

CRITICAL_SECTION g_cs;

const char* level_name(Level l) {
    switch (l) {
        case Level::Fatal: return "FATAL";
        case Level::Error: return "ERROR";
        case Level::Warn: return "WARN ";
        case Level::Info: return "INFO ";
        case Level::Debug: return "DEBUG";
        default: return "TRACE";
    }
}

Site* find_or_add(const char* key) {
    for (u32 i = 0; i < g_site_count; ++i) {
        if (g_sites[i].key == key) return &g_sites[i];
    }
    if (g_site_count < kMaxSites) {
        Site& s = g_sites[g_site_count++];
        s.key = key;
        s.prints = 0;
        s.hits = 0;
        return &s;
    }
    return nullptr;  // ledger full: counts degrade, nothing crashes and nothing floods
}

void timestamp(char* buf, size_t n) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    _snprintf_s(buf, n, _TRUNCATE, "%04u-%02u-%02u %02u:%02u:%02u.%03u +%-7llu", st.wYear, st.wMonth, st.wDay, st.wHour,
                st.wMinute, st.wSecond, st.wMilliseconds, (unsigned long long)GetTickCount64());
}

void emit_locked(Level level, const char* site, const char* tag, const char* text, bool suppressed, u64 hits) {
    if (!g_file && !g_console_attached) return;
    char ts[80];
    timestamp(ts, sizeof(ts));
    char line[2048];
    if (suppressed) {
        _snprintf_s(line, sizeof(line), _TRUNCATE, "[%s] %s [%s] %s  (%s: x%llu, further repeats suppressed)\n", ts,
                    level_name(level), tag ? tag : "-", text, site, (unsigned long long)hits);
    } else {
        _snprintf_s(line, sizeof(line), _TRUNCATE, "[%s] %s [%s] %s  (%s)\n", ts, level_name(level), tag ? tag : "-",
                    text, site);
    }
    ++g_lines;
    if (g_file && level <= g_file_level) {
        fputs(line, g_file);
        // Flush EVERY line, not only Error/Fatal. The first version flushed only on
        // Error/Fatal and relied on _IOLBF to do the rest, but setvbuf(_IOLBF) does not
        // take effect on a _fsopen handle here, so the file stayed fully buffered and
        // everything after the last forced flush was lost when the process was killed.
        // That hid an entire working frame loop behind a log that looked like a hang.
        // A log whose tail cannot be trusted is worse than a slightly slower one.
        fflush(g_file);
    }
    if (g_console_attached && level <= g_console_level) {
        fputs(line, stdout);
        fflush(stdout);
        OutputDebugStringA(line);
    }
}

}  // namespace

void init(const char* path, Level file_level, Level console_level) {
    InitializeCriticalSection(&g_cs);
    g_file_level = file_level;
    g_console_level = console_level;
    g_console_attached = GetConsoleWindow() != nullptr;
    if (path) {
        // Share-readable on purpose (field guide §2): an exclusively-locked log cannot be
        // tailed while the process lives, and "tail the log" is our primary observation tool.
        // NOTE: the API is _fsopen (returns FILE*); there is no _fsopen_s.
        FILE* f = _fsopen(path, "a", _SH_DENYNO);
        if (!f) fopen_s(&f, path, "a");  // fall back rather than lose the log entirely
        if (f) {
            // setvbuf with buffer=NULL makes the CRT allocate its own buffer of THIS size.
            // Passing 0 is invalid and trips the UCRT assert
            //   setvbuf.cpp:57: 2 <= buffer_size_in_bytes && buffer_size_in_bytes <= INT_MAX
            // which is exactly what killed the first run (modal dialog, then 0xC0000409).
            // 64 KiB is generous for line-buffered text and keeps one syscall per burst.
            if (setvbuf(f, nullptr, _IOLBF, 64 * 1024) != 0) {
                // Some CRT builds reject _IOLBF on a share-opened handle; full buffering is
                // still correct because emit_locked() flushes on Error/Fatal.
                setvbuf(f, nullptr, _IOFBF, 64 * 1024);
            }
            g_file = f;
        }
    }
    g_ready = true;
}

void shutdown() {
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    if (g_file) {
        fflush(g_file);
        fclose(g_file);
        g_file = nullptr;
    }
    g_ready = false;
    LeaveCriticalSection(&g_cs);
    DeleteCriticalSection(&g_cs);
}

void set_levels(Level file_level, Level console_level) {
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    g_file_level = file_level;
    g_console_level = console_level;
    LeaveCriticalSection(&g_cs);
}

Level file_level() { return g_file_level; }
Level console_level() { return g_console_level; }

void raw_write(const char* data, size_t len) {
    // Plain-C only: this is called from the CRT report hook and the exception filter,
    // where allocating or taking a C++ lock risks recursion/deadlock (field guide §4).
    if (!g_file || len == 0) return;
    fwrite(data, 1, len, g_file);
    fflush(g_file);
}

void flush() {
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    if (g_file) fflush(g_file);
    if (g_console_attached) fflush(stdout);
    LeaveCriticalSection(&g_cs);
}

void dump_ledger() {
    if (!g_ready) return;
    EnterCriticalSection(&g_cs);
    for (u32 i = 0; i < g_site_count; ++i) {
        const Site& s = g_sites[i];
        if (s.hits <= s.prints) continue;  // already fully reported
        char buf[256];
        _snprintf_s(buf, sizeof(buf), _TRUNCATE, "[ledger] %s hits=%llu printed=%zu suppressed=%llu\n", s.key,
                    (unsigned long long)s.hits, (size_t)s.prints, (unsigned long long)(s.hits - s.prints));
        if (g_file) fputs(buf, g_file);
        if (g_console_attached) {
            fputs(buf, stdout);
            fflush(stdout);
        }
        ++g_lines;
    }
    LeaveCriticalSection(&g_cs);
}

u64 lines_written() { return g_lines; }

void vwrite(Level level, const char* site, const char* tag, const char* fmt, va_list ap) {
    if (!g_ready) return;
    char text[1600];
    va_list copy;
    va_copy(copy, ap);
    vsnprintf(text, sizeof(text), fmt, copy);
    va_end(copy);
    text[sizeof(text) - 1] = '\0';

    EnterCriticalSection(&g_cs);
    Site* s = find_or_add(site);
    bool suppressed = false;
    if (s) {
        ++s->hits;
        if (s->prints >= kReportsPerSite || g_total_reports >= kTotalReports) {
            suppressed = true;
            // Counting continues past the cap; reporting does not. Emit the summary once,
            // at the moment the cap is crossed, so the first occurrence is always visible.
            if (s->prints == kReportsPerSite || (s->prints < kReportsPerSite && g_total_reports == kTotalReports)) {
                ++s->prints;
                ++g_total_reports;
                emit_locked(level, site, tag, text, true, s->hits);
            }
        } else {
            ++s->prints;
            ++g_total_reports;
            emit_locked(level, site, tag, text, false, s->hits);
        }
    }
    LeaveCriticalSection(&g_cs);
}

void write(Level level, const char* site, const char* tag, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vwrite(level, site, tag, fmt, ap);
    va_end(ap);
}

void write_once(Level level, const char* site, const char* tag, const char* fmt, ...) {
    if (!g_ready) return;
    char text[1600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof(text), fmt, ap);
    va_end(ap);
    text[sizeof(text) - 1] = '\0';
    EnterCriticalSection(&g_cs);
    emit_locked(level, site, tag, text, false, 1);
    LeaveCriticalSection(&g_cs);
}

}  // namespace os::log
