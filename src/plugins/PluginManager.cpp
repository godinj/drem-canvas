#include "PluginManager.h"
#include "dc/foundation/file_utils.h"

namespace dc
{

PluginManager::PluginManager()
{
    // Initialize JUCE format manager for backward compat (TODO: Phase 4 Agent 06 — remove)
    juce::addDefaultFormatsToManager (formatManager_);
}

void PluginManager::scanForPlugins()
{
    vst3Host_.scanPlugins();
    syncJucePluginList();
    savePluginList (getDefaultPluginListFile());
}

const std::vector<dc::PluginDescription>& PluginManager::getKnownPluginsDc() const
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
    syncJucePluginList();
}

std::filesystem::path PluginManager::getDefaultPluginListFile() const
{
    return dc::getUserAppDataDirectory() / "pluginList.yaml";
}

void PluginManager::syncJucePluginList()
{
    // TODO: Phase 4 Agent 06 — remove this entire method once callers migrated.
    // Populate JUCE KnownPluginList from the dc:: database for backward compat.
    juceKnownPlugins_.clear();

    for (const auto& dcDesc : vst3Host_.getKnownPlugins())
    {
        juce::PluginDescription juceDesc;
        juceDesc.name              = dcDesc.name;
        juceDesc.pluginFormatName  = "VST3";
        juceDesc.manufacturerName  = dcDesc.manufacturer;
        juceDesc.category          = dcDesc.category;
        juceDesc.version           = dcDesc.version;
        juceDesc.fileOrIdentifier  = dcDesc.path.string();
        juceDesc.numInputChannels  = dcDesc.numInputChannels;
        juceDesc.numOutputChannels = dcDesc.numOutputChannels;
        juceDesc.hasSharedContainer = false;
        juceDesc.isInstrument      = dcDesc.acceptsMidi;
        juceDesc.uniqueId          = 0; // hex UID doesn't map to int directly

        juceKnownPlugins_.addType (juceDesc);
    }
}

} // namespace dc
