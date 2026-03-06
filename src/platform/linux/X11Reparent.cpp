#if defined(__linux__)

// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named GUI framework classes.  Keeping all X11
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

unsigned long createWindow (void* display, int width, int height)
{
    auto* d = static_cast<Display*> (display);
    Window root = DefaultRootWindow (d);

    // override_redirect prevents the window manager from decorating,
    // managing, or moving the window on-screen.
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;

    Window win = XCreateWindow (d, root, -10000, -10000,
                                static_cast<unsigned int> (width),
                                static_cast<unsigned int> (height),
                                0, CopyFromParent, InputOutput,
                                CopyFromParent, CWOverrideRedirect, &attrs);
    XMapWindow (d, win);
    XFlush (d);
    return static_cast<unsigned long> (win);
}

void destroyWindow (void* display, unsigned long window)
{
    if (display != nullptr && window != 0)
    {
        auto* d = static_cast<Display*> (display);
        XDestroyWindow (d, static_cast<Window> (window));
        XFlush (d);
    }
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

float getContentScale (GLFWwindow* glfwWin)
{
    float xScale = 1.0f, yScale = 1.0f;
    glfwGetWindowContentScale (glfwWin, &xScale, &yScale);
    return xScale;
}

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
