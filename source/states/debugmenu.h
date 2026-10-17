#pragma once

namespace os::states { class Manager; }

namespace os::states::debugmenu {
void toggle();
bool open();
bool handle_keydown(unsigned key, long repeat_flags);
bool handle_char(wchar_t ch);
void install(Manager& manager);
} // namespace os::states::debugmenu
