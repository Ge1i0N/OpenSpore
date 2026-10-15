#include "engine/gge_dbcheck.h"
#include "engine/gge_intro.h"
#include "engine/spui.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

using namespace os;
using namespace os::gge;

struct Backend final : DbCheckBackend {
    bool load_ok = true, root = true;
    bool bad_descriptor = false;
    IntroGate* gate = nullptr;
    DbCheckSession* notify_session = nullptr;
    DbCheckSession* callback_on_attach = nullptr;
    DbCheckSession* callback_on_detach = nullptr;
    bool duplicate_start_accepted = false;
    bool disappear_after_first_find = false;
    unsigned finds = 0;
    std::vector<std::string> calls;
    bool load_ui(const DbCheckUi& ui) override {
        bad_descriptor = ui.instance != 0x4b0ac091u || ui.request_tag != 0x0510a95bu ||
                         ui.group != 0x40464100u || ui.mount != 0x05b598f7u ||
                         ui.root_control != 0x0431e538u;
        calls.push_back("load"); return load_ok;
    }
    bool find_root(u32 id) override {
        bad_descriptor |= id != 0x0431e538u;
        calls.push_back("find");
        ++finds;
        return root && (!disappear_after_first_find || finds == 1);
    }
    void attach_dialog_proc(u32) override {
        calls.push_back("proc");
        if (callback_on_attach) callback_on_attach->on_ui_callback(ContentCheck::Unknown,0,*this);
    }
    void attach_listener(u32) override { calls.push_back("listener"); }
    void manager_continue() override { calls.push_back("manager+20"); }
    void notify(u32 id) override {
        bad_descriptor |= id != kDbCheckNotification;
        calls.push_back("notify"); if (gate) gate->notify(id);
        if (notify_session) duplicate_start_accepted |= notify_session->start(ContentCheck::Absent,*this);
    }
    void dispatch_callback(u32, u32 control) override { calls.push_back("dispatch:"+std::to_string(control)); }
    void detach_dialog_proc(u32) override {
        calls.push_back("detach");
        if (callback_on_detach) callback_on_detach->on_ui_callback(ContentCheck::NeedsDialog,0,*this);
    }
    void unload_ui() override { calls.push_back("unload"); }
};

static bool corpus_check(const std::filesystem::path& dir) {
    std::ifstream manifest(dir/"manifest.txt");
    std::string file;
    size_t size, objects, windows, props, links, roots, matched = 0;
    while (manifest >> file >> size >> objects >> windows >> props >> links >> roots) {
        if (file.find("4b0ac091") == std::string::npos) continue;
        std::ifstream input(dir/file, std::ios::binary);
        std::vector<u8> data(std::istreambuf_iterator<char>(input), {});
        spui::Layout layout;
        std::string error;
        if (!spui::parse_binary(data,layout,error) || data.size()!=12173 || objects!=101 || windows!=24)
            return false;
        bool root = false;
        for (const auto& win : layout.windows) if (win.control_id == DbCheckUi{}.root_control) root = true;
        if (!root) return false;
        ++matched;
    }
    return matched != 0;
}

int main(int argc, char** argv) {
    static_assert(!std::is_copy_constructible_v<DbCheckSession>);
    if (argc != 2) { std::cerr << "usage: gge_dbcheck_verify <SPUI-fixture-dir>\n"; return 2; }
    const auto check = [](bool ok, const char* why) {
        if (!ok) std::cerr << "DBCheck regression: " << why << '\n';
        return ok;
    };
    if (!check(corpus_check(argv[1]),"real dialog fixture/root")) return 1;
    Backend b;
    DbCheckSession s;
    if (!check(!s.start(ContentCheck::Unknown,b) && b.calls.empty() &&
               s.state()==DbCheckState::DependencyBlocked,"unknown != absent")) return 1;
    if (!check(s.start(ContentCheck::Absent,b) && b.calls==std::vector<std::string>{"notify"},
               "original absent-manager path")) return 1;
    if (!check(!s.start(ContentCheck::Bypass,b) && b.calls.size()==1,"duplicate start")) return 1;
    s.cancel(b); b.calls.clear();
    s.start(ContentCheck::Bypass,b);
    if (!check(b.calls==std::vector<std::string>{"notify"},"manager bypass")) return 1;
    s.cancel(b); b.calls.clear(); b.load_ok=false;
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(b.calls==std::vector<std::string>{"load","notify"},"layout failure notification")) return 1;
    s.cancel(b); b.calls.clear(); b.load_ok=true; b.root=false;
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(b.calls==std::vector<std::string>{"load","find","unload","notify"},"root failure cleanup")) return 1;
    s.cancel(b); b.calls.clear(); b.root=true;
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(b.calls==std::vector<std::string>{"load","find","proc","find","listener"} &&
               s.state()==DbCheckState::Dialog && !b.bad_descriptor,"dialog start order")) return 1;
    b.calls.clear();
    if (!check(s.on_ui_callback(ContentCheck::Unknown,0,b) && s.callback_armed() && b.calls.empty(),
               "first callback must only arm")) return 1;
    if (!check(!s.on_ui_callback(ContentCheck::Unknown,0,b) &&
               !s.on_ui_callback(ContentCheck::Absent,0,b) && b.calls.empty(),"unavailable manager guard")) return 1;
    if (!check(s.on_ui_callback(ContentCheck::NeedsDialog,123,b) && b.calls==std::vector<std::string>{
        "manager+20","notify","find","dispatch:123","detach","unload"},"callback completion order")) return 1;
    b.calls.clear();
    if (!check(!s.on_ui_callback(ContentCheck::NeedsDialog,123,b) && b.calls.empty(),"duplicate terminal callback")) return 1;
    s.cancel(b); b.calls.clear(); s.start(ContentCheck::NeedsDialog,b); b.calls.clear(); s.cancel(b);
    if (!check(b.calls==std::vector<std::string>{"find","detach","unload"} &&
               s.state()==DbCheckState::Inactive && !s.callback_armed(),"cancel emits no continuation")) return 1;
    b.calls.clear(); b.finds=0; b.disappear_after_first_find=true;
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(b.calls==std::vector<std::string>{"load","find","proc","find"} &&
               s.state()==DbCheckState::Dialog,"root missing at listener lookup")) return 1;
    b.calls.clear(); s.on_ui_callback(ContentCheck::NeedsDialog,0,b);
    s.on_ui_callback(ContentCheck::Bypass,0,b);
    if (!check(b.calls==std::vector<std::string>{"manager+20","notify","find","unload"},
               "callback root vanished; cleanup still required")) return 1;
    s.cancel(b); b.calls.clear(); b.disappear_after_first_find=false;
    IntroGate gate;
    b.calls.clear(); b.gate=&gate; gate.enter();
    if (!check(gate.update(6,MovieObservation::Idle)==IntroAction::StartDbCheck,"intro start")) return 1;
    s.start(ContentCheck::Absent,b);
    if (!check(gate.notification_seen() && !gate.post_intro_requested() &&
               gate.update(6,MovieObservation::Idle)==IntroAction::RunPostIntro,"sync notification next-update ordering")) return 1;
    s.cancel(b); gate.leave(); gate.enter(); b.calls.clear();
    gate.update(6,MovieObservation::Idle);
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(gate.update(6,MovieObservation::Idle)==IntroAction::None && !gate.notification_seen(),
               "dialog start fabricated notification")) return 1;
    s.on_ui_callback(ContentCheck::NeedsDialog,0,b);
    if (!check(!gate.notification_seen(),"first callback fabricated notification")) return 1;
    s.on_ui_callback(ContentCheck::NeedsDialog,0,b);
    if (!check(gate.update(6,MovieObservation::Idle)==IntroAction::RunPostIntro,
               "dialog callback not integrated with intro gate")) return 1;
    s.cancel(b); b.gate=nullptr; b.calls.clear(); b.callback_on_attach=&s;
    s.start(ContentCheck::NeedsDialog,b);
    if (!check(s.callback_armed() && b.calls==std::vector<std::string>{"load","find","proc","find","listener"},
               "synchronous attach callback latch lost")) return 1;
    b.callback_on_attach=nullptr; b.notify_session=&s; b.calls.clear();
    s.on_ui_callback(ContentCheck::NeedsDialog,9,b);
    if (!check(!b.duplicate_start_accepted && b.calls==std::vector<std::string>{
        "manager+20","notify","find","dispatch:9","detach","unload"},
        "synchronous notification restarted completed request")) return 1;
    b.notify_session=nullptr;
    s.cancel(b); b.calls.clear(); s.start(ContentCheck::NeedsDialog,b);
    s.on_ui_callback(ContentCheck::NeedsDialog,0,b); // arm so a teardown callback would be dangerous
    b.callback_on_detach=&s; b.calls.clear(); s.cancel(b);
    if (!check(b.calls==std::vector<std::string>{"find","detach","unload"},
               "teardown callback produced a spurious continuation")) return 1;
    b.callback_on_detach=nullptr;
    std::cout << "DBCheck branches/callback ordering/cancel + real 101-object dialog fixture + Intro integration PASS\n";
    std::cout << "Controlled ports only; no live content manager, MoviePlayer or full UI WinProc claimed\n";
    return 0;
}
