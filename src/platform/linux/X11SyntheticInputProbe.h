#pragma once

#if defined(__linux__)

#include "plugins/SyntheticInputProbe.h"

namespace dc
{

/**
 * Linux implementation of SyntheticInputProbe.
 * Wraps x11::sendMouseProbe and x11::moveWindow for Phase 4 parameter probing.
 */
class X11SyntheticInputProbe : public SyntheticInputProbe
{
public:
    X11SyntheticInputProbe() = default;

    bool beginProbing (PluginEditorBridge& bridge) override;
    void endProbing (PluginEditorBridge& bridge) override;
    void sendProbe (int x, int y, ProbeMode mode) override;

private:
    void* xDisplay = nullptr;
    unsigned long xWindow = 0;
};

} // namespace dc

#endif // __linux__
