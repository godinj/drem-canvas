#include "PluginManager.h"
#include "dc/foundation/file_utils.h"

namespace dc
{

PluginManager::PluginManager()
{
}

void PluginManager::scanForPlugins()
{
    vst3Host_.scanPlugins();
    savePluginList (getDefaultPluginListFile());
}

const std::vector<dc::PluginDescription>& PluginManager::getKnownPlugins() const
{
    return vst3Host_.getKnownPlugins();
}

dc::VST3Host& PluginManager::getVST3Host()
{
    return vst3Host_;
}

void PluginManager::savePluginList (const std::filesystem::path& file) const
{
    vst3Host_.saveDatabase (file);
}

void PluginManager::loadPluginList (const std::filesystem::path& file)
{
    vst3Host_.loadDatabase (file);
}

std::filesystem::path PluginManager::getDefaultPluginListFile() const
{
    return dc::getUserAppDataDirectory() / "pluginList.yaml";
}

} // namespace dc
