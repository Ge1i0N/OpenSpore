#pragma once
// Developer console + command registry.
//
// PROMPT.md: "в виде Debug версии, т.е. будут доступны девелоперские команды". The
// original ships its own console command system (evidence: help-string clusters in
// .rdata, e.g. 'State manager control. With no arguments lists current state.' at
// 0x014136dc, '-list^' 0x0140e430, '-next^'/'-prev^'/'-reload^', and the ArgScript/*
// families). We mirror the *interface*: commands with '^'-separated help options.
//
// The console reads commands from stdin on a worker thread (only when a real console is
// attached), so it works unattended and from a pipe; the echo/tick happens on the main
// thread. Every command invocation is logged with its site counter, so the log stays
// consolidated even under scripted spam.

#include <functional>
#include <string>
#include <vector>

#include "common.h"

namespace os::console {

struct CmdContext {
    std::vector<std::string> args;  // argv[0] = command name, lowercased
};

using Handler = std::function<std::string(const CmdContext&)>;  // return = echo text (may be empty)

struct Command {
    std::string name;
    std::string help;    // one line; options separated by '^' like the original's strings
    std::string options;  // e.g. "-list^-next^-prev^-reload"
    Handler handler;
};

void init(bool enable_dev);
void shutdown();

// Registration is static-init friendly: commands register from their own TUs.
void reg(const Command& cmd);

bool exec_line(const std::string& line, std::string& out_result);
void exec_file(const std::string& path);  // 'sinclude' analog (key seen at 0x01413be0)

void start_stdin_thread();   // no-op without an attached console
void stop_stdin_thread();

// Pump queued stdin lines on the main thread. Call once per frame.
void poll();

// Threading contract: every mutation of the command list (init/reg) happens on the main
// thread during boot, and handlers run on the main thread via poll()/exec_line(). The
// stdin worker only enqueues text, it never touches the list. That is why help_command may
// iterate without holding g_mux; count() locks anyway because it is called from the boot
// log path where the invariant is less obvious.
const std::vector<Command>& commands();  // main-thread use only; prefer count()

size_t count();  // lock-safe size, for boot logging and the `help` verb

}  // namespace os::console
