#include "plugins/SyntheticMouseDrag.h"

#if defined(__linux__)
#include "platform/linux/X11SyntheticMouseDrag.h"
#elif defined(__APPLE__)
#include "platform/MacSyntheticMouseDrag.h"
#endif

namespace dc
{

std::unique_ptr<SyntheticMouseDrag> SyntheticMouseDrag::create()
{
#if defined(__linux__)
    return std::make_unique<X11SyntheticMouseDrag>();
#elif defined(__APPLE__)
    return std::make_unique<MacSyntheticMouseDrag>();
#else
    return nullptr;
#endif
}

} // namespace dc
