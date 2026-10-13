#include "engine/cmdline.h"

#include <algorithm>
#include <cctype>

namespace os {
namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string strip_dash(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == '-' || s[i] == '/')) ++i;
    return s.substr(i);
}

bool ci_eq(const std::string& a, const std::string& b) { return lower(a) == lower(b); }

}  // namespace

bool Args::has(const std::string& key) const {
    for (auto& [k, v] : kv) {
        (void)v;
        if (ci_eq(k, key)) return true;
    }
    return false;
}

std::string Args::get(const std::string& key, const std::string& def) const {
    for (auto& [k, v] : kv) {
        if (ci_eq(k, key)) return v.empty() ? def : v;
    }
    return def;
}

std::vector<std::string> Args::all(const std::string& key) const {
    std::vector<std::string> out;
    for (auto& [k, v] : kv) {
        if (ci_eq(k, key) && !v.empty()) out.push_back(v);
    }
    return out;
}

Args parse_tokens(const std::vector<std::string>& tokens) {
    Args a;
    for (size_t i = 0; i < tokens.size(); ++i) {
        const std::string& tok = tokens[i];
        if (tok.empty()) continue;
        if (tok[0] != '-' && tok[0] != '/') {
            a.raw.push_back(tok);  // bare token: keep for positional use (e.g. legacy -w style)
            continue;
        }
        std::string body = strip_dash(tok);
        std::string key, val;
        // The original uses a COLON for key:value (e.g. `-state:SpaceAccessoriesEditor`); we also
        // accept `=`. Split on whichever comes FIRST, so a Windows path in the value
        // (`-exec:pkg:-unpack,all,C:\dump`) keeps its colons.
        size_t eq = body.find('=');
        size_t co = body.find(':');
        size_t sep = std::string::npos;
        if (eq != std::string::npos && co != std::string::npos) sep = std::min(eq, co);
        else if (eq != std::string::npos) sep = eq;
        else sep = co;
        if (sep != std::string::npos) {
            key = body.substr(0, sep);
            val = body.substr(sep + 1);
        } else {
            key = body;
            // "-state Loader" style: the next token is the value iff it is not another flag
            if (i + 1 < tokens.size() && !tokens[i + 1].empty() && tokens[i + 1][0] != '-' && tokens[i + 1][0] != '/') {
                const std::string& nx = tokens[i + 1];
                static const char* value_keys[] = {"state",  "preload",  "locale",     "dataDir",   "userDataDir",
                                                   "width",  "height",   "logLevel",   "dataBaseDir",
                                                   "capture", "captureAfter", "exec", "execAfter", "quitAfter",
                                                   "r"};  // -r <WxH>, the original's resolution flag
                for (const char* vk : value_keys) {
                    if (ci_eq(key, vk)) {
                        val = nx;
                        ++i;
                        break;
                    }
                }
            }
        }
        a.kv.emplace_back(lower(key), val);
        a.raw.push_back(tok);
    }

    // Well-known mapping. Keys are matched case-insensitively because the original's
    // vocabulary is camelCase but Windows shells mangle case habits.
    //
    // Windowed/fullscreen/resolution are the original's own short flags, read straight from
    // the device-creation function at 0x7e74a0..0x7e7637: it looks up "f" (0x1413ab0) and
    // "w" (0x1413ab4) through the arg helper and sets a windowed byte (f -> 0, w -> 1), and
    // parses "r" (0x1413ac4) with the format "%dx%d" (0x1413ab8). f is tested first, so w
    // wins if both are present. Our loader defaults to windowed (a deliberate divergence:
    // the original's default comes from the `screenWindowed` device metric, which we do not
    // read yet).
    a.windowed = true;
    if (a.has("f") || a.has("fullscreen")) a.windowed = false;
    if (a.has("w") || a.has("window") || a.has("windowed")) a.windowed = true;
    if (a.has("r")) {
        const std::string res = a.get("r");
        size_t x = res.find_first_of("xX");
        if (x != std::string::npos) {
            int w = atoi(res.substr(0, x).c_str());
            int h = atoi(res.substr(x + 1).c_str());
            if (w > 0 && h > 0) { a.width = w; a.height = h; }
        }
    }
    a.dev_dirs = a.has("devdirs");
    a.ship_dirs = a.has("shipdirs");
    a.no_dev = a.has("nodev");
    a.safe_mode = a.has("safe");
    a.no_sound = a.has("nosound");
    a.no_vsync = a.has("novsync");
    a.no_plugins = a.has("noplugins");
    a.no_scripts = a.has("noscripts");
    a.multiple_instances = a.has("multipleinstances");
    a.silent = a.has("silent");
    a.verbose = a.has("verbose");
    a.state = a.get("state");
    a.preload = a.get("preload");
    a.locale = a.get("locale", "en-US");
    a.data_dir = a.get("datadir");
    a.user_data_dir = a.get("userdatadir");
    a.base_dir = a.get("databasedir");
    if (a.has("width")) a.width = atoi(a.get("width").c_str());
    if (a.has("height")) a.height = atoi(a.get("height").c_str());
    a.log_level = lower(a.get("loglevel", a.log_level));
    // NOTE: devDirs/shipDirs are PRESENCE flags in the original (the helper's return is
    // compared to -1, never read as a value), so they must not appear in value_keys above.
    a.capture_path = a.get("capture");
    if (a.has("captureafter")) a.capture_after = atoi(a.get("captureafter").c_str());
    if (a.capture_after < 1) a.capture_after = 1;
    // Repeatable keys: every -exec accumulates (in order), so a harness can script
    // `_abortNow:1;state -list` across several flags instead of one giant quoted string.
    for (const auto& e : a.all("exec")) {
        if (!a.exec_cmds.empty()) a.exec_cmds += ";";
        a.exec_cmds += e;
    }
    if (a.has("execafter")) a.exec_after = atoi(a.get("execafter").c_str());
    if (a.exec_after < 1) a.exec_after = 1;
    if (a.has("quitafter")) a.quit_after_frames = atoi(a.get("quitafter").c_str());
    a.headless = a.has("headless") || a.has("noWindow") || a.has("nowindow");
    if (a.width <= 0) a.width = 1280;
    if (a.height <= 0) a.height = 720;
    return a;
}

Args parse_command_line(const wchar_t* wide_cmdline, bool has_program_name) {
    // CommandLineToArgvW semantics done by hand to stay dependency-light:
    // backslash-aware quote handling, same rules as MSVC's argv.
    std::vector<std::string> tokens;
    std::string cur;
    bool in_q = false;
    bool has_cur = false;
    int bslashes = 0;
    std::string s;
    for (const wchar_t* p = wide_cmdline; *p; ++p) {
        char c = (*p < 128) ? (char)*p : '?';
        if (c == '\\' && !in_q) {
            // count runs, then decide on quote
            int n = 0;
            const wchar_t* q = p;
            while (*q == L'\\') { ++n; ++q; }
            if (*q == L'"') {
                for (int i = 0; i < n / 2; ++i) cur += '\\';
                if (n % 2 == 0) {
                    in_q = !in_q;
                    p = q;  // consumed the quote
                    --p;    // loop will ++
                    has_cur = true;
                    continue;
                }
                cur += '"';
                p = q;
                --p;
                has_cur = true;
                continue;
            }
            for (int i = 0; i < n; ++i) cur += '\\';
            p = q - 1;
            has_cur = true;
            continue;
        }
        if (c == '"') {
            in_q = !in_q;
            has_cur = true;
            continue;
        }
        if (!in_q && (c == ' ' || c == '\t')) {
            if (has_cur) {
                tokens.push_back(cur);
                cur.clear();
                has_cur = false;
            }
            continue;
        }
        cur += c;
        has_cur = true;
    }
    if (has_cur) tokens.push_back(cur);
    // Drop argv[0] ONLY when the string actually carries it (GetCommandLineW does; lpCmdLine
    // does not). Erasing unconditionally dropped the first real argument.
    if (has_program_name && !tokens.empty()) tokens.erase(tokens.begin());
    (void)bslashes;
    (void)s;
    return parse_tokens(tokens);
}

std::string to_string(const Args& a) {
    std::string s = "cmdline:";
    for (auto& [k, v] : a.kv) s += " -" + k + (v.empty() ? "" : ("=" + v));
    if (!a.raw.empty()) {
        s += " raw:";
        for (auto& r : a.raw) s += " " + r;
    }
    return s;
}

}  // namespace os
