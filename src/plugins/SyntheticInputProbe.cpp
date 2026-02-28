#include "plugins/SyntheticInputProbe.h"

#if defined(__linux__)
#include "platform/linux/X11SyntheticInputProbe.h"
#elif defined(__APPLE__)
#include "platform/MacSyntheticInputProbe.h"
#endif

namespace dc
{

std::unique_ptr<SyntheticInputProbe> SyntheticInputProbe::create()
{
#if defined(__linux__)
    return std::make_unique<X11SyntheticInputProbe>();
#elif defined(__APPLE__)
    return std::make_unique<MacSyntheticInputProbe>();
#else
    return nullptr;
#endif
}

} // namespace dc
