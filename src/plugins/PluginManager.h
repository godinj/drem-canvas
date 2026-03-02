#pragma once
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

    /// Returns the scanned plugin list as dc::PluginDescription
    const std::vector<dc::PluginDescription>& getKnownPlugins() const;

    /// Access the VST3Host directly
    dc::VST3Host& getVST3Host();

    // Persistence
    void savePluginList (const std::filesystem::path& file) const;
    void loadPluginList (const std::filesystem::path& file);

    std::filesystem::path getDefaultPluginListFile() const;

private:
    dc::VST3Host vst3Host_;

    PluginManager (const PluginManager&) = delete;
    PluginManager& operator= (const PluginManager&) = delete;
};

} // namespace dc
