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

    // Move the editor on-screen so XTest root coords are positive.
    // In Wayland mode the editor lives at (-10000,-10000) for compositor
    // capture — XTest events at negative root coords never reach the
    // plugin window on XWayland.
    platform::x11::moveWindow (xDisplay, xWindow, 0, 0);
    std::this_thread::sleep_for (std::chrono::milliseconds (50));

    return true;
}

void X11SyntheticInputProbe::endProbing (PluginEditorBridge& /*bridge*/)
{
    // Move editor back off-screen for compositor capture
    if (xDisplay != nullptr && xWindow != 0)
        platform::x11::moveWindow (xDisplay, xWindow, -10000, -10000);

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
