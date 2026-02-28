#include "MacSyntheticInputProbe.h"

#if defined(__APPLE__)

namespace dc
{

bool MacSyntheticInputProbe::beginProbing (PluginEditorBridge& /*bridge*/)
{
    // CGEvent-based probing not yet implemented
    return false;
}

void MacSyntheticInputProbe::endProbing (PluginEditorBridge& /*bridge*/)
{
}

void MacSyntheticInputProbe::sendProbe (int /*x*/, int /*y*/, ProbeMode /*mode*/)
{
}

} // namespace dc

#endif // __APPLE__
