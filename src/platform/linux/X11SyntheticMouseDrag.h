#pragma once

#if defined(__linux__)

#include "plugins/SyntheticMouseDrag.h"

namespace dc
{

/**
 * Linux implementation of SyntheticMouseDrag.
 *
 * Uses XSendEvent (not XTest) to deliver mouse events to the plugin window's
 * event queue without moving the physical cursor. XSendEvent works regardless
 * of window position (no need to move editor on-screen for XWayland).
 *
 * Wine and the host app both process XSendEvent-delivered mouse events — neither
 * filters the send_event flag.
 */
class X11SyntheticMouseDrag : public SyntheticMouseDrag
{
public:
    X11SyntheticMouseDrag() = default;

    bool beginDrag (PluginEditorBridge& bridge, int x, int y) override;
    void moveDrag (int x, int y) override;
    void endDrag (int x, int y) override;
    bool isActive() const override;

private:
    void* xDisplay = nullptr;
    unsigned long xWindow = 0;       // Host wrapper window (native coords reference)
    bool active = false;

    // Root-relative coordinates at drag origin (for delta computation)
    int originRootX = 0, originRootY = 0;
    int originLocalX = 0, originLocalY = 0;
    // Saved cursor position to restore after endDrag
    int savedCursorX = 0, savedCursorY = 0;
    // Whether we moved the window on-screen for XTest
    bool movedWindow = false;
    int savedWindowX = 0, savedWindowY = 0;
};

} // namespace dc

#endif // __linux__
