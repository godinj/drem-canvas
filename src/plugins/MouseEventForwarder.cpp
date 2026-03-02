#include "plugins/MouseEventForwarder.h"

#if defined(__linux__)
#include "platform/linux/X11MouseEventForwarder.h"
#endif

namespace dc
{

std::unique_ptr<MouseEventForwarder> MouseEventForwarder::create()
{
#if defined(__linux__)
    return std::make_unique<X11MouseEventForwarder>();
#else
    return nullptr;
#endif
}

} // namespace dc
