#pragma once

#if defined(__linux__)

#include <memory>
#include "include/core/SkImage.h"
#include "include/core/SkRefCnt.h"

namespace dc
{
namespace platform
{
namespace x11
{

/**
 * Captures a redirected X11 window's pixels via XComposite and converts
 * them to an SkImage for Skia compositing.
 *
 * Uses XCompositeRedirectWindow(Manual) so the X server renders the window
 * to an offscreen buffer. The window stays in the X11 tree so input events
 * route naturally — no event forwarding needed.
 *
 * This header uses pimpl to keep all X11 types out of the public API.
 */
class Compositor
{
public:
    Compositor();
    ~Compositor();

    /** Begin composite redirect on the given X11 window.
        Returns true if XComposite + XDamage are available and redirect succeeded. */
    bool startRedirect (void* display, unsigned long window);

    /** Stop redirecting and release all X11 resources. */
    void stopRedirect();

    /** Returns true if composite redirect is currently active. */
    bool isActive() const;

    /** Check for XDamage events (non-blocking). Returns true if the
        window has been redrawn since the last capture. */
    bool hasDamage();

    /** Capture the redirected window pixels as an SkImage.
        Returns a cached image if no damage since the last capture. */
    sk_sp<SkImage> capture();

    /** Re-acquire the offscreen pixmap after a window resize. */
    void handleResize();

    int getWidth() const;
    int getHeight() const;

    /** Move the redirected window off-screen so it's not visible as an
        overlay (needed on XWayland where CompositeRedirectManual doesn't
        prevent the Wayland compositor from showing the surface). */
    void hideWindow();

    /** Move the redirected window back to (x, y). */
    void showWindow (int x, int y);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
