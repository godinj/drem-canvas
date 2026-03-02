#pragma once
#include <JuceHeader.h>
#include "PluginManager.h"
#include "dc/model/PropertyTree.h"
#include <string>

namespace dc
{

class PluginHost
{
public:
    explicit PluginHost (PluginManager& manager);

    // Async plugin creation
    using PluginCallback = std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const std::string& error)>;
    void createPluginAsync (const juce::PluginDescription& desc,
                            double sampleRate, int blockSize,
                            PluginCallback callback);

    // Synchronous plugin creation (wraps JUCE error output parameter)
    static std::unique_ptr<juce::AudioPluginInstance> createPluginSync (
        juce::AudioPluginFormatManager& formatManager,
        const juce::PluginDescription& desc,
        double sampleRate, int blockSize);

    // State save/restore as base64
    static std::string savePluginState (juce::AudioPluginInstance& plugin);
    static void restorePluginState (juce::AudioPluginInstance& plugin, const std::string& base64State);

    // Reconstruct PluginDescription from a PLUGIN PropertyTree node
    static juce::PluginDescription descriptionFromPropertyTree (const PropertyTree& pluginNode);

private:
    PluginManager& pluginManager;

    PluginHost (const PluginHost&) = delete;
    PluginHost& operator= (const PluginHost&) = delete;
};

} // namespace dc
