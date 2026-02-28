#if defined(__linux__)

// This file deliberately does NOT include JuceHeader.h.
// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named JUCE classes.

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>

#include "X11MouseProbe.h"
#include <thread>
#include <chrono>

namespace dc
{
namespace platform
{
namespace x11
{

void sendMouseProbe (void* displayPtr, unsigned long windowId, int x, int y,
                     ProbeMode mode)
{
    auto* dpy = static_cast<Display*> (displayPtr);
    auto win = static_cast<Window> (windowId);
    if (dpy == nullptr || win == 0)
        return;

    // Check XTest extension
    int xtestEvent, xtestError, xtestMajor, xtestMinor;
    if (! XTestQueryExtension (dpy, &xtestEvent, &xtestError,
                               &xtestMajor, &xtestMinor))
        return;

    // Convert window-relative coords to root-relative for XTest
    Window child;
    int rootX = 0, rootY = 0;
    XTranslateCoordinates (dpy, win, DefaultRootWindow (dpy),
                           x, y, &rootX, &rootY, &child);

    // Move cursor to target position
    XTestFakeMotionEvent (dpy, DefaultScreen (dpy), rootX, rootY, 0);
    XFlush (dpy);
    std::this_thread::sleep_for (std::chrono::milliseconds (5));

    // Press left button
    XTestFakeButtonEvent (dpy, 1, True, 0);
    XFlush (dpy);

    if (mode != ProbeMode::click)
    {
        // Compute drag target
        int dragX = rootX;
        int dragY = rootY;
        switch (mode)
        {
            case ProbeMode::dragUp:    dragY -= 10; break;
            case ProbeMode::dragRight: dragX += 10; break;
            case ProbeMode::dragDown:  dragY += 10; break;
            default: break;
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (10));
        XTestFakeMotionEvent (dpy, DefaultScreen (dpy), dragX, dragY, 0);
        XFlush (dpy);

        // Wait for plugin to process drag
        std::this_thread::sleep_for (std::chrono::milliseconds (30));
    }
    else
    {
        // Click: brief hold then release
        std::this_thread::sleep_for (std::chrono::milliseconds (20));
    }

    // Release
    XTestFakeButtonEvent (dpy, 1, False, 0);
    XFlush (dpy);
}

void moveWindow (void* displayPtr, unsigned long windowId, int x, int y)
{
    auto* dpy = static_cast<Display*> (displayPtr);
    auto win = static_cast<Window> (windowId);
    if (dpy == nullptr || win == 0)
        return;

    XMoveWindow (dpy, win, x, y);
    XFlush (dpy);
}

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
