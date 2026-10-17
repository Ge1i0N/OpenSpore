#include "states/debugmenu.h"

#ifdef OS_ENABLE_DEV_COMMANDS
#include "engine/input.h"
#include "engine/log.h"
#include "render/device.h"
#include "render/overlay.h"
#include "states/statemgr.h"
#include <array>
#include <string>

namespace os::states::debugmenu {
namespace {
constexpr const char* kName = "OpenSporeDebugMenu";
Manager* g_manager = nullptr;
std::string g_previous;
bool g_open = false;
struct Item { const char* label; int action; };
constexpr std::array<Item, 6> kItems{{
    {"Return to previous state", 0}, {"Open GGEHostPreview (controlled)", 1},
    {"State: next", 2}, {"State: previous", 3}, {"Reload current state", 4}, {"Quit OpenSpore", 5},
}};
void close_menu() {
    if (!g_manager || !g_open) return;
    const std::string previous = g_previous; g_open = false; g_previous.clear();
    if (!previous.empty()) g_manager->request_goto(previous);
}
void choose(int action) {
    if (!g_manager) return;
    switch (action) { case 0: close_menu(); break;
    case 1: g_open=false; g_previous.clear(); g_manager->request_goto("GGEHostPreview"); break;
    case 2: g_open=false; g_previous.clear(); g_manager->next(); break;
    case 3: g_open=false; g_previous.clear(); g_manager->prev(); break;
    case 4: g_manager->reload(); break;
    case 5: PostQuitMessage(0); break; default: break; }
}
void enter() { g_open = true; LOG_INFO("debug-menu", "opened previous=%s", g_previous.c_str()); }
void exit() { g_open=false; g_previous.clear(); LOG_INFO("debug-menu", "closed"); }
void tick(f32) {
    if (!g_open || !g_manager || !os::input::clicked()) return;
    const int x=os::input::x(), y=os::input::y(), top=150, row=34;
    if (x<150 || x>760 || y<top || y>=top+(int)kItems.size()*row) return;
    choose((y-top)/row);
}
void render() {
    auto& device=os::render::device(); if(!device.d3d()||!device.begin_frame())return;
    device.clear(0xff080d16); auto& overlay=os::render::overlay(); overlay.begin(device.width(),device.height());
    const float left=150.f,width=610.f,top=92.f,row=34.f;
    overlay.rect(left-18.f,top-34.f,width+36.f,330.f,0xee101c2c);
    overlay.text(left,top,0xffffe3a0,"OpenSpore DEBUG MENU");
    overlay.text(left,top+23.f,0xff9fb4c4,"Ctrl+Shift+C toggles this menu | Esc returns");
    for(size_t i=0;i<kItems.size();++i){const float y=150.f+(float)i*row;const bool hot=os::input::x()>=(int)left&&os::input::x()<(int)(left+width)&&os::input::y()>=(int)y&&os::input::y()<(int)(y+row-2.f);overlay.rect(left,y,width,row-2.f,hot?0xff254866:0xff152638);overlay.text(left+14.f,y+8.f,hot?0xffffffff:0xffd4e2ec,kItems[i].label);}
    overlay.text_fmt(left,382.f,0xff8197ae,"Current before menu: %s | state id 0x%08X",g_previous.empty()?"<none>":g_previous.c_str(),g_manager->current_id());
    overlay.text(left,408.f,0xff8197ae,"OpenSpore developer tool; native EA Cheat Menu is not claimed.");
    overlay.end(); device.end_frame();
}
} // namespace
void toggle(){if(!g_manager)return;if(g_open){close_menu();return;}const std::string current=g_manager->current_name();if(current.empty()||current==kName)return;g_previous=current;g_manager->request_goto(kName);}
bool open(){return g_open;}
void install(Manager& manager){g_manager=&manager;State state;state.name=kName;state.on_enter=enter;state.on_exit=exit;state.on_tick=tick;state.on_render=render;if(manager.reg(std::move(state)))LOG_INFO("states","registered dev-only OpenSporeDebugMenu");}
} // namespace os::states::debugmenu
#else
namespace os::states::debugmenu { void toggle(){} bool open(){return false;} void install(Manager&){} }
#endif
