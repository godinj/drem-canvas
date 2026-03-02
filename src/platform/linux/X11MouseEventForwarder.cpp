// This file deliberately does NOT include JuceHeader.h.
// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named JUCE classes.

#if defined(__linux__)

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XTest.h>

#include "X11MouseEventForwarder.h"
#include "X11PluginEditorBridge.h"

#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>

namespace dc
{

bool X11MouseEventForwarder::bind (PluginEditorBridge& bridge)
{
    auto* x11Bridge = dynamic_cast<X11PluginEditorBridge*> (&bridge);
    if (x11Bridge == nullptr)
        return false;

    xDisplay = x11Bridge->getXDisplay();
    xWindow = x11Bridge->getXWindow();

    if (xDisplay == nullptr || xWindow == 0)
        return false;

    // Verify XTest is available
    auto* dpy = static_cast<Display*> (xDisplay);
    int xtestEvent, xtestError, xtestMajor, xtestMinor;
    if (! XTestQueryExtension (dpy, &xtestEvent, &xtestError,
                                &xtestMajor, &xtestMinor))
    {
        xDisplay = nullptr;
        xWindow = 0;
        return false;
    }

    // Cache content scale (logical points → X11 root pixels)
    contentScale = bridge.getContentScale();

    return true;
}

void X11MouseEventForwarder::unbind()
{
    if (windowOnScreen)
        restoreWindow();

    xDisplay = nullptr;
    xWindow = 0;
}

bool X11MouseEventForwarder::isBound() const
{
    return xDisplay != nullptr && xWindow != 0;
}

// ─── Window management ──────────────────────────────────────────

void X11MouseEventForwarder::bringWindowOnScreen()
{
    if (windowOnScreen || xDisplay == nullptr || xWindow == 0)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    auto win = static_cast<Window> (xWindow);

    // Check if window is off-screen
    XWindowAttributes attrs;
    XGetWindowAttributes (dpy, win, &attrs);

    if (attrs.x < -1000 || attrs.y < -1000)
    {
        savedWindowX = attrs.x;
        savedWindowY = attrs.y;

        // Set fully transparent so the window is invisible
        Atom opacityAtom = XInternAtom (dpy, "_NET_WM_WINDOW_OPACITY", False);
        unsigned long zero = 0;
        XChangeProperty (dpy, win, opacityAtom, XA_CARDINAL, 32,
                         PropModeReplace,
                         reinterpret_cast<unsigned char*> (&zero), 1);

        XMoveWindow (dpy, win, 0, 0);
        XFlush (dpy);

        // Brief pause for the window manager to process the move
        std::this_thread::sleep_for (std::chrono::milliseconds (30));

        windowOnScreen = true;
    }
}

void X11MouseEventForwarder::restoreWindow()
{
    if (! windowOnScreen || xDisplay == nullptr || xWindow == 0)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    auto win = static_cast<Window> (xWindow);

    // Move back off-screen
    XMoveWindow (dpy, win, savedWindowX, savedWindowY);

    // Remove opacity override to restore default
    Atom opacityAtom = XInternAtom (dpy, "_NET_WM_WINDOW_OPACITY", False);
    XDeleteProperty (dpy, win, opacityAtom);

    XFlush (dpy);
    windowOnScreen = false;
}

void X11MouseEventForwarder::saveCursor()
{
    if (xDisplay == nullptr)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    Window rootRet, childRet;
    int winX, winY;
    unsigned int maskRet;
    XQueryPointer (dpy, DefaultRootWindow (dpy), &rootRet, &childRet,
                   &savedCursorX, &savedCursorY, &winX, &winY, &maskRet);
}

void X11MouseEventForwarder::restoreCursor()
{
    if (xDisplay == nullptr)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    int screen = DefaultScreen (dpy);
    XTestFakeMotionEvent (dpy, screen, savedCursorX, savedCursorY, 0);
    XFlush (dpy);
}

bool X11MouseEventForwarder::nativeToRoot (int nativeX, int nativeY,
                                            int& rootX, int& rootY)
{
    if (xDisplay == nullptr || xWindow == 0)
        return false;

    auto* dpy = static_cast<Display*> (xDisplay);
    auto win = static_cast<Window> (xWindow);

    Window child;
    XTranslateCoordinates (dpy, win, DefaultRootWindow (dpy),
                            nativeX, nativeY, &rootX, &rootY, &child);
    return true;
}

// ─── Public API ─────────────────────────────────────────────────

void X11MouseEventForwarder::sendMouseDown (int nativeX, int nativeY, int button)
{
    if (xDisplay == nullptr || xWindow == 0)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    int screen = DefaultScreen (dpy);

    // Move window on-screen for valid root coordinates
    bringWindowOnScreen();

    // Save cursor so we can restore it on mouseUp
    if (! buttonPressed)
        saveCursor();

    // Convert native coords to root-relative
    int rootX, rootY;
    if (! nativeToRoot (nativeX, nativeY, rootX, rootY))
        return;

    // Cache root origin — screen-pixel deltas are applied to this
    originRootX = rootX;
    originRootY = rootY;

    // Warp cursor and press
    XTestFakeMotionEvent (dpy, screen, rootX, rootY, 0);
    XFlush (dpy);

    unsigned int xButton = static_cast<unsigned int> (button);
    XTestFakeButtonEvent (dpy, xButton, True, 0);
    XFlush (dpy);

    buttonPressed = true;
}

void X11MouseEventForwarder::sendMouseUp (int screenDeltaX, int screenDeltaY, int button)
{
    if (xDisplay == nullptr || xWindow == 0 || ! buttonPressed)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    int screen = DefaultScreen (dpy);

    // Scale widget-space delta (logical points) to X11 root pixels
    int rootX = originRootX + static_cast<int> (static_cast<float> (screenDeltaX) * contentScale);
    int rootY = originRootY + static_cast<int> (static_cast<float> (screenDeltaY) * contentScale);

    // Move to final position and release
    XTestFakeMotionEvent (dpy, screen, rootX, rootY, 0);
    XFlush (dpy);

    unsigned int xButton = static_cast<unsigned int> (button);
    XTestFakeButtonEvent (dpy, xButton, False, 0);
    XFlush (dpy);

    buttonPressed = false;

    // Restore cursor to where the user's physical cursor was
    restoreCursor();

    // Move window back off-screen
    restoreWindow();
}

void X11MouseEventForwarder::sendDragDelta (int screenDeltaX, int screenDeltaY)
{
    if (xDisplay == nullptr || xWindow == 0 || ! buttonPressed)
        return;

    auto* dpy = static_cast<Display*> (xDisplay);
    int screen = DefaultScreen (dpy);

    // Scale widget-space delta (logical points) to X11 root pixels.
    // On HiDPI displays, 10 logical points = 10 * contentScale root pixels.
    int rootX = originRootX + static_cast<int> (static_cast<float> (screenDeltaX) * contentScale);
    int rootY = originRootY + static_cast<int> (static_cast<float> (screenDeltaY) * contentScale);

    XTestFakeMotionEvent (dpy, screen, rootX, rootY, 0);
    XFlush (dpy);
}

void X11MouseEventForwarder::sendMouseWheel (int nativeX, int nativeY,
                                              float /*deltaX*/, float /*deltaY*/)
{
    // Wheel forwarding via XTest would require warping the cursor,
    // which disrupts the user. Skip for now — users can scroll via
    // vim parameter adjustment (h/l keys) instead.
    (void) nativeX;
    (void) nativeY;
}

} // namespace dc

#endif // __linux__
