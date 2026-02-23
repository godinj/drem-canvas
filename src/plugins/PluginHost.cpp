#include "PluginHost.h"
#include "model/Project.h"

namespace dc
{

PluginHost::PluginHost (PluginManager& manager)
    : pluginManager (manager)
{
}

void PluginHost::createPluginAsync (const juce::PluginDescription& desc,
                                    double sampleRate, int blockSize,
                                    PluginCallback callback)
{
    pluginManager.getFormatManager().createPluginInstanceAsync (
        desc, sampleRate, blockSize,
        [cb = std::move (callback)] (std::unique_ptr<juce::AudioPluginInstance> instance,
                                     const juce::String& error)
        {
            cb (std::move (instance), error);
        });
}

juce::String PluginHost::savePluginState (juce::AudioPluginInstance& plugin)
{
    juce::MemoryBlock stateData;
    plugin.getStateInformation (stateData);
    return stateData.toBase64Encoding();
}

void PluginHost::restorePluginState (juce::AudioPluginInstance& plugin,
                                     const juce::String& base64State)
{
    juce::MemoryBlock stateData;

    if (stateData.fromBase64Encoding (base64State))
    {
        plugin.setStateInformation (stateData.getData(),
                                    static_cast<int> (stateData.getSize()));
    }
}

juce::PluginDescription PluginHost::descriptionFromValueTree (const juce::ValueTree& pluginNode)
{
    juce::PluginDescription desc;
    desc.name                = pluginNode.getProperty (IDs::pluginName, juce::String()).toString();
    desc.pluginFormatName    = pluginNode.getProperty (IDs::pluginFormat, juce::String()).toString();
    desc.manufacturerName    = pluginNode.getProperty (IDs::pluginManufacturer, juce::String()).toString();
    desc.uniqueId            = pluginNode.getProperty (IDs::pluginUniqueId, 0);
    desc.fileOrIdentifier    = pluginNode.getProperty (IDs::pluginFileOrIdentifier, juce::String()).toString();
    return desc;
}

} // namespace dc
