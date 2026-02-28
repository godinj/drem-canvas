#pragma once

#if defined(__linux__)

namespace dc
{
namespace platform
{
namespace x11
{

enum class ProbeMode
{
    dragUp,        // Press + drag 10px up + release (vertical knobs)
    dragRight,     // Press + drag 10px right + release (horizontal sliders)
    dragDown,      // Press + drag 10px down + release (inverted knobs)
    click          // Press + release at same position (buttons/toggles)
};

/**
 * Send a synthetic mouse interaction to an X11 window at the given position,
 * to trigger the plugin's performEdit callback.
 *
 * Uses XTest fake events so Wine/yabridge plugins see real server-level
 * input events (not flagged as synthetic). This briefly moves the cursor.
 *
 * @param display   X11 Display* (as void* to avoid header conflicts)
 * @param window    Target X11 Window (the plugin editor window)
 * @param x         X position in window-relative coordinates
 * @param y         Y position in window-relative coordinates
 * @param mode      Interaction strategy (drag direction or click)
 */
void sendMouseProbe (void* display, unsigned long window, int x, int y,
                     ProbeMode mode = ProbeMode::dragUp);

/**
 * Move an X11 window to the given position.
 * Used to temporarily bring an off-screen window on-screen for XTest probing.
 */
void moveWindow (void* display, unsigned long window, int x, int y);

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
