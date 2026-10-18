#include "engine/input.h"

#include <cstdio>

namespace os::input {
namespace {

// The raw, message-thread view: what Win32 told us since the last frame.
int g_x = 0, g_y = 0;
bool g_left = false;
bool g_press_left = false;    // accumulated since the last begin_frame()
bool g_press_right = false;
int g_wheel = 0;             // accumulated wheel delta since last end_frame

// The frame-consistent view: latched at begin_frame(), so a click is visible for exactly one frame
// no matter how many messages arrived while it was being drawn.
int g_fx = 0, g_fy = 0;
bool g_fclicked = false, g_fright = false;

}  // namespace

void on_move(int x, int y) {
    g_x = x;
    g_y = y;
}

void on_wheel(int delta) {
    g_wheel += delta;
}

void on_button(bool left, bool down) {
    if (down) {
        // Edge-detect on the PRESS, and record the position at press time: a drag that ends outside a
        // widget still clicked the widget it started on, which is how the game's own buttons behave
        // (and the difference between a click and a marquee).
        if (left) {
            if (!g_left) g_press_left = true;
            g_left = true;
        } else {
            g_press_right = true;
        }
    } else if (left) {
        g_left = false;
    }
}

void begin_frame() {
    g_fx = g_x;
    g_fy = g_y;
    g_fclicked = g_press_left;
    g_fright = g_press_right;
    g_press_left = g_press_right = false;
}

void end_frame() {
    // Clear the wheel delta so it is visible for exactly one frame.
    g_wheel = 0;
}

int x() { return g_fx; }
int y() { return g_fy; }
int wheel() { return g_wheel; }
bool left_down() { return g_left; }
bool clicked() { return g_fclicked; }
bool right_clicked() { return g_fright; }

void inject_click(int cx, int cy) {
    // Feed the SAME path a real click does: set the accumulated press and let the FRAME LOOP latch it
    // on its next begin_frame(). Do NOT call begin_frame() here - the console verb runs after the
    // loop has already latched this frame, so latching here would be overwritten by the loop's own
    // begin_frame() on the next iteration and the click would vanish. That is exactly the bug this
    // comment exists to prevent: an injected click that no test can see, because the injection path
    // and the live path disagree about who owns the latch.
    g_x = cx;
    g_y = cy;
    g_left = false;           // so the press below edge-detects as a fresh press
    g_press_left = true;
    g_press_right = false;
}

char* describe(char* buf, size_t cap) {
    std::snprintf(buf, cap, "input: ptr (%d,%d) left=%d clicked=%d right_clicked=%d",
                  g_fx, g_fy, g_left ? 1 : 0, g_fclicked ? 1 : 0, g_fright ? 1 : 0);
    return buf;
}

}  // namespace os::input
