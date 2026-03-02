#include "PluginHost.h"
#include "model/Project.h"
#include "dc/foundation/base64.h"

namespace dc
{

PluginHost::PluginHost (PluginManager& manager)
    : pluginManager (manager)
{
}

// --- New dc:: API ---

void PluginHost::createPluginAsyncDc (const dc::PluginDescription& desc,
                                       double sampleRate, int blockSize,
                                       DcPluginCallback callback)
{
    pluginManager.getVST3Host().createInstance (
        desc, sampleRate, blockSize,
        [cb = std::move (callback)] (std::unique_ptr<dc::PluginInstance> instance, std::string error)
        {
            cb (std::move (instance), error);
        });
}

std::unique_ptr<dc::PluginInstance> PluginHost::createPluginSyncDc (
    const dc::PluginDescription& desc,
    double sampleRate, int blockSize)
{
    return pluginManager.getVST3Host().createInstanceSync (desc, sampleRate, blockSize);
}

std::string PluginHost::savePluginStateDc (dc::PluginInstance& plugin)
{
    auto state = plugin.getState();
    return dc::base64Encode (state);
}

void PluginHost::restorePluginStateDc (dc::PluginInstance& plugin,
                                        const std::string& base64State)
{
    auto bytes = dc::base64Decode (base64State);

    if (! bytes.empty())
        plugin.setState (bytes);
}

dc::PluginDescription PluginHost::descriptionFromPropertyTreeDc (const PropertyTree& pluginNode)
{
    dc::PluginDescription desc;
    desc.name         = pluginNode.getProperty (IDs::pluginName).getStringOr ("");
    desc.manufacturer = pluginNode.getProperty (IDs::pluginManufacturer).getStringOr ("");
    desc.uid          = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
    desc.path         = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
    return desc;
}

// --- Backward-compatible JUCE API (TODO: Phase 4 Agent 06 — remove) ---

void PluginHost::createPluginAsync (const juce::PluginDescription& desc,
                                    double sampleRate, int blockSize,
                                    PluginCallback callback)
{
    pluginManager.getFormatManager().createPluginInstanceAsync (
        desc, sampleRate, blockSize,
        [cb = std::move (callback)] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            cb (std::move (instance), error.toStdString());
        });
}

std::unique_ptr<juce::AudioPluginInstance> PluginHost::createPluginSync (
    juce::AudioPluginFormatManager& formatManager,
    const juce::PluginDescription& desc,
    double sampleRate, int blockSize)
{
    juce::String error; // JUCE API boundary — removed in Phase 4 Agent 06
    return formatManager.createPluginInstance (desc, sampleRate, blockSize, error);
}

std::string PluginHost::savePluginState (juce::AudioPluginInstance& plugin)
{
    juce::MemoryBlock stateData;
    plugin.getStateInformation (stateData);

    std::vector<uint8_t> bytes (static_cast<const uint8_t*> (stateData.getData()),
                                static_cast<const uint8_t*> (stateData.getData()) + stateData.getSize());
    return dc::base64Encode (bytes);
}

void PluginHost::restorePluginState (juce::AudioPluginInstance& plugin,
                                     const std::string& base64State)
{
    auto bytes = dc::base64Decode (base64State);

    if (! bytes.empty())
    {
        plugin.setStateInformation (bytes.data(),
                                    static_cast<int> (bytes.size()));
    }
}

juce::PluginDescription PluginHost::descriptionFromPropertyTree (const PropertyTree& pluginNode)
{
    juce::PluginDescription desc;
    desc.name                = pluginNode.getProperty (IDs::pluginName).getStringOr ("");
    desc.pluginFormatName    = pluginNode.getProperty (IDs::pluginFormat).getStringOr ("");
    desc.manufacturerName    = pluginNode.getProperty (IDs::pluginManufacturer).getStringOr ("");
    desc.uniqueId            = static_cast<int> (pluginNode.getProperty (IDs::pluginUniqueId).getIntOr (0));
    desc.fileOrIdentifier    = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
    return desc;
}

} // namespace dc
