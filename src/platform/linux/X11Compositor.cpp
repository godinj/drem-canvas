#if defined(__linux__)

// This file deliberately does NOT include JuceHeader.h.
// X11 defines Font, Time, Drawable, Bool, Status as typedefs/macros
// which collide with identically-named JUCE classes.

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XTest.h>

#include "X11Compositor.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkColorType.h"

#include <iostream>
#include <cstring>
#include <chrono>

namespace dc
{
namespace platform
{
namespace x11
{

struct Compositor::Impl
{
    Display* display = nullptr;
    Window window = 0;
    Pixmap pixmap = 0;
    Damage damage = 0;

    int damageEventBase = 0;
    int damageErrorBase = 0;

    sk_sp<SkImage> cachedImage;
    int width = 0;
    int height = 0;
    bool damaged = true;   // Start dirty so first capture always works
    bool active = false;
    bool nudged = false;   // Whether we've sent wake-up events

    // After redirect starts, plugins (especially Wine/yabridge) may take
    // several frames to render into the offscreen buffer.  During this
    // warmup window we force every capture to do a fresh XGetImage instead
    // of returning the (likely blank) cached frame.
    std::chrono::steady_clock::time_point redirectStartTime;
    static constexpr auto warmupDuration = std::chrono::seconds (3);

    bool inWarmup() const
    {
        return active
            && (std::chrono::steady_clock::now() - redirectStartTime) < warmupDuration;
    }

    // Wake Wine's Win32 message loop so it processes the pending WM_PAINT.
    //
    // WM_PAINT is the lowest-priority Win32 message — it's only generated
    // when GetMessage/PeekMessage runs and finds no other messages. Wine's
    // GetMessage blocks waiting for new X11 events, so QS_PAINT alone
    // won't wake it. We need to inject an event that sets QS_MOUSE or
    // QS_POSTMESSAGE to break the block.
    //
    // Strategy:
    //   1. XTestFakeMotionEvent — generates real server-level MotionNotify
    //      that Wine processes as WM_MOUSEMOVE, waking the message loop.
    //      Works on XWayland (via libei), unlike XWarpPointer.
    //   2. Synthetic ConfigureNotify — Wine maps these to
    //      WM_WINE_WINDOW_STATE_CHANGED (QS_POSTMESSAGE), which also
    //      wakes the loop. Sent to child windows as a fallback.
    void nudgePlugin()
    {
        // --- Approach 1: XTest fake motion ---
        int xtestEvent, xtestError, xtestMajor, xtestMinor;
        if (XTestQueryExtension (display, &xtestEvent, &xtestError,
                                 &xtestMajor, &xtestMinor))
        {
            // Convert plugin window center to root-relative coordinates
            Window child;
            int rootX = 0, rootY = 0;
            XTranslateCoordinates (display, window, DefaultRootWindow (display),
                                   width / 2, height / 2,
                                   &rootX, &rootY, &child);

            XTestFakeMotionEvent (display, DefaultScreen (display),
                                  rootX, rootY, 0);
            XFlush (display);
        }

        // --- Approach 2: synthetic ConfigureNotify to child windows ---
        // Wine doesn't check send_event for ConfigureNotify — it maps to
        // QS_POSTMESSAGE which wakes a blocked GetMessage.
        Window root, par;
        Window* children = nullptr;
        unsigned int nChildren = 0;

        if (XQueryTree (display, window, &root, &par, &children, &nChildren))
        {
            for (unsigned int i = 0; i < nChildren; ++i)
            {
                XWindowAttributes ca;
                if (! XGetWindowAttributes (display, children[i], &ca))
                    continue;

                XEvent ev;
                std::memset (&ev, 0, sizeof (ev));
                ev.type = ConfigureNotify;
                ev.xconfigure.event = children[i];
                ev.xconfigure.window = children[i];
                ev.xconfigure.x = ca.x;
                ev.xconfigure.y = ca.y;
                ev.xconfigure.width = ca.width;
                ev.xconfigure.height = ca.height;
                ev.xconfigure.border_width = 0;
                ev.xconfigure.above = None;
                ev.xconfigure.override_redirect = False;
                XSendEvent (display, children[i], False,
                            StructureNotifyMask | SubstructureNotifyMask, &ev);
            }
            if (children)
                XFree (children);
        }

        XFlush (display);
    }

    bool acquirePixmap()
    {
        if (pixmap != 0)
        {
            XFreePixmap (display, pixmap);
            pixmap = 0;
        }

        XWindowAttributes attrs;
        if (! XGetWindowAttributes (display, window, &attrs))
        {
            std::cerr << "[Compositor] XGetWindowAttributes failed\n";
            return false;
        }

        width = attrs.width;
        height = attrs.height;

        if (width <= 0 || height <= 0)
        {
            std::cerr << "[Compositor] window has zero size\n";
            return false;
        }

        pixmap = XCompositeNameWindowPixmap (display, window);
        if (pixmap == 0)
        {
            std::cerr << "[Compositor] XCompositeNameWindowPixmap failed\n";
            return false;
        }

        damaged = true;
        return true;
    }
};

Compositor::Compositor()
    : impl (std::make_unique<Impl>())
{
}

Compositor::~Compositor()
{
    stopRedirect();
}

bool Compositor::startRedirect (void* display, unsigned long window)
{
    if (impl->active)
        stopRedirect();

    auto* dpy = static_cast<Display*> (display);
    if (dpy == nullptr || window == 0)
        return false;

    // Check XComposite extension >= 0.2
    int compositeEventBase, compositeErrorBase;
    if (! XCompositeQueryExtension (dpy, &compositeEventBase, &compositeErrorBase))
    {
        std::cerr << "[Compositor] XComposite extension not available\n";
        return false;
    }

    int major = 0, minor = 0;
    XCompositeQueryVersion (dpy, &major, &minor);
    if (major < 1 && minor < 2)
    {
        std::cerr << "[Compositor] XComposite version " << major << "." << minor
                  << " too old (need >= 0.2)\n";
        return false;
    }

    // Check XDamage extension
    if (! XDamageQueryExtension (dpy, &impl->damageEventBase, &impl->damageErrorBase))
    {
        std::cerr << "[Compositor] XDamage extension not available\n";
        return false;
    }

    impl->display = dpy;
    impl->window = static_cast<Window> (window);

    // Redirect the window to an offscreen buffer
    XCompositeRedirectWindow (dpy, impl->window, CompositeRedirectManual);

    // Create damage tracking
    impl->damage = XDamageCreate (dpy, impl->window, XDamageReportNonEmpty);

    // Acquire the offscreen pixmap
    if (! impl->acquirePixmap())
    {
        // Cleanup on failure
        if (impl->damage != 0)
        {
            XDamageDestroy (dpy, impl->damage);
            impl->damage = 0;
        }
        XCompositeUnredirectWindow (dpy, impl->window, CompositeRedirectManual);
        impl->display = nullptr;
        impl->window = 0;
        return false;
    }

    impl->active = true;
    impl->nudged = false;
    impl->redirectStartTime = std::chrono::steady_clock::now();

    std::cerr << "[Compositor] started redirect for window 0x" << std::hex << window
              << std::dec << " (" << impl->width << "x" << impl->height << ")\n";
    return true;
}

void Compositor::stopRedirect()
{
    if (! impl->active)
        return;

    if (impl->damage != 0)
    {
        XDamageDestroy (impl->display, impl->damage);
        impl->damage = 0;
    }

    if (impl->pixmap != 0)
    {
        XFreePixmap (impl->display, impl->pixmap);
        impl->pixmap = 0;
    }

    XCompositeUnredirectWindow (impl->display, impl->window, CompositeRedirectManual);
    XFlush (impl->display);

    impl->cachedImage.reset();
    impl->active = false;
    impl->display = nullptr;
    impl->window = 0;
    impl->width = 0;
    impl->height = 0;

    std::cerr << "[Compositor] stopped\n";
}

bool Compositor::isActive() const
{
    return impl->active;
}

bool Compositor::hasDamage()
{
    if (! impl->active)
        return false;

    XEvent event;
    while (XCheckTypedEvent (impl->display, impl->damageEventBase + XDamageNotify, &event))
    {
        impl->damaged = true;
        XDamageSubtract (impl->display, impl->damage, None, None);
    }

    // During the warmup window, always recapture — the plugin may not have
    // rendered its first frame yet, and XDamage might not fire from Wine.
    if (impl->inWarmup())
    {
        impl->damaged = true;

        // After 500ms, nudge the plugin via XTest + ConfigureNotify to
        // wake Wine's Win32 message loop and trigger the initial WM_PAINT.
        if (! impl->nudged)
        {
            auto elapsed = std::chrono::steady_clock::now() - impl->redirectStartTime;
            if (elapsed > std::chrono::milliseconds (500))
            {
                impl->nudged = true;
                impl->nudgePlugin();
            }
        }
    }

    return impl->damaged;
}

sk_sp<SkImage> Compositor::capture()
{
    if (! impl->active)
        return nullptr;

    if (! impl->damaged && impl->cachedImage)
        return impl->cachedImage;

    if (impl->pixmap == 0 || impl->width <= 0 || impl->height <= 0)
        return impl->cachedImage;

    XImage* ximage = XGetImage (impl->display, impl->pixmap,
                                0, 0,
                                static_cast<unsigned int> (impl->width),
                                static_cast<unsigned int> (impl->height),
                                AllPlanes, ZPixmap);

    if (ximage == nullptr)
    {
        std::cerr << "[Compositor] XGetImage failed\n";
        return impl->cachedImage;
    }

    size_t rowBytes = static_cast<size_t> (ximage->bytes_per_line);

    auto imageInfo = SkImageInfo::Make (impl->width, impl->height,
                                        kBGRA_8888_SkColorType,
                                        kOpaque_SkAlphaType);

    SkBitmap bitmap;
    bitmap.allocPixels (imageInfo);
    for (int y = 0; y < impl->height; ++y)
    {
        std::memcpy (reinterpret_cast<char*> (bitmap.getAddr32 (0, y)),
                     ximage->data + y * rowBytes,
                     static_cast<size_t> (impl->width) * 4);
    }
    bitmap.notifyPixelsChanged();
    impl->cachedImage = bitmap.asImage();

    XDestroyImage (ximage);
    impl->damaged = false;

    return impl->cachedImage;
}

void Compositor::handleResize()
{
    if (! impl->active)
        return;

    impl->acquirePixmap();
}

int Compositor::getWidth() const
{
    return impl->width;
}

int Compositor::getHeight() const
{
    return impl->height;
}

void Compositor::hideWindow()
{
    if (! impl->active)
        return;

    // Move the window far off-screen without changing its size.
    // This keeps it mapped (plugin continues rendering, XComposite keeps
    // capturing) but the Wayland compositor won't display it.
    //
    // NOTE: on XWayland this also prevents the plugin from rendering — the
    // Wayland compositor skips offscreen surfaces. Use only on native X11.
    XMoveWindow (impl->display, impl->window, -10000, -10000);
    XFlush (impl->display);
    std::cerr << "[Compositor] window hidden off-screen\n";
}

void Compositor::showWindow (int x, int y)
{
    if (! impl->active)
        return;

    XMoveWindow (impl->display, impl->window, x, y);
    XFlush (impl->display);
}

} // namespace x11
} // namespace platform
} // namespace dc

#endif // __linux__
