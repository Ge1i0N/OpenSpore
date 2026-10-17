#pragma once

namespace os::states { class Manager; }

namespace os::states::debugmenu {
void toggle();
bool open();
void install(Manager& manager);
} // namespace os::states::debugmenu
