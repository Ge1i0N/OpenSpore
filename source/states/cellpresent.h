#pragma once
// Cell-stage presenter: draws the live cell world (engine/cell_world.h) with the immediate-mode
// overlay, and steers the player from the pointer.
//
// This is the render/input half of the cell work. It reads the world the stage driver ticks
// (engine/game_stages.cpp) and is deliberately thin: camera, colours and the HUD live here; the
// simulation lives in cell_world.cpp so it stays testable without a device.
//
// Our camera, colours and HUD are seeds, not the original's renderer (the RenderWare cell path is
// not reconstructed). What IS the game's: the level's spawn list (which archetypes, how many) and
// each cell's body structure. The on-screen discs stand in for the meshes we cannot yet draw.
#include "common.h"

namespace os::states {

// Present one frame of the cell world. Always opens and closes a device frame when a device
// exists (so a deferred -capture is serviced even with no data); returns false only when there
// is no device (the caller's placeholder is then its own job). Steering: the player is pointed at
// the pointer position converted to world space.
bool present_cell_world();

}  // namespace os::states
