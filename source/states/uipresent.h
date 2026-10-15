#pragma once
// Presenting a `spui` layout AS A FRAME, driven by the state machine's own script.
//
// This exists so that "the state machine drives the UI" has ONE implementation. Before it, only the
// Title state could draw a layout, so a game state whose script says
// `cheat "uilayout -load E3LaunchScreen 0x027996d8 0x0279bf9e"` entered, logged that it could not run
// the cheat, and presented NOTHING - the game named a screen and we ignored it. The name hashes to
// the layout's resource instance (E3LaunchScreen -> 0xE932BD44, measured), and we can read those
// bytes, so this is now an instruction we EXECUTE rather than a gap we report.
//
// The layout's geometry, colours and captions are all the game's own: coordinates come from the
// `Area` rectf records (LEFT,TOP,RIGHT,BOTTOM, measured), colours are used AS STORED including their
// alpha (transparency is part of the data, not something to normalise away), and a caption is either
// inline UTF-16 from the layout or a (table,key) pair resolved through engine/text.cpp.
#include <string>

namespace os::spui {
struct Layout;
}  // namespace os::spui

struct IDirect3DTexture9;

namespace os::states {

// The layout the CURRENT state's script asks for, or a recovered default primary mode UI subset.
// Returns null on missing/unsupported bindings. Sets `why` to a sentence explaining the
// outcome, for the frame and the log - "state 'DevLauncher' asked for 'E3LaunchScreen'" is a
// different claim about progress than "state 'X' names no layout".
const os::spui::Layout* requested_by_state(std::string& why);

// Draws `lay` scaled to the viewport over an optional full-screen background, presenting a complete
// frame. Returns the number of windows drawn, or -1 when there is no D3D device or the frame could
// not be begun (being headless is not an error).
int present_spui_layout(const os::spui::Layout& lay, const std::string& why,
                        IDirect3DTexture9* bg, int bg_w, int bg_h);

// Presents the current state's screen. With `fall_back` set, a state that asked for nothing (or for
// something unreadable) shows the largest readable layout instead - which is OUR choice of screen, so
// the caller is told which of the two happened via the frame's own caption. Without it, a state that
// asked for nothing readable presents nothing and this returns false, so the caller can show the gap
// rather than silently substitute a screen the game never asked for.
// Returns true when a frame was presented.
bool present_ui_screen(bool fall_back, IDirect3DTexture9* bg = nullptr, int bg_w = 0, int bg_h = 0);

}  // namespace os::states
