#pragma once
#include <JuceHeader.h>

namespace dc
{

class PluginManager
{
public:
    PluginManager();

    void scanForPlugins();
    void scanDefaultPaths();

    const juce::KnownPluginList& getKnownPlugins() const { return knownPlugins; }
    juce::KnownPluginList& getKnownPlugins()             { return knownPlugins; }

    juce::AudioPluginFormatManager& getFormatManager() { return formatManager; }

    // Persistence
    void savePluginList (const juce::File& file) const;
    void loadPluginList (const juce::File& file);

    juce::File getDefaultPluginListFile() const;

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginManager)
};

} // namespace dc
