#pragma once
// Command line + config-key vocabulary.
//
// Evidence (level 1, refs/bins/ReleaseGA/SporeApp.exe.unpacked.exe):
//   * The original looks up launch keys through one helper, 0x0092b570(this, name, ...),
//     returning an index or -1 when absent. Every call site pushes the name string
//     pointer, which makes the whole vocabulary mechanically recoverable: see
//     analysis/arg_vocab.md (76 call sites, 56 distinct names).
//   * Names carry NO leading dash in the binary ('devDirs', 'shipDirs', 'state',
//     'preload', 'safe', 'noVSync', ...). The dash is the shell-side convention; the
//     config layer strips it. We accept both forms.
//   * devDirs/shipDirs (0x007e7f27 / 0x007e8365): devDirs sets [obj+0x159] (useDevDirs),
//     shipDirs clears it, shipDirs is evaluated second so it wins when both are passed.
//   * state/preload (0x007e2235, 0x007e22a0): the startup state name + preload state.
//
// This class is OUR implementation of that contract, not a port of their code.

#include <string>
#include <vector>

#include "common.h"

namespace os {

struct Args {
    // raw
    std::vector<std::string> raw;          // every token, lowercased keys already split
    std::vector<std::pair<std::string, std::string>> kv;  // (key, value-or-"")

    // well-known flags (vocabulary from analysis/arg_vocab.md)
    bool windowed = true;        // -w  (PROMPT.md test arg; original's windowed default is verified below)
    bool dev_dirs = false;       // -devDirs
    bool ship_dirs = false;      // -shipDirs  (wins over devDirs, per 0x7e838e)
    bool no_dev = false;         // -noDev
    bool safe_mode = false;      // -safe
    bool no_sound = false;       // -noSound
    bool no_vsync = false;       // -noVSync
    bool no_plugins = false;     // -noPlugins
    bool no_scripts = false;     // -noScripts
    bool multiple_instances = false;  // -multipleInstances
    bool verbose = false;        // -verbose (ours; the original's analog is 'silent' inverted)
    bool silent = false;         // -silent
    std::string state;           // -state <name>       (startup app state)
    std::string preload;         // -preload <name>     (preloaded state)
    std::string locale;          // -locale <name>
    std::string data_dir;        // -dataDir <path>
    std::string user_data_dir;   // -userDataDir <path>
    std::string base_dir;        // -dataBaseDir <path>
    int width = 1280;            // -width / derived
    int height = 720;            // -height
    std::string log_level = "debug";  // -logLevel <fatal|error|warn|info|debug|trace>

    // -capture <path>: write one frame to a BMP from inside the device, then keep going.
    // Needed because PrintWindow cannot capture D3D9 content (it returns all-black), so
    // without this the rendered frame has no machine-checkable evidence at all.
    std::string capture_path;
    int capture_after = 30;  // -captureAfter <n> frames; 30 is enough to be past any reset

    // Unattended harness: run console commands and quit without a human or a pipe, so the
    // fault-tolerance path and the frame capture are TESTABLE rather than merely built.
    // -exec is ';'-separated; -execAfter is in frames, i.e. it fires inside the loop after
    // the renderer produced real frames - which is what makes it a test and not a mock.
    std::string exec_cmds;
    int exec_after = 5;
    int quit_after_frames = 0;  // -quitAfter <n>; 0 = never

    // -headless: no window, no D3D9 device, no font atlas. Console commands still run, so the
    // unpacker/inspector work on a machine with no display (or in CI). This is what makes the
    // standalone-unpacker wishlist item a real tool rather than a GUI side effect.
    bool headless = false;

    // Effective devDirs decision, exactly the original's precedence:
    // devDirs sets it, shipDirs afterwards clears it, so shipDirs wins.
    bool use_dev_dirs() const { return dev_dirs && !ship_dirs; }

    bool has(const std::string& key) const;
    std::string get(const std::string& key, const std::string& def = {}) const;
    // Every value given for a repeatable key (-exec, -capture), in command-line order.
    std::vector<std::string> all(const std::string& key) const;
};

// Parse a Win32 command line (or an explicit token list, used by tests).
Args parse_command_line(const wchar_t* wide_cmdline);
Args parse_tokens(const std::vector<std::string>& tokens);

std::string to_string(const Args& a);  // one-line summary for the log

}  // namespace os
