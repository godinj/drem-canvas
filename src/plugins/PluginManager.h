#pragma once
#include <JuceHeader.h>  // TODO: Phase 4 Agent 06 — remove once all callers migrated from juce::PluginDescription
#include "dc/plugins/VST3Host.h"
#include "dc/plugins/PluginDescription.h"
#include <filesystem>
#include <vector>

namespace dc
{

class PluginManager
{
public:
    PluginManager();

    void scanForPlugins();

    /// New dc:: API: returns the scanned plugin list as dc::PluginDescription
    const std::vector<dc::PluginDescription>& getKnownPluginsDc() const;

    /// New dc:: API: access the VST3Host directly
    dc::VST3Host& getVST3Host();

    // ----- Backward-compatible JUCE API (TODO: Phase 4 Agent 06 — remove) -----

    /// Returns a juce::KnownPluginList for callers still using JUCE types.
    /// Populated from the dc:: database on scan/load.
    const juce::KnownPluginList& getKnownPlugins() const { return juceKnownPlugins_; }
    juce::KnownPluginList& getKnownPlugins()             { return juceKnownPlugins_; }

    /// Returns the JUCE format manager (still needed by callers using JUCE plugin APIs).
    /// TODO: Phase 4 Agent 06 — remove when all callers migrated.
    juce::AudioPluginFormatManager& getFormatManager() { return formatManager_; }

    // Persistence
    void savePluginList (const std::filesystem::path& file) const;
    void loadPluginList (const std::filesystem::path& file);

    std::filesystem::path getDefaultPluginListFile() const;

private:
    dc::VST3Host vst3Host_;

    // Backward-compatibility shims (TODO: Phase 4 Agent 06 — remove)
    juce::AudioPluginFormatManager formatManager_;
    juce::KnownPluginList juceKnownPlugins_;

    /// Sync the dc:: database into the JUCE KnownPluginList for backward compat
    void syncJucePluginList();

    PluginManager (const PluginManager&) = delete;
    PluginManager& operator= (const PluginManager&) = delete;
};

} // namespace dc
