#pragma once
#include "states/statemgr.h"

namespace os::states {
// Debug-only controlled reconstruction: explicit typed catalogue/camera, real shipped atlas PNG,
// owned host scene and UI picking. It is not registered in release builds and is not native GGE.
void install_gge_host_preview(Manager& manager);
}  // namespace os::states
