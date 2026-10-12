#pragma once
// Application boot chain and frame loop.
//
// Order is deliberate: observation infrastructure BEFORE any engine code (field guide §8.5
// and §4). If the renderer fails we must already have a log file, a crash handler and a
// console, otherwise the only evidence is a window that closed.
#include <windows.h>

namespace os::app {

int run(HINSTANCE inst, HINSTANCE prev, LPWSTR cmdline, int show);

// Requested by console commands / the window close button.
void request_quit(int code);
bool quitting();

}  // namespace os::app
