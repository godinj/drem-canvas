#pragma once

#if defined(__linux__)

struct GLFWwindow;

namespace dc
{
namespace platform
{
namespace x11
{

/** Returns true if GLFW is running on X11 (false on Wayland). */
bool isX11();

/** Get the X11 Display* from GLFW (returned as void* to avoid X11 header). */
void* getDisplay();

/** Open an independent X11 display connection (for use on Wayland/XWayland).
    Caller is responsible for closing via closeDisplay(). */
void* openDisplay();

/** Close an X11 display connection opened via openDisplay(). */
void closeDisplay (void* display);

/** Get the X11 Window from a GLFWwindow (returned as unsigned long). */
unsigned long getWindow (GLFWwindow* glfwWin);

/** Reparent child into parent at (x, y) and map it. */
void reparent (void* display, unsigned long child, unsigned long parent, int x, int y);

/** Move and resize an X11 window. */
void moveResize (void* display, unsigned long window, int x, int y, int w, int h);

/** Get the screen position of a GLFW window (works on both X11 and Wayland). */
void getWindowPos (GLFWwindow* glfwWin, int& x, int& y);

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
