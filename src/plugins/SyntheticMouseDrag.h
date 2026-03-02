#pragma once

#include <memory>

namespace dc
{

class PluginEditorBridge;

/**
 * Abstract interface for injecting synthetic mouse drag events into a plugin editor.
 *
 * Used for vim parameter adjustment (h/l/H/L keys) to send mouse drag events
 * at a parameter's spatial centroid, so plugin UIs animate their knobs/sliders.
 *
 * Unlike SyntheticInputProbe (which uses XTest and moves the cursor),
 * this uses XSendEvent to deliver events to the window's event queue
 * without touching the physical pointer.
 *
 * A drag session spans multiple keystrokes:
 *   beginDrag() on first h/l press, moveDrag() on subsequent presses,
 *   endDrag() when the parameter changes or the view closes.
 */
class SyntheticMouseDrag
{
public:
    virtual ~SyntheticMouseDrag() = default;

    /** Start a new drag session at (x, y) in native editor coordinates.
        Sends ButtonPress + initial MotionNotify.
        Returns true if drag was started successfully. */
    virtual bool beginDrag (PluginEditorBridge& bridge, int x, int y) = 0;

    /** Continue an active drag session by moving to (x, y).
        Sends MotionNotify with Button1 held. */
    virtual void moveDrag (int x, int y) = 0;

    /** End the drag session at (x, y).
        Sends ButtonRelease. */
    virtual void endDrag (int x, int y) = 0;

    /** Returns true if a drag session is currently active. */
    virtual bool isActive() const = 0;

    /** Factory: create the platform-appropriate implementation. */
    static std::unique_ptr<SyntheticMouseDrag> create();
};

} // namespace dc
