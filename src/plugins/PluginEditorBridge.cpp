#include "plugins/PluginEditorBridge.h"

#if defined(__linux__)
#include "platform/linux/X11PluginEditorBridge.h"
#elif defined(__APPLE__)
#include "platform/MacPluginEditorBridge.h"
#endif

namespace dc
{

std::unique_ptr<PluginEditorBridge> PluginEditorBridge::create (void* nativeWindowHandle)
{
#if defined(__linux__)
    return std::make_unique<X11PluginEditorBridge> (nativeWindowHandle);
#elif defined(__APPLE__)
    return std::make_unique<MacPluginEditorBridge> (nativeWindowHandle);
#else
    (void) nativeWindowHandle;
    return nullptr;
#endif
}

} // namespace dc
