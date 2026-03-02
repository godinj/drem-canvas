#include "PluginHost.h"
#include "model/Project.h"
#include "dc/foundation/base64.h"

namespace dc
{

PluginHost::PluginHost (PluginManager& manager)
    : pluginManager (manager)
{
}

void PluginHost::createPluginAsync (const dc::PluginDescription& desc,
                                    double sampleRate, int blockSize,
                                    PluginCallback callback)
{
    pluginManager.getVST3Host().createInstance (
        desc, sampleRate, blockSize,
        [cb = std::move (callback)] (std::unique_ptr<dc::PluginInstance> instance, std::string error)
        {
            cb (std::move (instance), error);
        });
}

std::unique_ptr<dc::PluginInstance> PluginHost::createPluginSync (
    const dc::PluginDescription& desc,
    double sampleRate, int blockSize)
{
    return pluginManager.getVST3Host().createInstanceSync (desc, sampleRate, blockSize);
}

std::string PluginHost::savePluginState (dc::PluginInstance& plugin)
{
    auto state = plugin.getState();
    return dc::base64Encode (state);
}

void PluginHost::restorePluginState (dc::PluginInstance& plugin,
                                     const std::string& base64State)
{
    auto bytes = dc::base64Decode (base64State);

    if (! bytes.empty())
        plugin.setState (bytes);
}

dc::PluginDescription PluginHost::descriptionFromPropertyTree (const PropertyTree& pluginNode)
{
    dc::PluginDescription desc;
    desc.name         = pluginNode.getProperty (IDs::pluginName).getStringOr ("");
    desc.manufacturer = pluginNode.getProperty (IDs::pluginManufacturer).getStringOr ("");
    desc.uid          = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
    desc.path         = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
    return desc;
}

} // namespace dc
