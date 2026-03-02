// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named GUI framework classes.

#if defined(__linux__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>

#include "X11SyntheticMouseDrag.h"
#include "X11PluginEditorBridge.h"

#include <thread>
#include <chrono>

namespace dc
{

bool X11SyntheticMouseDrag::beginDrag (PluginEditorBridge& bridge, int x, int y)
{
    auto* x11Bridge = dynamic_cast<X11PluginEditorBridge*> (&bridge);
    if (x11Bridge == nullptr)
        return false;

    xDisplay = x11Bridge->getXDisplay();
    xWindow = x11Bridge->getXWindow();

    if (xDisplay == nullptr || xWindow == 0)
        return false;

    auto* dpy = static_cast<Display*> (xDisplay);
    auto win = static_cast<Window> (xWindow);

    // Check XTest extension
    int xtestEvent, xtestError, xtestMajor, xtestMinor;
    if (! XTestQueryExtension (dpy, &xtestEvent, &xtestError,
                                &xtestMajor, &xtestMinor))
        return false;

    // Save cursor position for restoration in endDrag
    {
        Window rootRet, childRet;
        int winX, winY;
        unsigned int maskRet;
        XQueryPointer (dpy, DefaultRootWindow (dpy), &rootRet, &childRet,
                       &savedCursorX, &savedCursorY, &winX, &winY, &maskRet);
    }

    // If the editor window is off-screen (e.g. hidden at -10000,-10000 for
    // compositor capture), move it on-screen so XTest root coordinates are
    // valid. Set opacity to 0 first so the Wayland compositor doesn't
    // display the window surface while it's at (0,0).
    movedWindow = false;
    {
        XWindowAttributes attrs;
        XGetWindowAttributes (dpy, win, &attrs);
        if (attrs.x < -1000 || attrs.y < -1000)
        {
            movedWindow = true;
            savedWindowX = attrs.x;
            savedWindowY = attrs.y;

            // Fully transparent — invisible to compositor
            Atom opacityAtom = XInternAtom (dpy, "_NET_WM_WINDOW_OPACITY", False);
            unsigned long zero = 0;
            XChangeProperty (dpy, win, opacityAtom, XA_CARDINAL, 32,
                             PropModeReplace,
                             reinterpret_cast<unsigned char*> (&zero), 1);

            XMoveWindow (dpy, win, 0, 0);
            XFlush (dpy);
            std::this_thread::sleep_for (std::chrono::milliseconds (50));
        }
    }

    // Convert native plugin coords to root-relative for XTest
    Window child;
    int rootX = 0, rootY = 0;
    XTranslateCoordinates (dpy, win, DefaultRootWindow (dpy),
                            x, y, &rootX, &rootY, &child);

    originRootX = rootX;
    originRootY = rootY;
    originLocalX = x;
    originLocalY = y;

    int screen = DefaultScreen (dpy);

    // Warp cursor to centroid and press — begins a continuous drag session
    XTestFakeMotionEvent (dpy, screen, rootX, rootY, 0);
    XFlush (dpy);
    std::this_thread::sleep_for (std::chrono::milliseconds (5));

    XTestFakeButtonEvent (dpy, 1, True, 0);
    XFlush (dpy);

    active = true;
    return true;
}

void X11SyntheticMouseDrag::moveDrag (int x, int y)
{
    if (! active || xDisplay == nullptr)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);

    // Compute root coords from the delta relative to drag origin
    int rootX = originRootX + (x - originLocalX);
    int rootY = originRootY + (y - originLocalY);

    XTestFakeMotionEvent (dpy, DefaultScreen (dpy), rootX, rootY, 0);
    XFlush (dpy);
}

void X11SyntheticMouseDrag::endDrag (int x, int y)
{
    if (! active || xDisplay == nullptr)
    {
        active = false;
        return;
    }

    auto* dpy = static_cast<Display*> (xDisplay);
    auto win = static_cast<Window> (xWindow);
    int screen = DefaultScreen (dpy);

    // Move to final position and release
    int rootX = originRootX + (x - originLocalX);
    int rootY = originRootY + (y - originLocalY);
    XTestFakeMotionEvent (dpy, screen, rootX, rootY, 0);
    XFlush (dpy);

    XTestFakeButtonEvent (dpy, 1, False, 0);
    XFlush (dpy);

    // Restore cursor to original position
    XTestFakeMotionEvent (dpy, screen, savedCursorX, savedCursorY, 0);
    XFlush (dpy);

    // Move editor window back off-screen and restore opacity
    if (movedWindow)
    {
        XMoveWindow (dpy, win, savedWindowX, savedWindowY);

        Atom opacityAtom = XInternAtom (dpy, "_NET_WM_WINDOW_OPACITY", False);
        XDeleteProperty (dpy, win, opacityAtom);

        XFlush (dpy);
        movedWindow = false;
    }

    active = false;
    xDisplay = nullptr;
    xWindow = 0;
}

bool X11SyntheticMouseDrag::isActive() const
{
    return active;
}

} // namespace dc

#endif // __linux__
