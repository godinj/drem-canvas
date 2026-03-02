#pragma once

#include <memory>

namespace dc
{

class PluginEditorBridge;

/**
 * Abstract interface for forwarding real mouse events to a composited plugin editor.
 *
 * When a plugin editor is rendered offscreen via compositor (XComposite), its pixels
 * are captured and drawn as an image in the PluginViewWidget. This class bridges the
 * gap by forwarding mouse events from the composited image area back to the plugin
 * window in native editor coordinates.
 *
 * Unlike SyntheticMouseDrag (which injects XTest events and moves the physical cursor),
 * this uses XSendEvent to deliver events directly to the plugin window's event queue
 * without disturbing the pointer.
 */
class MouseEventForwarder
{
public:
    virtual ~MouseEventForwarder() = default;

    /** Bind to a plugin editor bridge. Must be called before forwarding events.
        Returns true if forwarding is possible on this platform. */
    virtual bool bind (PluginEditorBridge& bridge) = 0;

    /** Unbind from the current bridge. */
    virtual void unbind() = 0;

    /** Returns true if currently bound to a bridge. */
    virtual bool isBound() const = 0;

    /** Forward a mouse button press at (nativeX, nativeY) in editor coordinates.
        button: 1=left, 2=middle, 3=right */
    virtual void sendMouseDown (int nativeX, int nativeY, int button) = 0;

    /** Forward a mouse button release.
        screenDeltaX/Y are the total drag delta in screen pixels from the mouseDown point. */
    virtual void sendMouseUp (int screenDeltaX, int screenDeltaY, int button) = 0;

    /** Forward a mouse drag delta in screen pixels (1:1 with user's physical drag).
        Use this during drag (button held) so the composited image scale doesn't
        amplify or dampen the movement. */
    virtual void sendDragDelta (int screenDeltaX, int screenDeltaY) = 0;

    /** Forward a mouse wheel event at (nativeX, nativeY) in editor coordinates. */
    virtual void sendMouseWheel (int nativeX, int nativeY, float deltaX, float deltaY) = 0;

    /** Factory: create the platform-appropriate implementation. */
    static std::unique_ptr<MouseEventForwarder> create();
};

} // namespace dc
