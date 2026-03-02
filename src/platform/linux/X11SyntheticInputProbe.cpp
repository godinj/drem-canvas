#include "X11SyntheticInputProbe.h"

#if defined(__linux__)

#include "platform/linux/X11PluginEditorBridge.h"
#include "platform/linux/X11MouseProbe.h"
#include <thread>
#include <chrono>

namespace dc
{

bool X11SyntheticInputProbe::beginProbing (PluginEditorBridge& bridge)
{
    auto* x11Bridge = dynamic_cast<X11PluginEditorBridge*> (&bridge);
    if (x11Bridge == nullptr)
        return false;

    xDisplay = x11Bridge->getXDisplay();
    xWindow = x11Bridge->getXWindow();

    if (xDisplay == nullptr || xWindow == 0)
        return false;

    // Make the window fully transparent before moving on-screen.
    // XTest needs positive root coords, so we must place the window at (0,0),
    // but on XWayland the Wayland compositor still renders visible surfaces
    // regardless of XComposite redirect. Setting opacity to 0 hides it.
    platform::x11::setWindowOpacity (xDisplay, xWindow, 0.0f);
    platform::x11::moveWindow (xDisplay, xWindow, 0, 0);
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    return true;
}

void X11SyntheticInputProbe::endProbing (PluginEditorBridge& /*bridge*/)
{
    if (xDisplay != nullptr && xWindow != 0)
    {
        // Move back off-screen, then restore opacity
        platform::x11::moveWindow (xDisplay, xWindow, -10000, -10000);
        platform::x11::setWindowOpacity (xDisplay, xWindow, -1.0f);
    }

    xDisplay = nullptr;
    xWindow = 0;
}

void X11SyntheticInputProbe::sendProbe (int x, int y, ProbeMode mode)
{
    if (xDisplay == nullptr || xWindow == 0)
        return;

    platform::x11::ProbeMode x11Mode;
    switch (mode)
    {
        case ProbeMode::dragUp:    x11Mode = platform::x11::ProbeMode::dragUp; break;
        case ProbeMode::dragDown:  x11Mode = platform::x11::ProbeMode::dragDown; break;
        case ProbeMode::dragRight: x11Mode = platform::x11::ProbeMode::dragRight; break;
        case ProbeMode::click:     x11Mode = platform::x11::ProbeMode::click; break;
    }

    platform::x11::sendMouseProbe (xDisplay, xWindow, x, y, x11Mode);
}

} // namespace dc

#endif // __linux__
