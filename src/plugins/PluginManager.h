#pragma once
#include <JuceHeader.h>
#include <filesystem>

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
    void savePluginList (const std::filesystem::path& file) const;
    void loadPluginList (const std::filesystem::path& file);

    std::filesystem::path getDefaultPluginListFile() const;

private:
    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;

    PluginManager (const PluginManager&) = delete;
    PluginManager& operator= (const PluginManager&) = delete;
};

} // namespace dc
