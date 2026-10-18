#pragma once
#include "states/statemgr.h"

namespace os::states {
// Debug-only controlled reconstruction: explicit typed catalogue/camera, real shipped atlas PNG,
// owned host scene and UI picking. It also owns the separate ScenarioHostDiagnostic handoff. Neither
// state is registered in release builds and neither is native GGE/ScenarioMode execution.
void install_gge_host_preview(Manager& manager);
}  // namespace os::states
