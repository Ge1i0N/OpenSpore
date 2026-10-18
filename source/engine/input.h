#pragma once
// Input state: where the pointer is, and what was clicked THIS frame.
//
// This is the seam that turns the menu from a picture into a menu. Until now the engine read the
// game's UI layouts, resolved the game's strings, and let the state machine pick the screen - but
// nothing could ACT on a widget, so the chain stopped one step short of playable. The layouts already
// carry, per window, both the `Area` rect (LEFT,TOP,RIGHT,BOTTOM, measured) and a `command_id`; what
// was missing was the pointer.
//
// WHY A SEPARATE MODULE RATHER THAN READING THE WIN32 MESSAGES IN THE RENDERER
// The window procedure runs on the message-pump thread and the UI is drawn in the frame loop. Latching
// input once per frame gives the renderer a stable, edge-triggered view (a click is true for exactly
// one frame) instead of a racing count. It also makes the whole thing TESTABLE WITHOUT A MOUSE:
// `inject_click()` feeds the same path a human click does, so `-exec ui:-click,<x>,<y>` can prove the
// hit-test chain in a headless run on any machine. A UI that can only be exercised by hand is a UI
// that cannot be regression-tested, and this project's yardsticks are all machine-checked.
//
// COORDINATES are CLIENT coordinates (the window's own pixels, 0,0 at the top-left of the client
// area), which is what the layout's scaled Areas are compared against. Not screen coordinates.
#include <cstddef>

namespace os::input {

// Called from the window procedure. Safe to call before init(); state is simply latched.
void on_move(int x, int y);
void on_button(bool left, bool down);
void on_wheel(int delta);

// Frame boundaries. begin_frame() latches the accumulated button edges so a click that arrived mid-
// frame is visible for exactly one frame; end_frame() clears what the frame did not consume.
void begin_frame();
void end_frame();

int x();
int y();
int wheel();                    // accumulated wheel delta since last end_frame
bool left_down();               // level: the button is currently held
bool clicked();                 // edge: a left press completed during THIS frame
bool right_clicked();

// Feed a synthetic click, exactly as if the user had pressed and released the left button at (x,y).
// Used by the console verb so the chain is testable unattended.
void inject_click(int cx, int cy);

// One-line summary for the log and the console verb.
char* describe(char* buf, size_t cap);

}  // namespace os::input
