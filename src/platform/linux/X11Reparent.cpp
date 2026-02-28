#if defined(__linux__)

// This file deliberately does NOT include JuceHeader.h.
// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named JUCE classes.  Keeping all X11
// calls in this translation unit avoids the conflict entirely.

#include <X11/Xlib.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "X11Reparent.h"

namespace dc
{
namespace platform
{
namespace x11
{

bool isX11()
{
    return glfwGetPlatform() == GLFW_PLATFORM_X11;
}

void* getDisplay()
{
    if (! isX11())
        return nullptr;
    return static_cast<void*> (glfwGetX11Display());
}

void* openDisplay()
{
    return static_cast<void*> (XOpenDisplay (nullptr));
}

void closeDisplay (void* display)
{
    if (display != nullptr)
        XCloseDisplay (static_cast<Display*> (display));
}

unsigned long getWindow (GLFWwindow* glfwWin)
{
    if (! isX11())
        return 0;
    return static_cast<unsigned long> (glfwGetX11Window (glfwWin));
}

void reparent (void* display, unsigned long child, unsigned long parent, int x, int y)
{
    auto* d = static_cast<Display*> (display);
    XReparentWindow (d, static_cast<Window> (child),
                     static_cast<Window> (parent), x, y);
    XMapWindow (d, static_cast<Window> (child));
    XFlush (d);
}

void moveResize (void* display, unsigned long window, int x, int y, int w, int h)
{
    auto* d = static_cast<Display*> (display);
    XMoveResizeWindow (d, static_cast<Window> (window),
                       x, y,
                       static_cast<unsigned int> (w),
                       static_cast<unsigned int> (h));
    XFlush (d);
}

void getWindowPos (GLFWwindow* glfwWin, int& x, int& y)
{
    glfwGetWindowPos (glfwWin, &x, &y);
}

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
