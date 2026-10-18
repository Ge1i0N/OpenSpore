#pragma once
// Consolidated logging: verbose in the file, quiet in the console.
//
// PROMPT.md asks for "подробное, но консолидированное" logging. The mechanism is a
// per-call-site ledger: the first occurrence of a site prints in full, later ones are
// counted and only summarised, so an assert storm or a per-frame warning cannot flood the
// console while the file still holds every distinct message with its file:line.
//
// The original's own log surface, for reference (level 1, refs/bins/ReleaseGA):
//   0x01413bf0 u 'Logging.txt'        referenced from 0x007e7c85
//   0x01413c08 u 'LocalLogging.txt'   referenced from 0x007e7c5b
//   0x013f8cb0 a 'Stack trace is:'    referenced from 0x007e... (crash path)
// The app normally supplies unique logs/OpenSpore-<timestamp>-pid<id>.log paths, so every run
// remains available for comparison. The sink keeps the same share-readable/tailable property.
// (_SH_DENYNO) precisely so it can be tailed while the process is alive (field guide §2).
#include <cstdarg>

#include "common.h"

namespace os::log {

enum class Level : u8 { Fatal = 0, Error, Warn, Info, Debug, Trace };

void init(const char* path, Level file_level, Level console_level);
void shutdown();

void set_levels(Level file_level, Level console_level);
Level file_level();
Level console_level();

// The consolidated entry point. `site` is __FILE__:__LINE__ supplied by the macros below.
void write(Level level, const char* site, const char* tag, const char* fmt, ...);
void vwrite(Level level, const char* site, const char* tag, const char* fmt, va_list ap);

// Bypass the per-site ledger. For one-per-run lines (boot markers, shutdown summaries)
// where suppression would be a bug rather than a feature: the first run's boot chain
// printed markers 1-8 and then "x9, further repeats suppressed", because every marker
// shares the mark() call site and the ledger keys on that site string.
void write_once(Level level, const char* site, const char* tag, const char* fmt, ...);

// Plain-C escape hatch for the diagnostic hooks in diag.cpp, which run inside the CRT's own
// report path and the loader's exception filter. It must not allocate, must not take the
// C++ stream locks, and must not call back into write(). Implemented as fwrite under the
// same critical section, so interleaving with normal log lines is still serialised.
void raw_write(const char* data, size_t len);

// Flush the file sink (called after every Fatal/Error line and by the crash handler).
void flush();

// Dump the per-site ledger, so "we saw 4120 of these" is auditable rather than implied.
void dump_ledger();

u64 lines_written();

}  // namespace os::log

#define OS_SITE_ __FILE__ ":" OS_STR_(__LINE__)
#define OS_STR_(x) OS_LIT_(x)
#define OS_LIT_(x) #x

// Ledger-bypassing variants: for one-per-run lines where suppression is a bug.
#define LOG_ONCE_FATAL(tag, ...) ::os::log::write_once(::os::log::Level::Fatal, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ONCE_ERROR(tag, ...) ::os::log::write_once(::os::log::Level::Error, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ONCE_WARN(tag, ...) ::os::log::write_once(::os::log::Level::Warn, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ONCE_INFO(tag, ...) ::os::log::write_once(::os::log::Level::Info, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ONCE_DEBUG(tag, ...) ::os::log::write_once(::os::log::Level::Debug, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ONCE_TRACE(tag, ...) ::os::log::write_once(::os::log::Level::Trace, OS_SITE_, tag, __VA_ARGS__)

#define LOG_FATAL(tag, ...) ::os::log::write(::os::log::Level::Fatal, OS_SITE_, tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) ::os::log::write(::os::log::Level::Error,  OS_SITE_, tag, __VA_ARGS__)
#define LOG_WARN(tag, ...)  ::os::log::write(::os::log::Level::Warn,   OS_SITE_, tag, __VA_ARGS__)
#define LOG_INFO(tag, ...)  ::os::log::write(::os::log::Level::Info,   OS_SITE_, tag, __VA_ARGS__)
#define LOG_DEBUG(tag, ...) ::os::log::write(::os::log::Level::Debug,  OS_SITE_, tag, __VA_ARGS__)
#define LOG_TRACE(tag, ...) ::os::log::write(::os::log::Level::Trace,  OS_SITE_, tag, __VA_ARGS__)
