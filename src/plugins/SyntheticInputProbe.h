#pragma once

#include <memory>

namespace dc
{

class PluginEditorBridge;

/** Interaction mode for synthetic mouse probing. */
enum class ProbeMode
{
    dragUp,        // Press + drag up (vertical knobs)
    dragDown,      // Press + drag down (inverted knobs)
    dragRight,     // Press + drag right (horizontal sliders)
    click          // Press + release (buttons/toggles)
};

/**
 * Abstract interface for injecting synthetic mouse input into a plugin editor.
 *
 * Used by Phase 4 parameter discovery to probe unmapped parameters by
 * simulating mouse interactions at their spatial locations.
 */
class SyntheticInputProbe
{
public:
    virtual ~SyntheticInputProbe() = default;

    /** Prepare for probing: move editor on-screen, etc.
        Returns true if probing is possible on this platform. */
    virtual bool beginProbing (PluginEditorBridge& bridge) = 0;

    /** End probing: move editor back off-screen, etc. */
    virtual void endProbing (PluginEditorBridge& bridge) = 0;

    /** Inject a synthetic mouse probe at (x, y) in native editor coordinates. */
    virtual void sendProbe (int x, int y, ProbeMode mode) = 0;

    /** Factory: create the platform-appropriate probe implementation. */
    static std::unique_ptr<SyntheticInputProbe> create();
};

} // namespace dc
