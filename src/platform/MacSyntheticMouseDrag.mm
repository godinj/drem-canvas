#include "MacSyntheticMouseDrag.h"

#if defined(__APPLE__)

namespace dc
{

bool MacSyntheticMouseDrag::beginDrag (PluginEditorBridge& /*bridge*/, int /*x*/, int /*y*/)
{
    // CGEventPost-based drag injection not yet implemented
    return false;
}

void MacSyntheticMouseDrag::moveDrag (int /*x*/, int /*y*/)
{
}

void MacSyntheticMouseDrag::endDrag (int /*x*/, int /*y*/)
{
}

bool MacSyntheticMouseDrag::isActive() const
{
    return false;
}

} // namespace dc

#endif // __APPLE__
