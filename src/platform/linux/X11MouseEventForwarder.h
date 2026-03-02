#pragma once

#if defined(__linux__)

#include "plugins/MouseEventForwarder.h"

namespace dc
{

/**
 * Linux implementation of MouseEventForwarder.
 *
 * Uses XTest to inject mouse events into the plugin window. The plugin window
 * is temporarily moved on-screen at (0,0) with 0 opacity during click+drag
 * interactions. XTest generates server-level events that Wine/yabridge process
 * correctly (unlike XSendEvent, whose send_event flag may be filtered).
 *
 * Only click+drag interactions are forwarded (mouseDown → mouseDrag → mouseUp).
 * Hover-only motion is skipped because XTest warps the physical cursor, which
 * would disrupt GLFW's mouse tracking.
 */
class X11MouseEventForwarder : public MouseEventForwarder
{
public:
    X11MouseEventForwarder() = default;

    bool bind (PluginEditorBridge& bridge) override;
    void unbind() override;
    bool isBound() const override;

    void sendMouseDown (int nativeX, int nativeY, int button) override;
    void sendMouseUp (int screenDeltaX, int screenDeltaY, int button) override;
    void sendDragDelta (int screenDeltaX, int screenDeltaY) override;
    void sendMouseWheel (int nativeX, int nativeY, float deltaX, float deltaY) override;

private:
    void* xDisplay = nullptr;
    unsigned long xWindow = 0;

    // Window on-screen state
    bool windowOnScreen = false;
    int savedWindowX = 0, savedWindowY = 0;

    // Cursor save/restore
    int savedCursorX = 0, savedCursorY = 0;

    // Drag origin in root coordinates (screen-pixel deltas applied to this)
    int originRootX = 0, originRootY = 0;
    bool buttonPressed = false;

    // Logical points → X11 root pixels (from GLFW content scale)
    float contentScale = 1.0f;

    void bringWindowOnScreen();
    void restoreWindow();
    void saveCursor();
    void restoreCursor();
    bool nativeToRoot (int nativeX, int nativeY, int& rootX, int& rootY);
};

} // namespace dc

#endif // __linux__
