#pragma once
#include <JuceHeader.h>
#include "PluginManager.h"

namespace dc
{

class PluginHost
{
public:
    explicit PluginHost (PluginManager& manager);

    // Async plugin creation
    using PluginCallback = std::function<void (std::unique_ptr<juce::AudioPluginInstance>, const juce::String& error)>;
    void createPluginAsync (const juce::PluginDescription& desc,
                            double sampleRate, int blockSize,
                            PluginCallback callback);

    // State save/restore as base64
    static juce::String savePluginState (juce::AudioPluginInstance& plugin);
    static void restorePluginState (juce::AudioPluginInstance& plugin, const juce::String& base64State);

    // Reconstruct PluginDescription from a PLUGIN ValueTree node
    static juce::PluginDescription descriptionFromValueTree (const juce::ValueTree& pluginNode);

private:
    PluginManager& pluginManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginHost)
};

} // namespace dc
