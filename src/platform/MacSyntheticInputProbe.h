#pragma once

#if defined(__APPLE__)

#include "plugins/SyntheticInputProbe.h"

namespace dc
{

/**
 * macOS stub implementation of SyntheticInputProbe.
 * CGEvent-based probing is planned for a future release.
 */
class MacSyntheticInputProbe : public SyntheticInputProbe
{
public:
    bool beginProbing (PluginEditorBridge& bridge) override;
    void endProbing (PluginEditorBridge& bridge) override;
    void sendProbe (int x, int y, ProbeMode mode) override;
};

} // namespace dc

#endif // __APPLE__
