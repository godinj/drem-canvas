#pragma once

#if defined(__APPLE__)

#include "plugins/SyntheticMouseDrag.h"

namespace dc
{

/**
 * macOS stub implementation of SyntheticMouseDrag.
 * CGEventPost-based drag injection is planned for a future release.
 */
class MacSyntheticMouseDrag : public SyntheticMouseDrag
{
public:
    bool beginDrag (PluginEditorBridge& bridge, int x, int y) override;
    void moveDrag (int x, int y) override;
    void endDrag (int x, int y) override;
    bool isActive() const override;
};

} // namespace dc

#endif // __APPLE__
