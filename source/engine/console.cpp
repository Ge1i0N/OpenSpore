#include "engine/console.h"

#include <windows.h>

#include <atomic>
#include <cctype>  // tolower, used to normalise command names and their input
#include <cstdio>
#include <deque>
#include <mutex>
#include <thread>

#include "engine/log.h"

namespace os::console {
namespace {

std::vector<Command> g_cmds;
std::mutex g_mux;
bool g_dev = false;

// stdin worker -> queue -> main-thread poll. The worker never touches engine state itself.
std::deque<std::string> g_queue;
std::mutex g_qmux;
std::atomic<bool> g_run{false};
std::thread g_thread;

std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<std::string> split_args(const std::string& line) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        if (i >= line.size()) break;
        std::string tok;
        bool q = false;
        for (; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                q = !q;
                continue;
            }
            if (!q && (c == ' ' || c == '\t')) break;
            tok += c;
        }
        out.push_back(tok);
    }
    for (auto& t : out) {
        if (!t.empty() && t[0] == '-') {
            // keep flags as-is; command names get lowercased by the caller
        }
    }
    return out;
}

void stdin_worker() {
    char buf[4096];
    while (g_run.load()) {
        if (!fgets(buf, sizeof(buf), stdin)) break;  // EOF (pipe closed) or error
        std::string line = trim(buf);
        if (line.empty()) continue;
        {
            std::lock_guard<std::mutex> lk(g_qmux);
            if (g_queue.size() < 256) g_queue.push_back(std::move(line));
        }
    }
    g_run.store(false);
}

std::string help_command(const CmdContext& c) {
    if (c.args.size() > 1) {
        for (auto& cmd : g_cmds) {
            if (cmd.name == c.args[1]) {
                std::string s = cmd.name + " - " + cmd.help;
                if (!cmd.options.empty()) s += "  [" + cmd.options + "]";
                return s;
            }
        }
        return "no such command: " + c.args[1];
    }
    std::string s = "commands:";
    for (auto& cmd : g_cmds) {
        if (!g_dev && cmd.name.size() > 0 && cmd.name[0] == '_') continue;  // _-prefix = dev-only
        s += " " + cmd.name;
    }
    return s;
}

std::string loglevel_command(const CmdContext& c) {
    if (c.args.size() < 2) return "logLevel <fatal|error|warn|info|debug|trace>";
    std::string v = c.args[1];
    auto parse = [](const std::string& s, log::Level& out) {
        if (s == "fatal") out = log::Level::Fatal;
        else if (s == "error") out = log::Level::Error;
        else if (s == "warn") out = log::Level::Warn;
        else if (s == "info") out = log::Level::Info;
        else if (s == "debug") out = log::Level::Debug;
        else if (s == "trace") out = log::Level::Trace;
        else return false;
        return true;
    };
    log::Level fl = log::file_level(), cl = log::console_level();
    if (!parse(v, fl) || !parse(v, cl)) return "bad level: " + v;
    log::set_levels(fl, cl);
    return "log levels -> file/console = " + v;
}

}  // namespace

void init(bool enable_dev) {
    g_dev = enable_dev;
    {
        // Clear under the lock, then RELEASE it before registering: std::mutex is
        // non-recursive, and reg() locks it again. Holding it across reg() threw
        // _Mutex_base::lock's future_error on the very first run (the crash harness
        // symbolised it: console.cpp:144 <- console.cpp:118 <- app.cpp:300).
        std::lock_guard<std::mutex> lk(g_mux);
        g_cmds.clear();
    }
    reg({"help", "List commands, or show help for one.", "-<name>", help_command});
    reg({"logLevel", "Set file+console verbosity.", "", loglevel_command});
    reg({"echo", "Echo arguments back (harness sanity probe).", "",
         [](const CmdContext& c) {
             std::string s;
             for (size_t i = 1; i < c.args.size(); ++i) {
                 if (i > 1) s += " ";
                 s += c.args[i];
             }
             return s;
         }});
    reg({"quit", "Request clean shutdown.", "",
         [](const CmdContext&) -> std::string {
             PostQuitMessage(0);
             return "quitting";
         }});
    {
        std::lock_guard<std::mutex> lk(g_mux);
        LOG_INFO("console", "initialised (dev=%d), %zu base commands", (int)g_dev, g_cmds.size());
    }
}

void shutdown() {
    stop_stdin_thread();
    std::lock_guard<std::mutex> lk(g_mux);
    g_cmds.clear();
}

void reg(const Command& cmd) {
    std::lock_guard<std::mutex> lk(g_mux);
    // Normalise the registered name to lower case: exec_line lower-cases the user's input
    // before comparing, so a CamelCase name (logLevel, _abortNow) could never match. This
    // was found by actually running `-exec _abortNow`, which reported "unknown command"
    // instead of testing the abort path - an unexercised path is a hypothesis, not a feature.
    Command c = cmd;
    for (auto& ch : c.name) ch = (char)tolower((unsigned char)ch);
    for (auto& e : g_cmds) {
        if (e.name == c.name) {
            LOG_WARN("console", "command re-registered: %s", c.name.c_str());
            e = c;
            return;
        }
    }
    g_cmds.push_back(c);
}

const std::vector<Command>& commands() { return g_cmds; }

size_t count() {
    std::lock_guard<std::mutex> lk(g_mux);
    return g_cmds.size();
}

bool exec_line(const std::string& line, std::string& out_result) {
    std::string t = trim(line);
    if (t.empty() || t[0] == '#') return true;  // comments allowed for script files
    auto toks = split_args(t);
    if (toks.empty()) return true;
    CmdContext c;
    c.args = toks;
    for (auto& ch : c.args[0]) ch = (char)tolower((unsigned char)ch);
    // `name:arg` form, so the unattended harness never depends on shell quoting:
    //   -exec "_abortNow 9"      needs the shell to survive PowerShell -> CreateProcess
    //                            (it does not: the value arrived as '"_abortNow 9\')
    //   -exec _abortNow:9        needs nothing.
    // Splitting on the FIRST colon only when the prefix is a registered command keeps
    // `capture:D:\shots\x.bmp` working (prefix `capture` is a command, the rest is the path).
    {
        const size_t colon = c.args[0].find(':');
        if (colon != std::string::npos && colon > 0) {
            std::string head = c.args[0].substr(0, colon);
            bool known = false;
            {
                std::lock_guard<std::mutex> lk(g_mux);
                for (auto& cmd : g_cmds) {
                    if (cmd.name == head) {
                        known = true;
                        break;
                    }
                }
            }
            if (known) {
                std::string rest = c.args[0].substr(colon + 1);
                c.args.erase(c.args.begin());
                c.args.insert(c.args.begin(), head);
                // the remainder becomes argv[1]; existing argv shifts after it
                if (!rest.empty()) c.args.insert(c.args.begin() + 1, rest);
            }
        }
    }
    // Copy the handler out under the lock instead of keeping a Command*: a handler that
    // registers a new command would grow g_cmds and reallocate it, leaving a pointer into
    // freed memory. (Command execution is main-thread-only via poll(), so this is about
    // registration from other threads, not reentrancy.)
    std::function<std::string(const CmdContext&)> handler;
    std::string cmd_name;
    {
        std::lock_guard<std::mutex> lk(g_mux);
        for (auto& cmd : g_cmds) {
            if (cmd.name == c.args[0]) {
                handler = cmd.handler;
                cmd_name = cmd.name;
                break;
            }
        }
    }
    if (!handler) {
        out_result = "unknown command: " + c.args[0];
        LOG_WARN("console", "%s", out_result.c_str());
        return false;
    }
    if (!g_dev && !cmd_name.empty() && cmd_name[0] == '_') {
        out_result = "dev-only command in non-dev build: " + c.args[0];
        return false;
    }
    LOG_DEBUG("console", "exec: %s", t.c_str());
    out_result = handler(c);
    if (!out_result.empty()) LOG_INFO("console", "-> %s", out_result.c_str());
    return true;
}

void exec_file(const std::string& path) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "r") != 0 || !f) {
        LOG_ERROR("console", "sinclude: cannot open %s", path.c_str());
        return;
    }
    char buf[4096];
    size_t n = 0;
    while (fgets(buf, sizeof(buf), f)) {
        std::string line = trim(buf);
        if (line.empty()) continue;
        std::string r;
        exec_line(line, r);
        ++n;
    }
    fclose(f);
    LOG_INFO("console", "sinclude %s: %zu lines", path.c_str(), n);
}

void start_stdin_thread() {
    // Read stdin when it is a real console OR when it is a redirect/pipe. The second case
    // is what makes the app scriptable unattended:
    //   Start-Process ... -RedirectStandardInput cmds.txt
    // The first version only checked GetConsoleWindow(), so a piped script was silently
    // ignored and the console looked dead.
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    const UINT kind = in == INVALID_HANDLE_VALUE ? 0 : GetFileType(in);
    if (!GetConsoleWindow() && kind != FILE_TYPE_PIPE && kind != FILE_TYPE_DISK) {
        LOG_INFO("console", "stdin is neither a console nor a redirect (%u) - no reader thread", kind);
        return;
    }
    g_run.store(true);
    g_thread = std::thread(stdin_worker);
    LOG_INFO("console", "stdin reader thread started (kind=%u)", kind);
}

void stop_stdin_thread() {
    g_run.store(false);
    if (g_thread.joinable()) {
        // The worker blocks in fgets(); detach rather than deadlock on exit.
        g_thread.detach();
    }
}

void poll() {
    for (;;) {
        std::string line;
        {
            std::lock_guard<std::mutex> lk(g_qmux);
            if (g_queue.empty()) break;
            line = std::move(g_queue.front());
            g_queue.pop_front();
        }
        std::string r;
        exec_line(line, r);
        if (!r.empty()) {
            printf("> %s\n%s\n> ", line.c_str(), r.c_str());
            fflush(stdout);
        }
    }
}

}  // namespace os::console
